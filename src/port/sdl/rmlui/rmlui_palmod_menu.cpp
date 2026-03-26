/**
 * @file rmlui_palmod_menu.cpp
 * @brief RmlUi palette editor — character palette cycling, stage palette
 *        editing, and per-color RGB picker with live CPS3 ColorRAM updates.
 *
 * Data model name: "palmod"
 * Bindings:
 *   Characters tab:   p1_color, p2_color, p1_name, p2_name, p1_label, p2_label
 *   Stage tab:        stage_row, stage_row_max
 *   Color picker:     sel_index, sel_r, sel_g, sel_b, sel_preview, sel_original
 *   Mode:             edit_mode (0=chars, 1=stage), in_game, picker_active
 *   Actions:          pick_char_color(player, index), pick_stage_color(index),
 *                     reset_color, close_picker
 */
#include "port/sdl/rmlui/rmlui_palmod_menu.h"
#include "port/config/config.h"
#ifdef PALMOD_HAS_STORAGE
#include "port/sdl/rmlui/palmod_storage.h"
#endif
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/sdl/rmlui/palette_remix.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>
#include <cstdio>
#include <cstring>
#include <vector>

extern "C" {

#include "character_names.h"
#include "sf33rd/Source/Game/rendering/color3rd.h"

extern unsigned char My_char[2];
extern signed char Player_Color[2];
extern unsigned char Play_Game;

} /* extern "C" */

/* ── CPS3 Color Format Helpers ───────────────────────────────────── */

/* CPS3 format: 1BBBBBGGGGGRRRRR (MSB always set, 5 bits per channel) */
static inline int cps3_r(u16 c) { return (c >> 0) & 0x1F; }
static inline int cps3_g(u16 c) { return (c >> 5) & 0x1F; }
static inline int cps3_b(u16 c) { return (c >> 10) & 0x1F; }
static inline u16 cps3_pack(int r, int g, int b) {
    return (u16)(0x8000 | ((b & 0x1F) << 10) | ((g & 0x1F) << 5) | (r & 0x1F));
}

/* Convert 5-bit CPS3 channel to 8-bit for CSS display */
static inline int to8bit(int v5) { return (v5 << 3) | (v5 >> 2); }

static Rml::String color_to_css(u16 c) {
    char buf[32];
    snprintf(buf, sizeof(buf), "rgb(%d,%d,%d)", to8bit(cps3_r(c)), to8bit(cps3_g(c)), to8bit(cps3_b(c)));
    return Rml::String(buf);
}

/* ── Palette color labels ─────────────────────────────────────────── */
static const char* s_color_labels[] = {
    "LP", "MP", "HP", "LK", "MK", "HK",
    "LP+MK+HP", "Extra-1", "Extra-2", "Extra-3",
    "Extra-4", "Extra-5", "Extra-6", "Extra-7", "Extra-8", "Extra-9"
};
static const int s_color_label_count = sizeof(s_color_labels) / sizeof(s_color_labels[0]);

static const char* get_color_label(int index) {
    if (index >= 0 && index < s_color_label_count)
        return s_color_labels[index];
    return "???";
}

/* ── Per-character config keys ────────────────────────────────────── */

static void make_config_key(char* buf, size_t size, int player, int char_id) {
    snprintf(buf, size, "palmod-p%d-char%d", player + 1, char_id);
}

static int load_saved_color(int player, int char_id) {
    char key[64];
    make_config_key(key, sizeof(key), player, char_id);
    if (Config_HasKey(key)) {
        int v = Config_GetInt(key);
        if (v >= 0 && v <= 15) return v;
    }
    return -1;
}

static void save_color(int player, int char_id, int color) {
    char key[64];
    make_config_key(key, sizeof(key), player, char_id);
    Config_SetInt(key, color);
}

/* ── Persistent palette source cache ──────────────────────────────── */
/* The native engine frees plcol[id] memory after init_trans_color_ram().
 * We cache a persistent copy here so apply_palette() can safely read
 * palette source data at any time without touching freed memory. */
static COL s_plcol_cache[2];
static bool s_plcol_valid[2] = { false, false };

static void palmod_on_color_loaded(int id, const COL* data) {
    if (id >= 0 && id < 2 && data) {
        memcpy(&s_plcol_cache[id], data, sizeof(COL));
        s_plcol_valid[id] = true;
    }
}

/* ── Data model state ─────────────────────────────────────────────── */
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

/* Cached snapshot for dirty detection */
struct PalmodSnapshot {
    int p1_color, p2_color;
    int edit_mode;
    int stage_row;
    int sel_index, sel_r, sel_g, sel_b;
    bool in_game, picker_active;
    Rml::String p1_name, p2_name, p1_label, p2_label;
    Rml::String sel_preview, sel_original;
};
static PalmodSnapshot s_cache = {};

/* Per-player palette override (-1 = follow game default) */
static int s_palmod_color[2] = { -1, -1 };

/* Track last-known character to reload saved overrides on char change */
static int s_last_char[2] = { -1, -1 };

/* Config dirty flag (debounced save) */
static bool s_config_dirty = false;

/* Init guard: prevent setter-triggered palette writes during document load.
 * RmlUi initialises range sliders on load, calling their setters with
 * the initial value. This would corrupt ColorRAM if not guarded.
 * Set to true after the first full update cycle (NOT at init time). */
static bool s_init_complete = false;

/* Edit mode: 0 = Characters, 1 = Stage */
static int s_edit_mode = 0;

/* Stage palette row selector */
static int s_stage_row = 32;
static const int STAGE_ROW_MIN = 32;
static const int STAGE_ROW_MAX = 500;

