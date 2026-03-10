/**
 * @file lua_trials_loader.cpp
 * @brief Runtime loader for trial definitions from Lua scripts.
 *
 * Executes sf3_3rd_trial_clean.lua in the RmlUi Lua state, then walks
 * the comboTest/kadai/difficulty/gaugeMaxflg tables to build TrialDef
 * structs dynamically. This replaces the build-time lua_trial_parser.py
 * pipeline.
 */

#include "lua_trials_loader.h"

#include <RmlUi/Lua/Interpreter.h>
#include <SDL3/SDL.h>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
#include <lualib.h>
}

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
// Forward-declare instead of #include <windows.h> to avoid typedef clashes
// with MSYS2/MinGW BSD type headers that SDL already pulls in.
extern "C" __declspec(dllimport) unsigned long __stdcall GetModuleFileNameA(void* hModule, char* lpFilename,
                                                                            unsigned long nSize);
#endif

// Character IDs mapping Lua index (1-19) -> engine My_char value
// Must match lua_trial_parser.py CHARA_IDS
static const s16 CHARA_IDS[] = {
    -1, // 0 unused
    1,  // 1  Alex
    2,  // 2  Ryu
    11, // 3  Ken
    14, // 4  Gouki
    12, // 5  Sean
    7,  // 6  Ibuki
    15, // 7  Chun-Li
    8,  // 8  Elena
    16, // 9  Makoto
    3,  // 10 Yun
    10, // 11 Yang
    9,  // 12 Oro
    4,  // 13 Dudley
    13, // 14 Urien
    19, // 15 Remy
    6,  // 16 Hugo
    5,  // 17 Necro
    18, // 18 Twelve
    17, // 19 Q
};

static const char* CHARA_NAMES[] = { NULL,      "Alex",  "Ryu",    "Ken",   "Gouki",  "Sean", "Ibuki",
                                     "Chun-Li", "Elena", "Makoto", "Yun",   "Yang",   "Oro",  "Dudley",
                                     "Urien",   "Remy",  "Hugo",   "Necro", "Twelve", "Q" };

// ---- Dynamic storage ----
struct DynTrialStep {
    TrialRequirementType type;
    s16 waza_ids[MAX_WAZA_ALTERNATIVES];
    std::string display_name;
    std::string kadai_input;
};

struct DynTrialDef {
    s16 chara_id;
    s16 difficulty;
    s16 gauge_max;
    std::vector<DynTrialStep> steps;
};

// Global storage for runtime-loaded trials
static std::map<int, std::vector<DynTrialDef>> s_dyn_trials; // chara_lua_idx -> trials
static std::vector<TrialDef> s_flat_defs;
static std::vector<const TrialDef*> s_flat_def_ptrs;
static std::vector<TrialCharacterDef> s_char_defs;
static bool s_loaded = false;

// ---- Waza ID parser (mirrors lua_trial_parser.py parse_waza_id) ----

static void parse_waza_values(const char* s, s16* out, int max_out, int* count) {
    *count = 0;
    if (!s || strlen(s) < 2)
        return;

    const char* hex = s + 1; // skip prefix letter
    size_t len = strlen(hex);

    if (len == 8) {
        // Two 4-hex-digit values
        char buf[5] = { 0 };
        memcpy(buf, hex, 4);
        s16 v1 = (s16)strtol(buf, NULL, 16);
        memcpy(buf, hex + 4, 4);
        s16 v2 = (s16)strtol(buf, NULL, 16);
        out[(*count)++] = v1;
        if (v2 != v1 && *count < max_out)
            out[(*count)++] = v2;
    } else if (len >= 4) {
        char buf[5] = { 0 };
        memcpy(buf, hex, 4);
        out[(*count)++] = (s16)strtol(buf, NULL, 16);
    }
}

// ---- Lua table walkers ----

/**
 * Read comboTest[chara][trial] from the Lua state.
 * Expects the Lua state has already executed the trial script.
 */