/* Color picker state */
static bool s_picker_active = false;
static int s_picker_player = -1;  /* -1 = stage, 0 = P1, 1 = P2 */
static int s_picker_row = 0;      /* ColorRAM row being edited */
static int s_picker_index = 0;    /* Color index within row (0–63) */
static int s_picker_r = 0;
static int s_picker_g = 0;
static int s_picker_b = 0;
static u16 s_picker_original = 0; /* Original value for reset */

/* Save/Load state */
static char s_save_name[64] = "";
static Rml::String s_save_name_str;
#ifdef PALMOD_HAS_STORAGE
static PalmodEntry s_palette_list[PALMOD_MAX_PALETTES] = {};
#endif
static int s_palette_count = 0;
static Rml::String s_palette_list_str; /* display: "name1, name2, ..." */
static int s_selected_palette = 0;     /* index into s_palette_list */
static bool s_list_dirty = true;       /* refresh list on next update */

/* ── Remix Tab State ── */
static bool s_remix_target_p1 = true;
static bool s_remix_target_p2 = false;
static bool s_remix_target_stage = false;
static int s_remix_scope = 0;  // 0=Global, 1=Selected Bank, 2=Ramps, 3=Checked Banks
static bool s_remix_auto_preview = true;

/* Committed remix state: when Apply is clicked, we snapshot ColorRAM
 * so that switching targets doesn't clobber previous work.           */
static bool s_remix_committed[2] = { false, false };
static u16  s_committed_cram[2][16][64]; /* rows 0-15 for P1, 16-31 for P2 */
static bool s_stage_committed = false;
static u16  s_committed_stage[480][64]; /* rows 32-511 */

/* Original stage palette snapshot — captured on remix tab entry so
 * that Reset can restore stage palettes (there's no plcol equivalent). */
static bool s_stage_original_valid = false;
static u16  s_original_stage[480][64]; /* rows 32-511 */

static void cache_stage_original() {
    for (int r = STAGE_ROW_MIN; r <= STAGE_ROW_MAX && (r - STAGE_ROW_MIN) < 480; r++)
        memcpy(s_original_stage[r - STAGE_ROW_MIN], ColorRAM[r], sizeof(ColorRAM[0]));
    s_stage_original_valid = true;
}

static void restore_stage_original() {
    if (!s_stage_original_valid) return;
    for (int r = STAGE_ROW_MIN; r <= STAGE_ROW_MAX && (r - STAGE_ROW_MIN) < 480; r++)
        memcpy(ColorRAM[r], s_original_stage[r - STAGE_ROW_MIN], sizeof(ColorRAM[0]));
    palUpdateGhostCP3(STAGE_ROW_MIN, STAGE_ROW_MAX - STAGE_ROW_MIN + 1);
}

/* Check if a palette color-index is all-black (empty/unused Extra preset).
 * Checks both banks — Gill (My_char==0) uses bank 1 independently. */
static bool is_palette_empty(int id, int color_index) {
    if (!s_plcol_valid[id] || color_index < 0 || color_index > 15) return true;
    const COL* col = &s_plcol_cache[id];
    for (int bank = 0; bank < 2; bank++) {
        for (int i = 1; i < 64; i++) { /* skip index 0 (transparent) */
            if (col->col[bank][color_index][i] != 0) return false;
        }
    }
    return true;
}

static bool s_remix_l2 = true;
static bool s_remix_l0 = true;
static bool s_remix_l1 = true;

enum RemixFxType {
    FX_MOOD,   /* global color grading */
    FX_RAMP,   /* per-bank ramp manipulation */
    FX_LOOK,   /* layer look (apply_layer_look) */
};

struct RemixEffectState {
    const char* id;
    const char* name;
    RemixFxType type;
    bool checked;
    int value; // 0-100
};

static RemixEffectState s_remix_effects[] = {
    {"chk_noir",       "Noir",             FX_MOOD, false, 50},
    {"chk_miami",      "Miami",            FX_MOOD, false, 50},
    {"chk_grindhouse", "Grindhouse",       FX_MOOD, false, 50},
    {"chk_sunrise",    "Sunrise",          FX_MOOD, false, 50},
    {"chk_noon",       "Noon",             FX_MOOD, false, 50},
    {"chk_dusk",       "Dusk",             FX_MOOD, false, 50},
    {"chk_midnight",   "Midnight",         FX_MOOD, false, 50},
    {"chk_stormy",     "Stormy",           FX_MOOD, false, 50},
    {"chk_neon_night", "Neon-night",       FX_MOOD, false, 50},
    {"chk_cold_steel", "Cold Steel",       FX_MOOD, false, 50},
    {"chk_heatwave",   "Heatwave",         FX_MOOD, false, 50},
    {"chk_grad_curve", "Gradient Curve",   FX_RAMP, false, 50},
    {"chk_posterize",  "Posterize",        FX_RAMP, false, 50},
    {"chk_dither",     "Dither-Friendly",  FX_RAMP, false, 50},
    {"chk_edge",       "Edge Accent",      FX_RAMP, false, 50},
    {"chk_neonizer",   "Neonizer",         FX_RAMP, false, 50},
    {"chk_fg_pop",     "Foreground Pop",   FX_LOOK, false, 50},
    {"chk_bg_wash",    "Background Wash",  FX_LOOK, false, 50},
    {"chk_crt",        "CRT / Arcade",     FX_LOOK, false, 50},
    {"chk_cel",        "Comic / Cel",      FX_LOOK, false, 50}
};
static const int REMIX_EFFECT_COUNT = sizeof(s_remix_effects) / sizeof(s_remix_effects[0]);

/* Apply flash timer: counts down frames after commit for visual feedback */
static int s_apply_flash_timer = 0;
static const int APPLY_FLASH_FRAMES = 90; /* ~1.5s at 60fps */

/* Helpers to determine current context for save/load */
#ifdef PALMOD_HAS_STORAGE
static void get_current_context(const char** out_category, char* out_sub, size_t sub_size) {
    if (s_edit_mode == 1) {
        *out_category = "stage";
        snprintf(out_sub, sub_size, "row_%d", s_stage_row);
    } else {
        *out_category = "char";
        snprintf(out_sub, sub_size, "%s", character_get_name(My_char[0]));
    }
}
#endif

static void refresh_palette_list() {
#ifdef PALMOD_HAS_STORAGE
    const char* cat;
    char sub[64];
    get_current_context(&cat, sub, sizeof(sub));
    s_palette_count = palmod_list(cat, sub, s_palette_list);
    s_palette_list_str.clear();
    for (int i = 0; i < s_palette_count; i++) {
        if (i > 0) s_palette_list_str += ", ";
        s_palette_list_str += s_palette_list[i].name;
    }
    if (s_selected_palette >= s_palette_count)
        s_selected_palette = s_palette_count > 0 ? s_palette_count - 1 : 0;
#else
    s_palette_count = 0;
    s_palette_list_str = "(rebuild required)";
#endif
    s_list_dirty = false;
}

static void do_save_current_row() {
#ifdef PALMOD_HAS_STORAGE
    if (s_save_name[0] == '\0') return;
    const char* cat;
    char sub[64];
    get_current_context(&cat, sub, sizeof(sub));
    int row = (s_edit_mode == 1) ? s_stage_row : 0;
    palmod_save(cat, sub, s_save_name, ColorRAM[row]);
    s_list_dirty = true;
#endif
}

static void do_load_selected() {
#ifdef PALMOD_HAS_STORAGE
    if (s_palette_count == 0 || s_selected_palette >= s_palette_count) return;
    const char* cat;
    char sub[64];
    get_current_context(&cat, sub, sizeof(sub));
    int row = (s_edit_mode == 1) ? s_stage_row : 0;
    u16 colors[64];
    if (palmod_load(cat, sub, s_palette_list[s_selected_palette].name, colors)) {
        for (int i = 0; i < 64; i++)
            ColorRAM[row][i] = colors[i];
        palUpdateGhostCP3(row, 1);
    }
#endif
}

static void do_delete_selected() {
#ifdef PALMOD_HAS_STORAGE
    if (s_palette_count == 0 || s_selected_palette >= s_palette_count) return;
    const char* cat;
    char sub[64];
    get_current_context(&cat, sub, sizeof(sub));
    palmod_delete(cat, sub, s_palette_list[s_selected_palette].name);
    s_list_dirty = true;
#endif
}

/* ── Live palette swap ────────────────────────────────────────────── */

static void apply_palette(int id, int color_index) {
    if (!s_plcol_valid[id] || color_index < 0 || color_index > 15)
        return;

    const COL* col = &s_plcol_cache[id];
    int base = id * 16;

    if (My_char[id] == 0) {
        for (int i = 0; i < 64; i++) {
            ColorRAM[base][i]     = palConvSrcToRam(col->col[0][color_index][i]);
            ColorRAM[base + 8][i] = palConvSrcToRam(col->col[1][color_index][i]);
        }
    } else {
        for (int i = 0; i < 64; i++) {
            u16 converted = palConvSrcToRam(col->col[0][color_index][i]);
            ColorRAM[base][i]     = converted;
            ColorRAM[base + 8][i] = converted;
        }
    }

    palUpdateGhostCP3(base, 16);
}

static void apply_remix_preview() {
    /* 1. Reset base colors — skip players that have been committed */
    for (int id = 0; id < 2; id++) {
        if (s_remix_committed[id]) {
            int base = id * 16;
            for (int r = 0; r < 16; r++)
                memcpy(ColorRAM[base + r], s_committed_cram[id][r], sizeof(ColorRAM[0]));
            palUpdateGhostCP3(base, 16);
        } else if (s_plcol_valid[id]) {
            apply_palette(id, s_palmod_color[id] >= 0 ? s_palmod_color[id] : (int)Player_Color[id]);
        }
    }
    if (s_stage_committed) {
        for (int r = STAGE_ROW_MIN; r <= STAGE_ROW_MAX && (r - STAGE_ROW_MIN) < 480; r++)
            memcpy(ColorRAM[r], s_committed_stage[r - STAGE_ROW_MIN], sizeof(ColorRAM[0]));
        palUpdateGhostCP3(STAGE_ROW_MIN, STAGE_ROW_MAX - STAGE_ROW_MIN + 1);
    }
    
    auto apply_to_range = [&](int start_row, int end_row) {
        if (start_row < 0) start_row = 0;
        if (end_row > 511) end_row = 511;

        size_t count = (size_t)(end_row - start_row + 1) * 64;
        u16* colors = &ColorRAM[start_row][0];
        
        std::vector<int> ramp_indices;
        std::vector<int> global_indices;
        for (size_t i = 0; i < count; i++) {
            if (i % 16 != 0 && colors[i] != 0) {
                global_indices.push_back((int)i);
            }
        }
        
        if (s_remix_scope == 2) { // Smart Detect Ramps
            for (size_t r = 0; r < count / 64; r++) {
                for (int sub = 0; sub < 4; sub++) {
                    std::vector<u16> sub_colors;
                    std::vector<int> sub_idx;
                    for (int i = 1; i < 16; i++) {
                        int idx = (int)(r * 64 + sub * 16 + i);
                        sub_colors.push_back(colors[idx]);
                        sub_idx.push_back(idx);
                    }
                    if (PaletteRemix::is_likely_ramp(sub_colors)) {
                        ramp_indices.insert(ramp_indices.end(), sub_idx.begin(), sub_idx.end());
                    }
                }
            }
        } else {
            ramp_indices = global_indices;
        }
        
        /* Apply effects — ramp effects dispatched per 16-color bank
         * to match Python reference (groupby x // 16).              */
        for (int i = 0; i < REMIX_EFFECT_COUNT; i++) {
            const auto& fx = s_remix_effects[i];
            if (!fx.checked) continue;
            float intensity = fx.value / 100.0f;
            
            switch (fx.type) {
            case FX_MOOD:
                PaletteRemix::apply_ramp_effect(fx.name, intensity, colors, count, ramp_indices);
                break;
            case FX_RAMP: {
                /* Per-bank dispatch: group ramp_indices by (idx / 16) */
                std::vector<int> bank_chunk;
                int prev_bank = -1;
                for (int idx : ramp_indices) {
                    int bank = idx / 16;
                    if (bank != prev_bank && !bank_chunk.empty()) {
                        PaletteRemix::apply_ramp_effect(fx.name, intensity, colors, count, bank_chunk);
                        bank_chunk.clear();
                    }
                    bank_chunk.push_back(idx);
                    prev_bank = bank;
                }
                if (!bank_chunk.empty())
                    PaletteRemix::apply_ramp_effect(fx.name, intensity, colors, count, bank_chunk);
                break;
            }
            case FX_LOOK:
                PaletteRemix::apply_layer_look(fx.name, intensity, colors, count, global_indices);
                break;
            }
        }
        
        // Push updates
        for (int r = start_row; r <= end_row; r++) {
            palUpdateGhostCP3(r, 1);
        }
    };
    
    // Apply for each active target
    if (s_remix_target_p1) {
        int start = (s_remix_scope == 1) ? ((s_palmod_color[0] >= 0) ? s_palmod_color[0] : 0) : 0;
        int end = (s_remix_scope == 1) ? start : 15;
        apply_to_range(start, end);
    }
    if (s_remix_target_p2) {
        int start = (s_remix_scope == 1) ? 16 + ((s_palmod_color[1] >= 0) ? s_palmod_color[1] : 0) : 16;
        int end = (s_remix_scope == 1) ? start : 31;
        apply_to_range(start, end);
    }
    if (s_remix_target_stage) {
        /* Apply to full stage palette range, not just one row */
        apply_to_range(STAGE_ROW_MIN, STAGE_ROW_MAX);
    }
}