static bool read_combo_tests(lua_State* L) {
    lua_getglobal(L, "comboTest");
    if (!lua_istable(L, -1)) {
        SDL_Log("[Lua Trials] comboTest not found or not a table");
        lua_pop(L, 1);
        return false;
    }

    // Iterate comboTest[chara]
    lua_pushnil(L);
    while (lua_next(L, -2)) {
        int chara = (int)lua_tointeger(L, -2);
        if (!lua_istable(L, -1) || chara < 1 || chara > 19) {
            lua_pop(L, 1);
            continue;
        }

        // Iterate comboTest[chara][trial]
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            // int trial_num = (int)lua_tointeger(L, -2); // unused, iteration order gives index
            if (!lua_istable(L, -1)) {
                lua_pop(L, 1);
                continue;
            }

            DynTrialDef def;
            def.chara_id = CHARA_IDS[chara];
            def.difficulty = 1;
            def.gauge_max = 0;

            // Iterate steps in comboTest[chara][trial]
            int step_count = (int)luaL_len(L, -1);
            for (int si = 1; si <= step_count && si <= MAX_TRIAL_STEPS; si++) {
                lua_rawgeti(L, -1, si);
                if (!lua_istable(L, -1)) {
                    lua_pop(L, 1);
                    continue;
                }

                DynTrialStep step;
                step.type = TRIAL_REQ_ATTACK_HIT;
                for (int w = 0; w < MAX_WAZA_ALTERNATIVES; w++)
                    step.waza_ids[w] = (s16)0xFFFF;

                // Field 1: display name
                lua_rawgeti(L, -1, 1);
                if (lua_isstring(L, -1))
                    step.display_name = lua_tostring(L, -1);
                lua_pop(L, 1);

                // Field 2: type string (H, HR, T, F, D, J, K, U, ANIM2P)
                lua_rawgeti(L, -1, 2);
                if (lua_isstring(L, -1)) {
                    const char* ts = lua_tostring(L, -1);
                    if (strcmp(ts, "T") == 0)
                        step.type = TRIAL_REQ_THROW_HIT;
                    else if (strcmp(ts, "F") == 0)
                        step.type = TRIAL_REQ_FIREBALL_HIT;
                    else if (strcmp(ts, "D") == 0 || strcmp(ts, "J") == 0 || strcmp(ts, "K") == 0 ||
                             strcmp(ts, "K12K") == 0)
                        step.type = TRIAL_REQ_ACTIVE_MOVE;
                    else if (strcmp(ts, "U") == 0)
                        step.type = TRIAL_REQ_SPECIAL_COND;
                    else if (strcmp(ts, "ANIM2P") == 0)
                        step.type = TRIAL_REQ_ANIMATION;
                    // H, HR -> ATTACK_HIT (default)
                }
                lua_pop(L, 1);

                // Fields 3+: waza ID strings like "A00460034"
                int waza_count = 0;
                int field_count = (int)luaL_len(L, -1);
                for (int fi = 3; fi <= field_count && waza_count < MAX_WAZA_ALTERNATIVES; fi++) {
                    lua_rawgeti(L, -1, fi);
                    if (lua_isstring(L, -1)) {
                        const char* ws = lua_tostring(L, -1);
                        // Skip timer values (pure numbers)
                        if (ws[0] >= 'A' && ws[0] <= 'Z') {
                            s16 vals[MAX_WAZA_ALTERNATIVES];
                            int vc = 0;
                            parse_waza_values(ws, vals, MAX_WAZA_ALTERNATIVES, &vc);
                            for (int vi = 0; vi < vc && waza_count < MAX_WAZA_ALTERNATIVES; vi++) {
                                // Deduplicate
                                bool dup = false;
                                for (int di = 0; di < waza_count; di++) {
                                    if (step.waza_ids[di] == vals[vi]) {
                                        dup = true;
                                        break;
                                    }
                                }
                                if (!dup)
                                    step.waza_ids[waza_count++] = vals[vi];
                            }
                        }
                    } else if (lua_istable(L, -1)) {
                        // Sub-array of hex ints (ANIM2P style: {0x422A, 0x42E9})
                        int sub_len = (int)luaL_len(L, -1);
                        for (int subi = 1; subi <= sub_len && waza_count < MAX_WAZA_ALTERNATIVES; subi++) {
                            lua_rawgeti(L, -1, subi);
                            if (lua_isinteger(L, -1)) {
                                step.waza_ids[waza_count++] = (s16)lua_tointeger(L, -1);
                            }
                            lua_pop(L, 1);
                        }
                    }
                    lua_pop(L, 1);
                }

                def.steps.push_back(step);
                lua_pop(L, 1); // pop step table
            }

            if (!def.steps.empty()) {
                s_dyn_trials[chara].push_back(std::move(def));
            }
            lua_pop(L, 1); // pop trial table
        }
        lua_pop(L, 1); // pop chara table
    }
    lua_pop(L, 1); // pop comboTest

    return true;
}