static void trigger_remix() {
    if (s_init_complete && s_remix_auto_preview && s_edit_mode == 2 && Play_Game != 0) {
        apply_remix_preview();
    }
}

/* Write a single color to ColorRAM and push ghost update */
static void write_single_color(int row, int col_index, u16 color) {
    if (row < 0 || row >= 512 || col_index < 0 || col_index >= 64)
        return;
    ColorRAM[row][col_index] = color;
    palUpdateGhostCP3(row, 1);
}

/* Open the color picker for a specific ColorRAM cell */
static void open_picker(int player, int row, int col_index) {
    s_picker_active = true;
    s_picker_player = player;
    s_picker_row = row;
    s_picker_index = col_index;
    u16 c = ColorRAM[row][col_index];
    s_picker_original = c;
    s_picker_r = cps3_r(c);
    s_picker_g = cps3_g(c);
    s_picker_b = cps3_b(c);
}

static void close_picker() {
    s_picker_active = false;
}

static void reset_picker_color() {
    if (!s_picker_active) return;
    s_picker_r = cps3_r(s_picker_original);
    s_picker_g = cps3_g(s_picker_original);
    s_picker_b = cps3_b(s_picker_original);
    write_single_color(s_picker_row, s_picker_index, s_picker_original);
}

/* ── Config flush ─────────────────────────────────────────────────── */

static void flush_config(void) {
    if (s_config_dirty) {
        Config_Save();
        s_config_dirty = false;
    }
}

extern "C" void rmlui_palmod_menu_flush_config(void) { flush_config(); }

/* ── Data model init ──────────────────────────────────────────────── */