/** Read kadai[chara][trial] and attach to existing dyn trials. */
static void read_kadai(lua_State* L) {
    lua_getglobal(L, "kadai");
    if (!lua_istable(L, -1)) {
        lua_pop(L, 1);
        return;
    }

    lua_pushnil(L);
    while (lua_next(L, -2)) {
        int chara = (int)lua_tointeger(L, -2);
        if (!lua_istable(L, -1) || chara < 1 || chara > 19) {
            lua_pop(L, 1);
            continue;
        }

        auto it = s_dyn_trials.find(chara);
        if (it == s_dyn_trials.end()) {
            lua_pop(L, 1);
            continue;
        }

        // Build a quick trial-number index for this character
        // The trials are stored in order they were parsed
        int trial_idx = 0;
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            // int trial_num = (int)lua_tointeger(L, -2);
            if (!lua_istable(L, -1) || trial_idx >= (int)it->second.size()) {
                lua_pop(L, 1);
                trial_idx++;
                continue;
            }

            DynTrialDef& def = it->second[trial_idx];
            int step_count = (int)luaL_len(L, -1);
            for (int si = 1; si <= step_count && si - 1 < (int)def.steps.size(); si++) {
                lua_rawgeti(L, -1, si);
                if (lua_istable(L, -1)) {
                    // Concatenate all quoted strings in this kadai entry
                    std::string kadai_str;
                    int field_count = (int)luaL_len(L, -1);
                    for (int fi = 1; fi <= field_count; fi++) {
                        lua_rawgeti(L, -1, fi);
                        if (lua_isstring(L, -1)) {
                            if (!kadai_str.empty())
                                kadai_str += " ";
                            kadai_str += lua_tostring(L, -1);
                        }
                        lua_pop(L, 1);
                    }
                    def.steps[si - 1].kadai_input = kadai_str;
                }
                lua_pop(L, 1);
            }

            lua_pop(L, 1);
            trial_idx++;
        }
        lua_pop(L, 1);
    }
    lua_pop(L, 1);
}

/** Read difficulty[chara][trial] and gaugeMaxflg[chara][trial]. */
static void read_metadata(lua_State* L) {
    // Difficulty
    lua_getglobal(L, "difficulty");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            int chara = (int)lua_tointeger(L, -2);
            if (lua_istable(L, -1) && chara >= 1 && chara <= 19) {
                auto it = s_dyn_trials.find(chara);
                if (it != s_dyn_trials.end()) {
                    int ti = 0;
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (lua_isinteger(L, -1) && ti < (int)it->second.size()) {
                            it->second[ti].difficulty = (s16)lua_tointeger(L, -1);
                        }
                        lua_pop(L, 1);
                        ti++;
                    }
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);

    // gaugeMaxflg
    lua_getglobal(L, "gaugeMaxflg");
    if (lua_istable(L, -1)) {
        lua_pushnil(L);
        while (lua_next(L, -2)) {
            int chara = (int)lua_tointeger(L, -2);
            if (lua_istable(L, -1) && chara >= 1 && chara <= 19) {
                auto it = s_dyn_trials.find(chara);
                if (it != s_dyn_trials.end()) {
                    int ti = 0;
                    lua_pushnil(L);
                    while (lua_next(L, -2)) {
                        if (lua_isinteger(L, -1) && ti < (int)it->second.size()) {
                            it->second[ti].gauge_max = (s16)lua_tointeger(L, -1);
                        }
                        lua_pop(L, 1);
                        ti++;
                    }
                }
            }
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
}

/** Flatten the dynamic data into C-compatible flat arrays. */
static void flatten_to_c_structs() {
    s_flat_defs.clear();
    s_flat_def_ptrs.clear();
    s_char_defs.clear();

    // Pre-count totals so we can reserve() and avoid reallocation.
    // Without this, push_back can invalidate the raw pointers stored
    // in s_flat_def_ptrs (into s_flat_defs) and s_char_defs (into s_flat_def_ptrs).
    size_t total_trials = 0;
    size_t total_chars = 0;
    for (auto& pair : s_dyn_trials) {
        if (!pair.second.empty()) {
            total_trials += pair.second.size();
            total_chars++;
        }
    }
    s_flat_defs.reserve(total_trials);
    s_flat_def_ptrs.reserve(total_trials);
    s_char_defs.reserve(total_chars);

    for (auto& pair : s_dyn_trials) {
        int chara_lua_idx = pair.first;
        auto& trials = pair.second;
        if (trials.empty())
            continue;

        int first_def_idx = (int)s_flat_defs.size();

        for (auto& dt : trials) {
            TrialDef td;
            td.chara_id = dt.chara_id;
            td.difficulty = dt.difficulty;
            td.gauge_max = dt.gauge_max;
            td.num_steps = (s16)dt.steps.size();
            if (td.num_steps > MAX_TRIAL_STEPS)
                td.num_steps = MAX_TRIAL_STEPS;

            memset(td.steps, 0, sizeof(td.steps));
            for (int i = 0; i < td.num_steps; i++) {
                td.steps[i].type = dt.steps[i].type;
                memcpy(td.steps[i].waza_ids, dt.steps[i].waza_ids, sizeof(td.steps[i].waza_ids));
                // Strings: use strdup so they persist (leaked on purpose - lives for app lifetime)
                td.steps[i].display_name = strdup(dt.steps[i].display_name.c_str());
                td.steps[i].kadai_input = strdup(dt.steps[i].kadai_input.c_str());
            }
            s_flat_defs.push_back(td);
        }

        // Build pointer array for this character
        int ptr_start = (int)s_flat_def_ptrs.size();
        for (int i = first_def_idx; i < (int)s_flat_defs.size(); i++) {
            s_flat_def_ptrs.push_back(&s_flat_defs[i]);
        }

        TrialCharacterDef cd;
        cd.chara_id = CHARA_IDS[chara_lua_idx];
        cd.num_trials = (s16)(s_flat_def_ptrs.size() - ptr_start);
        cd.trials = &s_flat_def_ptrs[ptr_start];
        cd.chara_name = CHARA_NAMES[chara_lua_idx];
        s_char_defs.push_back(cd);
    }
}

// ---- Public API ----

// Helper: get the exe directory path with trailing separator.
// Tries SDL_GetBasePath() first, falls back to platform API if empty.
static std::string get_exe_dir() {
    const char* base = SDL_GetBasePath();
    if (base && base[0])
        return std::string(base);

#ifdef _WIN32
    // SDL_GetBasePath() returns empty on some MSYS2/MinGW builds.
    // Fall back to Win32 GetModuleFileNameA.
    char buf[512];
    unsigned long len = GetModuleFileNameA(NULL, buf, sizeof(buf));
    if (len > 0 && len < sizeof(buf)) {
        // Truncate to directory (find last separator)
        for (unsigned long i = len; i > 0; i--) {
            if (buf[i - 1] == '\\' || buf[i - 1] == '/') {
                buf[i] = '\0';
                return std::string(buf);
            }
        }
    }
#endif

    return std::string(); // empty = fall back to relative path (CWD)
}

static bool s_load_failed = false; // true after first failed attempt — don't retry

bool lua_trials_load(const char* lua_path) {
    lua_State* L = Rml::Lua::Interpreter::GetLuaState();
    if (!L) {
        SDL_Log("[Lua Trials] No Lua state available");
        return false;
    }

    SDL_Log("[Lua Trials] Loading %s...", lua_path);

    // The Lua file expects joypad.get() to exist (called at line 96)
    // and gd.createFromPng() for image loading (line 2200+, FBNeo GUI only).
    // Provide stubs so the file loads without error.
    Rml::Lua::Interpreter::DoString("joypad = joypad or {}\n"
                                    "joypad.get = joypad.get or function() return {} end\n"
                                    "do\n"
                                    "  local img = { gdStr = function() return '' end }\n"
                                    "  local mt = { __index = img }\n"
                                    "  gd = { createFromPng = function() return setmetatable({}, mt) end }\n"
                                    "  package.preload['gd'] = function() return gd end\n"
                                    "end\n");

    // Resolve path relative to exe directory (user may run from any CWD)
    std::string exe_dir = get_exe_dir();
    std::string abs_path = exe_dir.empty() ? lua_path : (exe_dir + lua_path);
    SDL_Log("[Lua Trials] Resolved path: %s", abs_path.c_str());

    if (luaL_dofile(L, abs_path.c_str()) != LUA_OK) {
        SDL_Log("[Lua Trials] ERROR loading: %s", lua_tostring(L, -1));
        lua_pop(L, 1);
        return false;
    }

    if (!read_combo_tests(L)) {
        SDL_Log("[Lua Trials] Failed to read comboTest table");
        return false;
    }

    read_kadai(L);
    read_metadata(L);
    flatten_to_c_structs();

    int total_trials = (int)s_flat_defs.size();
    int total_chars = (int)s_char_defs.size();
    SDL_Log("[Lua Trials] Loaded %d trials for %d characters", total_trials, total_chars);

    s_loaded = true;
    return true;
}

void lua_trials_free(void) {
    if (!s_loaded)
        return;

    // Free strdup'd strings
    for (auto& td : s_flat_defs) {
        for (int i = 0; i < td.num_steps; i++) {
            free((void*)td.steps[i].display_name);
            free((void*)td.steps[i].kadai_input);
        }
    }

    s_flat_defs.clear();
    s_flat_def_ptrs.clear();
    s_char_defs.clear();
    s_dyn_trials.clear();
    s_loaded = false;
    s_load_failed = false;
}

const TrialCharacterDef* lua_trials_get_characters(int* out_count) {
    // Lazy-load trial definitions on first access (deferred from boot)
    if (!s_loaded && !s_load_failed) {
        if (!lua_trials_load("lua/sf3_3rd_trial_clean.lua")) {
            SDL_Log("[Lua Trials] Lazy load failed, no trial data available");
            s_load_failed = true; // Don't retry every frame
            if (out_count)
                *out_count = 0;
            return NULL;
        }
    }
    if (out_count)
        *out_count = (int)s_char_defs.size();
    return s_char_defs.empty() ? NULL : s_char_defs.data();
}