static void do_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi Palmod] No context");
        return;
    }

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("palmod");
    if (!ctor) {
        SDL_Log("[RmlUi Palmod] Failed to create data model");
        return;
    }

    /* ─── Characters tab ─── */

    ctor.BindFunc("p1_color",
        [](Rml::Variant& v) {
            v = (s_palmod_color[0] >= 0) ? s_palmod_color[0] : (int)Player_Color[0];
        },
        [](const Rml::Variant& v) {
            int val = v.Get<int>();
            if (val < 0) val = 0;
            if (val > 15) val = 15;
            /* Skip empty Extra presets that would turn character all-black */
            if (is_palette_empty(0, val)) return;
            s_palmod_color[0] = val;
            if (!s_init_complete) return; /* don't write during doc load */
            save_color(0, My_char[0], val);
            s_config_dirty = true;
            if (Play_Game != 0) apply_palette(0, val);
        });

    ctor.BindFunc("p2_color",
        [](Rml::Variant& v) {
            v = (s_palmod_color[1] >= 0) ? s_palmod_color[1] : (int)Player_Color[1];
        },
        [](const Rml::Variant& v) {
            int val = v.Get<int>();
            if (val < 0) val = 0;
            if (val > 15) val = 15;
            /* Skip empty Extra presets that would turn character all-black */
            if (is_palette_empty(1, val)) return;
            s_palmod_color[1] = val;
            if (!s_init_complete) return;
            save_color(1, My_char[1], val);
            s_config_dirty = true;
            if (Play_Game != 0) apply_palette(1, val);
        });

    ctor.BindFunc("p1_name", [](Rml::Variant& v) {
        v = Rml::String(character_get_name(My_char[0]));
    });
    ctor.BindFunc("p2_name", [](Rml::Variant& v) {
        v = Rml::String(character_get_name(My_char[1]));
    });
    ctor.BindFunc("p1_label", [](Rml::Variant& v) {
        int c = (s_palmod_color[0] >= 0) ? s_palmod_color[0] : (int)Player_Color[0];
        v = Rml::String(get_color_label(c));
    });
    ctor.BindFunc("p2_label", [](Rml::Variant& v) {
        int c = (s_palmod_color[1] >= 0) ? s_palmod_color[1] : (int)Player_Color[1];
        v = Rml::String(get_color_label(c));
    });

    /* ─── Edit mode ─── */

    ctor.BindFunc("edit_mode",
        [](Rml::Variant& v) { v = s_edit_mode; },
        [](const Rml::Variant& v) { s_edit_mode = v.Get<int>(); });

    ctor.BindFunc("in_game", [](Rml::Variant& v) {
        v = (Play_Game != 0);
    });

    /* ─── Stage tab ─── */

    ctor.BindFunc("stage_row",
        [](Rml::Variant& v) { v = s_stage_row; },
        [](const Rml::Variant& v) {
            int val = v.Get<int>();
            if (val < STAGE_ROW_MIN) val = STAGE_ROW_MIN;
            if (val > STAGE_ROW_MAX) val = STAGE_ROW_MAX;
            s_stage_row = val;
        });

    ctor.BindFunc("stage_row_max", [](Rml::Variant& v) { v = STAGE_ROW_MAX; });

    /* ─── Color picker ─── */

    ctor.BindFunc("picker_active", [](Rml::Variant& v) { v = s_picker_active; });
    ctor.BindFunc("sel_index", [](Rml::Variant& v) { v = s_picker_index; });

    ctor.BindFunc("sel_r",
        [](Rml::Variant& v) { v = s_picker_r; },
        [](const Rml::Variant& v) {
            s_picker_r = v.Get<int>();
            if (s_picker_r < 0) s_picker_r = 0;
            if (s_picker_r > 31) s_picker_r = 31;
            if (s_picker_active)
                write_single_color(s_picker_row, s_picker_index,
                                   cps3_pack(s_picker_r, s_picker_g, s_picker_b));
        });

    ctor.BindFunc("sel_g",
        [](Rml::Variant& v) { v = s_picker_g; },
        [](const Rml::Variant& v) {
            s_picker_g = v.Get<int>();
            if (s_picker_g < 0) s_picker_g = 0;
            if (s_picker_g > 31) s_picker_g = 31;
            if (s_picker_active)
                write_single_color(s_picker_row, s_picker_index,
                                   cps3_pack(s_picker_r, s_picker_g, s_picker_b));
        });

    ctor.BindFunc("sel_b",
        [](Rml::Variant& v) { v = s_picker_b; },
        [](const Rml::Variant& v) {
            s_picker_b = v.Get<int>();
            if (s_picker_b < 0) s_picker_b = 0;
            if (s_picker_b > 31) s_picker_b = 31;
            if (s_picker_active)
                write_single_color(s_picker_row, s_picker_index,
                                   cps3_pack(s_picker_r, s_picker_g, s_picker_b));
        });

    ctor.BindFunc("sel_preview", [](Rml::Variant& v) {
        v = color_to_css(cps3_pack(s_picker_r, s_picker_g, s_picker_b));
    });

    ctor.BindFunc("sel_original", [](Rml::Variant& v) {
        v = color_to_css(s_picker_original);
    });

    /* ─── Event callbacks ─── */

    ctor.BindEventCallback("pick_p1_color",
        [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int idx = args[0].Get<int>();
            if (idx < 0 || idx >= 64) return;
            open_picker(0, 0, idx);
        });

    ctor.BindEventCallback("pick_p2_color",
        [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int idx = args[0].Get<int>();
            if (idx < 0 || idx >= 64) return;
            open_picker(1, 16, idx);
        });

    ctor.BindEventCallback("pick_stage_color",
        [](Rml::DataModelHandle, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int idx = args[0].Get<int>();
            if (idx < 0 || idx >= 64) return;
            open_picker(-1, s_stage_row, idx);
        });

    ctor.BindEventCallback("reset_color",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            reset_picker_color();
        });

    ctor.BindEventCallback("close_picker",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            close_picker();
        });

    ctor.BindEventCallback("set_mode_chars",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            s_edit_mode = 0;
            s_list_dirty = true;
        });

    ctor.BindEventCallback("set_mode_stage",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            s_edit_mode = 1;
            s_list_dirty = true;
        });

    ctor.BindEventCallback("set_mode_remix",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            s_edit_mode = 2;
            s_list_dirty = true;
            /* Cache original stage palette on first entry to remix tab */
            if (!s_stage_original_valid && Play_Game != 0)
                cache_stage_original();
            trigger_remix();
        });

    /* ─── Remix Tab ─── */
    
    ctor.BindFunc("remix_target_p1",
        [](Rml::Variant& v) { v = s_remix_target_p1; },
        [](const Rml::Variant& v) { s_remix_target_p1 = v.Get<bool>(); trigger_remix(); });
    ctor.BindFunc("remix_target_p2",
        [](Rml::Variant& v) { v = s_remix_target_p2; },
        [](const Rml::Variant& v) { s_remix_target_p2 = v.Get<bool>(); trigger_remix(); });
    ctor.BindFunc("remix_target_stage",
        [](Rml::Variant& v) { v = s_remix_target_stage; },
        [](const Rml::Variant& v) { s_remix_target_stage = v.Get<bool>(); trigger_remix(); });
        
    ctor.BindFunc("remix_scope",
        [](Rml::Variant& v) { v = s_remix_scope; },
        [](const Rml::Variant& v) { s_remix_scope = v.Get<int>(); trigger_remix(); });
        
    ctor.BindFunc("remix_auto_preview",
        [](Rml::Variant& v) { v = s_remix_auto_preview; },
        [](const Rml::Variant& v) { s_remix_auto_preview = v.Get<bool>(); trigger_remix(); });
        
    ctor.BindFunc("remix_l2",
        [](Rml::Variant& v) { v = s_remix_l2; },
        [](const Rml::Variant& v) { s_remix_l2 = v.Get<bool>(); trigger_remix(); });
    ctor.BindFunc("remix_l0",
        [](Rml::Variant& v) { v = s_remix_l0; },
        [](const Rml::Variant& v) { s_remix_l0 = v.Get<bool>(); trigger_remix(); });
    ctor.BindFunc("remix_l1",
        [](Rml::Variant& v) { v = s_remix_l1; },
        [](const Rml::Variant& v) { s_remix_l1 = v.Get<bool>(); trigger_remix(); });
        
    for (int i = 0; i < REMIX_EFFECT_COUNT; i++) {
        Rml::String chk_name = s_remix_effects[i].id;
        Rml::String val_name = Rml::String(s_remix_effects[i].id);
        val_name.replace(0, 4, "val_"); // "chk_noir" -> "val_noir"
        
        ctor.BindFunc(chk_name,
            [i](Rml::Variant& v) { v = s_remix_effects[i].checked; },
            [i](const Rml::Variant& v) { 
                bool newval = v.Get<bool>();
                SDL_Log("[PalMod Remix] chk_%s setter: %d -> %d", s_remix_effects[i].id, s_remix_effects[i].checked, newval);
                s_remix_effects[i].checked = newval; 
                trigger_remix(); 
            });
            
        ctor.BindFunc(val_name,
            [i](Rml::Variant& v) { v = s_remix_effects[i].value; },
            [i](const Rml::Variant& v) { 
                s_remix_effects[i].value = v.Get<int>(); 
                trigger_remix(); 
            });
    }
    
    /* Event callbacks for effect controls inside the scroll container.
     * Native <input> form controls don't work inside overflow-y: auto,
     * so we use data-event-click spans + these callbacks instead. */
    ctor.BindEventCallback("toggle_fx",
        [](Rml::DataModelHandle h, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int i = args[0].Get<int>();
            if (i < 0 || i >= REMIX_EFFECT_COUNT) return;
            s_remix_effects[i].checked = !s_remix_effects[i].checked;
            h.DirtyVariable(s_remix_effects[i].id);
            trigger_remix();
        });
    
    ctor.BindEventCallback("fx_inc",
        [](Rml::DataModelHandle h, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int i = args[0].Get<int>();
            if (i < 0 || i >= REMIX_EFFECT_COUNT) return;
            s_remix_effects[i].value = std::min(100, s_remix_effects[i].value + 5);
            ev.StopImmediatePropagation(); /* don't toggle checkbox */
            Rml::String val_name = Rml::String(s_remix_effects[i].id);
            val_name.replace(0, 4, "val_");
            h.DirtyVariable(val_name);
            trigger_remix();
        });
    
    ctor.BindEventCallback("fx_dec",
        [](Rml::DataModelHandle h, Rml::Event& ev, const Rml::VariantList& args) {
            if (args.empty()) return;
            int i = args[0].Get<int>();
            if (i < 0 || i >= REMIX_EFFECT_COUNT) return;
            s_remix_effects[i].value = std::max(0, s_remix_effects[i].value - 5);
            ev.StopImmediatePropagation(); /* don't toggle checkbox */
            Rml::String val_name = Rml::String(s_remix_effects[i].id);
            val_name.replace(0, 4, "val_");
            h.DirtyVariable(val_name);
            trigger_remix();
        });
        
    ctor.BindEventCallback("remix_reset",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            for (int i = 0; i < REMIX_EFFECT_COUNT; i++) {
                s_remix_effects[i].checked = false;
                s_remix_effects[i].value = 50;
            }
            s_remix_committed[0] = s_remix_committed[1] = false;
            s_stage_committed = false;
            if (s_plcol_valid[0]) apply_palette(0, s_palmod_color[0] >= 0 ? s_palmod_color[0] : (int)Player_Color[0]);
            if (s_plcol_valid[1]) apply_palette(1, s_palmod_color[1] >= 0 ? s_palmod_color[1] : (int)Player_Color[1]);
            restore_stage_original();
        });
        
    ctor.BindEventCallback("remix_apply",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            apply_remix_preview();
            if (s_remix_target_p1) {
                for (int r = 0; r < 16; r++)
                    memcpy(s_committed_cram[0][r], ColorRAM[r], sizeof(ColorRAM[0]));
                s_remix_committed[0] = true;
            }
            if (s_remix_target_p2) {
                for (int r = 0; r < 16; r++)
                    memcpy(s_committed_cram[1][r], ColorRAM[16 + r], sizeof(ColorRAM[0]));
                s_remix_committed[1] = true;
            }
            if (s_remix_target_stage) {
                for (int r = STAGE_ROW_MIN; r <= STAGE_ROW_MAX && (r - STAGE_ROW_MIN) < 480; r++)
                    memcpy(s_committed_stage[r - STAGE_ROW_MIN], ColorRAM[r], sizeof(ColorRAM[0]));
                s_stage_committed = true;
            }
            s_apply_flash_timer = APPLY_FLASH_FRAMES;
        });

    /* "Applied ✓" flash + committed state indicators */
    ctor.BindFunc("apply_flash", [](Rml::Variant& v) {
        v = (s_apply_flash_timer > 0);
    });
    ctor.BindFunc("apply_label", [](Rml::Variant& v) {
        v = (s_apply_flash_timer > 0) ? Rml::String("Applied!") : Rml::String("Apply");
    });
    ctor.BindFunc("committed_p1", [](Rml::Variant& v) { v = s_remix_committed[0]; });
    ctor.BindFunc("committed_p2", [](Rml::Variant& v) { v = s_remix_committed[1]; });
    ctor.BindFunc("committed_stage", [](Rml::Variant& v) { v = s_stage_committed; });

    /* ─── Save/Load bindings ─── */

    ctor.BindFunc("save_name",
        [](Rml::Variant& v) { v = s_save_name_str; },
        [](const Rml::Variant& v) {
            s_save_name_str = v.Get<Rml::String>();
            size_t len = s_save_name_str.size();
            if (len >= sizeof(s_save_name)) len = sizeof(s_save_name) - 1;
            memcpy(s_save_name, s_save_name_str.c_str(), len);
            s_save_name[len] = '\0';
        });

    ctor.BindFunc("saved_list", [](Rml::Variant& v) {
        v = s_palette_list_str;
    });

    ctor.BindFunc("saved_count", [](Rml::Variant& v) { v = s_palette_count; });

    ctor.BindFunc("selected_palette",
        [](Rml::Variant& v) { v = s_selected_palette; },
        [](const Rml::Variant& v) {
            s_selected_palette = v.Get<int>();
            if (s_selected_palette < 0) s_selected_palette = 0;
            if (s_selected_palette >= s_palette_count && s_palette_count > 0)
                s_selected_palette = s_palette_count - 1;
        });

    ctor.BindFunc("selected_name", [](Rml::Variant& v) {
#ifdef PALMOD_HAS_STORAGE
        if (s_palette_count > 0 && s_selected_palette < s_palette_count)
            v = Rml::String(s_palette_list[s_selected_palette].name);
        else
#endif
            v = Rml::String("(none)");
    });

    ctor.BindEventCallback("do_save",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            do_save_current_row();
        });

    ctor.BindEventCallback("do_load",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            do_load_selected();
        });

    ctor.BindEventCallback("do_delete",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            do_delete_selected();
        });

    ctor.BindEventCallback("prev_palette",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            if (s_selected_palette > 0) s_selected_palette--;
        });

    ctor.BindEventCallback("next_palette",
        [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) {
            if (s_selected_palette < s_palette_count - 1) s_selected_palette++;
        });

    /* ─── Color grid: 64 swatches bound as individual CSS color strings ─── */
    for (int i = 0; i < 64; i++) {
        char name[16];
        snprintf(name, sizeof(name), "sw%d", i);
        ctor.BindFunc(Rml::String(name),
            [i](Rml::Variant& v) {
                int row = (s_edit_mode == 0) ? 0 : s_stage_row;
                v = color_to_css(ColorRAM[row][i]);
            });
    }

    /* The second player's 64 swatches */
    for (int i = 0; i < 64; i++) {
        char name[16];
        snprintf(name, sizeof(name), "sw2_%d", i);
        ctor.BindFunc(Rml::String(name),
            [i](Rml::Variant& v) {
                v = color_to_css(ColorRAM[16][i]);
            });
    }

    /* Stage: swatch rows showing s_stage_row..+3 */
    for (int row_ofs = 0; row_ofs <= 3; row_ofs++) {
        for (int i = 0; i < 64; i++) {
            char name[24];
            snprintf(name, sizeof(name), "sw_s%d_%d", row_ofs, i);
            ctor.BindFunc(Rml::String(name),
                [row_ofs, i](Rml::Variant& v) {
                    int row = s_stage_row + row_ofs;
                    if (row > 511) row = 511;
                    v = color_to_css(ColorRAM[row][i]);
                });
        }
    }

    /* Stage row labels for each displayed row */
    for (int row_ofs = 0; row_ofs <= 3; row_ofs++) {
        char name[24];
        snprintf(name, sizeof(name), "stage_row_label_%d", row_ofs);
        ctor.BindFunc(Rml::String(name),
            [row_ofs](Rml::Variant& v) {
                char buf[16];
                snprintf(buf, sizeof(buf), "Row %d", s_stage_row + row_ofs);
                v = Rml::String(buf);
            });
    }

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;
    /* s_init_complete stays false until the first update() cycle completes.
     * RmlUi initialises slider values on document load, triggering setters
     * with default values — those must be absorbed before we allow writes. */

    /* Register the color-load hook so we get a persistent COL copy */
    palmod_set_hook(palmod_on_color_loaded);

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION,
                 "[RmlUi Palmod] Data model registered (%d bindings)", 128 + 20);
}

extern "C" void rmlui_palmod_menu_init(void) { do_init(); }

/* ── Per-frame update ─────────────────────────────────────────────── */

extern "C" void rmlui_palmod_menu_update(void) {
    if (!s_model_registered) { do_init(); if (!s_model_registered) return; }
    if (!s_model_handle) return;

    /* Arm setters after the first clean update cycle.  RmlUi initialises
     * slider values on document load, triggering setters with default
     * values.  Delaying one full update absorbs those spurious writes. */
    if (!s_init_complete) {
        s_init_complete = true;
        return; /* skip rest of first frame */
    }

    /* Detect character changes → load saved overrides */
    for (int id = 0; id < 2; id++) {
        if ((int)My_char[id] != s_last_char[id]) {
            s_last_char[id] = (int)My_char[id];
            s_palmod_color[id] = load_saved_color(id, My_char[id]);
        }
    }

    /* Re-apply saved palette when entering gameplay
     * Only triggers on Play_Game 0→1 transition (round start).
     * Initialise s_prev to 0xFF so the very first call never
     * detects a false 0→1 transition when the menu is opened
     * mid-match. */
    static unsigned char s_prev_play_game = 0xFF;
    if (s_prev_play_game == 0 && Play_Game != 0) {
        for (int id = 0; id < 2; id++) {
            if (s_palmod_color[id] >= 0 && plcol[id])
                apply_palette(id, s_palmod_color[id]);
        }
    }
    s_prev_play_game = Play_Game;

    /* ─── Dirty checking ─── */

#define DIRTY_INT(name, expr) do { int _c=(expr); if(_c!=s_cache.name){s_cache.name=_c;s_model_handle.DirtyVariable(#name);} } while(0)
#define DIRTY_BOOL(name, expr) do { bool _c=(expr); if(_c!=s_cache.name){s_cache.name=_c;s_model_handle.DirtyVariable(#name);} } while(0)
#define DIRTY_STR(name, expr) do { Rml::String _c(expr); if(_c!=s_cache.name){s_cache.name=_c;s_model_handle.DirtyVariable(#name);} } while(0)

    int p1c = (s_palmod_color[0] >= 0) ? s_palmod_color[0] : (int)Player_Color[0];
    int p2c = (s_palmod_color[1] >= 0) ? s_palmod_color[1] : (int)Player_Color[1];

    DIRTY_INT(p1_color, p1c);
    DIRTY_INT(p2_color, p2c);
    DIRTY_INT(edit_mode, s_edit_mode);
    DIRTY_INT(stage_row, s_stage_row);
    DIRTY_INT(sel_index, s_picker_index);
    DIRTY_INT(sel_r, s_picker_r);
    DIRTY_INT(sel_g, s_picker_g);
    DIRTY_INT(sel_b, s_picker_b);
    DIRTY_BOOL(in_game, Play_Game != 0);
    DIRTY_BOOL(picker_active, s_picker_active);

    DIRTY_STR(p1_name, character_get_name(My_char[0]));
    DIRTY_STR(p2_name, character_get_name(My_char[1]));
    DIRTY_STR(p1_label, get_color_label(p1c));
    DIRTY_STR(p2_label, get_color_label(p2c));
    DIRTY_STR(sel_preview, color_to_css(cps3_pack(s_picker_r, s_picker_g, s_picker_b)));
    DIRTY_STR(sel_original, color_to_css(s_picker_original));
    
    // Check effect dirty state
    static RemixEffectState s_fx_cache[20];
    static bool fx_init = false;
    if (!fx_init) {
        memcpy(s_fx_cache, s_remix_effects, sizeof(s_remix_effects));
        fx_init = true;
    } else {
        for (int i = 0; i < REMIX_EFFECT_COUNT; i++) {
            if (s_remix_effects[i].checked != s_fx_cache[i].checked) {
                s_fx_cache[i].checked = s_remix_effects[i].checked;
                s_model_handle.DirtyVariable(s_remix_effects[i].id);
            }
            if (s_remix_effects[i].value != s_fx_cache[i].value) {
                s_fx_cache[i].value = s_remix_effects[i].value;
                Rml::String val_name = s_remix_effects[i].id;
                val_name.replace(0, 4, "val_");
                s_model_handle.DirtyVariable(val_name);
            }
        }
    }

    /* Apply flash timer + committed state indicators */
    if (s_apply_flash_timer > 0) {
        s_apply_flash_timer--;
        s_model_handle.DirtyVariable("apply_flash");
        s_model_handle.DirtyVariable("apply_label");
    }
    /* Dirty committed indicators (cheap — only 3 bools) */
    s_model_handle.DirtyVariable("committed_p1");
    s_model_handle.DirtyVariable("committed_p2");
    s_model_handle.DirtyVariable("committed_stage");

    /* Refresh saved palette list if dirty */
    if (s_list_dirty) {
        refresh_palette_list();
        s_model_handle.DirtyVariable("saved_list");
        s_model_handle.DirtyVariable("saved_count");
        s_model_handle.DirtyVariable("selected_name");
    }

    /* Dirty swatch grid only when content could have changed. */
    static int s_prev_edit_mode = -1;
    static int s_prev_stage_row_swatch = -1;
    static int s_prev_p1c_swatch = -1;
    static int s_prev_p2c_swatch = -1;
    bool swatch_dirty = false;
    if (s_prev_edit_mode != s_edit_mode) { s_prev_edit_mode = s_edit_mode; swatch_dirty = true; }
    if (s_prev_stage_row_swatch != s_stage_row) { s_prev_stage_row_swatch = s_stage_row; swatch_dirty = true; }
    if (s_prev_p1c_swatch != p1c) { s_prev_p1c_swatch = p1c; swatch_dirty = true; }
    if (s_prev_p2c_swatch != p2c) { s_prev_p2c_swatch = p2c; swatch_dirty = true; }
    if (s_picker_active) swatch_dirty = true;
    if (s_edit_mode == 2) swatch_dirty = true; /* remix tab: colors change every frame */
    if (swatch_dirty) {
        for (int i = 0; i < 64; i++) {
            char name[24];
            snprintf(name, sizeof(name), "sw%d", i);
            s_model_handle.DirtyVariable(name);
            snprintf(name, sizeof(name), "sw2_%d", i);
            s_model_handle.DirtyVariable(name);
            for (int r = 0; r <= 3; r++) {
                snprintf(name, sizeof(name), "sw_s%d_%d", r, i);
                s_model_handle.DirtyVariable(name);
            }
        }
        for (int r = 0; r <= 3; r++) {
            char name[24];
            snprintf(name, sizeof(name), "stage_row_label_%d", r);
            s_model_handle.DirtyVariable(name);
        }
    }

#undef DIRTY_INT
#undef DIRTY_BOOL
#undef DIRTY_STR
}

/* ── Shutdown ─────────────────────────────────────────────────────── */

extern "C" void rmlui_palmod_menu_shutdown(void) {
    flush_config();
    palmod_set_hook(NULL); /* unregister before teardown */
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx) ctx->RemoveDataModel("palmod");
        s_model_registered = false;
    }
    s_init_complete = false;
    s_plcol_valid[0] = s_plcol_valid[1] = false;
    s_stage_original_valid = false;
    SDL_Log("[RmlUi Palmod] Shut down");
}
