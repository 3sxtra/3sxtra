/**
 * @file lua_engine_bridge.cpp
 * @brief Lua engine bridge: exposes native engine state to RmlUi Lua scripts.
 *
 * Replaces FBNeo's memory.read, joypad, emu APIs with direct struct access.
 * Registered as the `engine` global table in the Lua state.
 *
 * Lua API:
 *   engine.read_player(id)       -> full player table (50+ fields from PLW/WORK)
 *   engine.read_globals()        -> global game state table
 *   engine.get_player_state(id)  -> TrainingPlayerState table (derived fields)
 *   engine.get_frame_number()    -> integer
 *   engine.is_in_match()         -> boolean
 *   engine.get_input(id)         -> integer (raw p1sw_0 / p2sw_0)
 *   engine.set_lever_buff(id, v) -> nil (writes Lever_Buff[id])
 */

#include "lua_engine_bridge.h"

#include <RmlUi/Lua/Interpreter.h>
#include <SDL3/SDL.h>
#include <cstring>

extern "C" {
#include <lauxlib.h>
#include <lua.h>
}

extern "C" {
#include "sf33rd/Source/Game/engine/plcnt.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/training/training_state.h"
#include "structs.h"
}

// ---- Helpers ----

// Push a light int field: lua_pushinteger + lua_setfield
#define PUSH_INT(L, tbl, name, val)                                                                                    \
    lua_pushinteger(L, (lua_Integer)(val));                                                                            \
    lua_setfield(L, tbl, name)

#define PUSH_NUM(L, tbl, name, val)                                                                                    \
    lua_pushnumber(L, (lua_Number)(val));                                                                              \
    lua_setfield(L, tbl, name)

#define PUSH_BOOL(L, tbl, name, val)                                                                                   \
    lua_pushboolean(L, (int)(val));                                                                                    \
    lua_setfield(L, tbl, name)

// ---- engine.read_player(id) ----
// Returns the raw PLW/WORK fields matching gamestate.lua's field names.

static int l_read_player(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    if (id < 1 || id > 2) {
        lua_pushnil(L);
        return 1;
    }

    PLW* wk = &plw[id - 1];
    WORK* wu = &wk->wu;

    lua_createtable(L, 0, 80); // pre-allocate hash part
    int t = lua_gettop(L);

    // --- Position (matches memory_addresses offsets 0x64-0x6A) ---
    // xyz[0] = X, xyz[1] = Y, xyz[2] = Z
    PUSH_INT(L, t, "pos_x_char", wu->xyz[0].disp.pos);
    PUSH_INT(L, t, "pos_x_mantissa", (u8)wu->xyz[0].disp.low);
    PUSH_INT(L, t, "pos_y_char", wu->xyz[1].disp.pos);
    PUSH_INT(L, t, "pos_y_mantissa", (u8)wu->xyz[1].disp.low);
    // Computed float position (for convenience)
    PUSH_NUM(L, t, "pos_x", wu->xyz[0].disp.pos + (u8)wu->xyz[0].disp.low / 256.0);
    PUSH_NUM(L, t, "pos_y", wu->xyz[1].disp.pos + (u8)wu->xyz[1].disp.low / 256.0);

    // --- Velocity (offsets 0x7C-0x8A) ---
    PUSH_INT(L, t, "velocity_x_char", wu->mvxy.a[0].real.h);
    PUSH_INT(L, t, "velocity_x_mantissa", (u8)wu->mvxy.a[0].real.l);
    PUSH_INT(L, t, "velocity_y_char", wu->mvxy.a[1].real.h);
    PUSH_INT(L, t, "velocity_y_mantissa", (u8)wu->mvxy.a[1].real.l);
    PUSH_NUM(L, t, "velocity_x", wu->mvxy.a[0].real.h + (u8)wu->mvxy.a[0].real.l / 256.0);
    PUSH_NUM(L, t, "velocity_y", wu->mvxy.a[1].real.h + (u8)wu->mvxy.a[1].real.l / 256.0);

    // --- Acceleration (offsets 0x84-0x92) ---
    PUSH_INT(L, t, "acceleration_x_char", wu->mvxy.d[0].real.h);
    PUSH_INT(L, t, "acceleration_x_mantissa", (u8)wu->mvxy.d[0].real.l);
    PUSH_INT(L, t, "acceleration_y_char", wu->mvxy.d[1].real.h);
    PUSH_INT(L, t, "acceleration_y_mantissa", (u8)wu->mvxy.d[1].real.l);

    // --- Direction (offset 0x0A = flip_x) ---
    PUSH_INT(L, t, "flip_x", wu->direction);
    PUSH_INT(L, t, "direction", wu->direction);

    // --- Character ID (offset 0x3C0) ---
    PUSH_INT(L, t, "char_id", wu->cg_number);

    // --- Vitality (offset 0x9F via base) ---
    PUSH_INT(L, t, "life", wu->vitality);

    // --- Posture (offsets 0x20E, 0x209) ---
    // In the original, posture is at base+0x20E which maps to char_state posture info
    // Using the same approach as training_state.c
    PUSH_INT(L, t, "posture", wu->cg_type);
    PUSH_INT(L, t, "posture_ext", wu->cg_ctr);

    // --- Movement type (offset 0x0AD) ---
    // action_type in memory_addresses is at same offset
    PUSH_INT(L, t, "movement_type", wu->cgd_type);
    PUSH_INT(L, t, "movement_type2", wu->kind_of_waza);
    PUSH_INT(L, t, "action_type", wu->cgd_type);

    // --- Routine state ---
    PUSH_INT(L, t, "routine_no_0", wu->routine_no[0]);
    PUSH_INT(L, t, "routine_no_1", wu->routine_no[1]);

    // --- Recovery (offset 0x187) ---
    PUSH_INT(L, t, "recovery_time", wu->dm_stop);

    // --- Hit stop ---
    PUSH_INT(L, t, "hit_stop", wu->hit_stop);

    // --- Remaining freeze frames (offset 0x45) ---
    PUSH_INT(L, t, "remaining_freeze_frames", wu->dead_f);

    // --- Animation frame IDs ---
    PUSH_INT(L, t, "animation_frame_id", wu->cg_ix);
    PUSH_INT(L, t, "char_index", wu->char_index);
    PUSH_INT(L, t, "cg_number", wu->cg_number);

    // --- Attack state ---
    PUSH_INT(L, t, "kind_of_waza", wu->kind_of_waza);
    PUSH_INT(L, t, "pat_status", wu->pat_status);
    PUSH_INT(L, t, "attpow", wu->attpow);
    PUSH_INT(L, t, "defpow", wu->defpow);

    // --- Missing fields from Phase 2 audit ---
    // Damage state
    PUSH_INT(L, t, "dm_stop", wu->dm_stop);
    PUSH_INT(L, t, "dm_guard_success", wu->dm_guard_success);
    PUSH_INT(L, t, "dm_attlv", wu->dm_attlv);
    PUSH_INT(L, t, "dm_piyo", wu->dm_piyo);

    // Extended routine state
    PUSH_INT(L, t, "routine_no_2", wu->routine_no[2]);
    PUSH_INT(L, t, "routine_no_3", wu->routine_no[3]);

    // Animation - old cg_number for change detection
    PUSH_INT(L, t, "old_cgnum", wu->old_cgnum);

    // Attack attributes and hit tracking
    PUSH_INT(L, t, "at_attribute", wu->at_attribute);
    PUSH_INT(L, t, "hit_flag", wu->hf.hit_flag);
    PUSH_INT(L, t, "attack_num", wu->attack_num);
    PUSH_INT(L, t, "zu_flag", wu->zu_flag);
    PUSH_INT(L, t, "vitality", wu->vitality);
    PUSH_INT(L, t, "vital_new", wu->vital_new);

    // Family / friends (for projectile identification)
    PUSH_INT(L, t, "my_family", wu->my_family);

    // --- PLW-specific fields ---
    // Guard
    PUSH_INT(L, t, "guard_flag", wk->guard_flag);
    PUSH_INT(L, t, "guard_chuu", wk->guard_chuu);
    PUSH_INT(L, t, "kind_of_blocking", wk->kind_of_blocking);

    // Throws
    PUSH_BOOL(L, t, "is_being_thrown", wk->tsukamare_f);
    PUSH_BOOL(L, t, "is_throwing", wk->tsukami_f);
    PUSH_INT(L, t, "tsukami_num", wk->tsukami_num);

    // Combo
    PUSH_INT(L, t, "combo_total", wk->combo_type.total);
    PUSH_INT(L, t, "combo_new_dm", wk->combo_type.new_dm);

    // Dead
    PUSH_INT(L, t, "dead_flag", wk->dead_flag);

    // Cancel
    PUSH_INT(L, t, "cancel_timer", wk->cancel_timer);

    // Player number
    PUSH_INT(L, t, "player_number", wk->player_number);

    // Ukemi
    PUSH_INT(L, t, "ukemi_ok_timer", wk->ukemi_ok_timer);

    // --- Stun (PiyoriType) ---
    if (wk->py) {
        PUSH_INT(L, t, "stun_flag", wk->py->flag);
        PUSH_INT(L, t, "stun_genkai", wk->py->genkai);
        PUSH_INT(L, t, "stun_time", wk->py->time);
        PUSH_INT(L, t, "stun_now", wk->py->now.timer);
        PUSH_INT(L, t, "stun_recover", wk->py->recover);
    } else {
        PUSH_INT(L, t, "stun_flag", 0);
        PUSH_INT(L, t, "stun_genkai", 0);
        PUSH_INT(L, t, "stun_time", 0);
        PUSH_INT(L, t, "stun_now", 0);
        PUSH_INT(L, t, "stun_recover", 0);
    }

    // --- Super gauge (SA_WORK) ---
    if (wk->sa) {
        PUSH_INT(L, t, "gauge", wk->sa->gauge.s.h);
        PUSH_INT(L, t, "gauge_mantissa", wk->sa->gauge.s.l);
        PUSH_INT(L, t, "gauge_len", wk->sa->gauge_len);
        PUSH_INT(L, t, "store", wk->sa->store);
        PUSH_INT(L, t, "store_max", wk->sa->store_max);
        PUSH_INT(L, t, "selected_sa", wk->sa->kind_of_arts);
        PUSH_INT(L, t, "sa_state", wk->sa->ok);
    } else {
        PUSH_INT(L, t, "gauge", 0);
        PUSH_INT(L, t, "gauge_mantissa", 0);
        PUSH_INT(L, t, "gauge_len", 0);
        PUSH_INT(L, t, "store", 0);
        PUSH_INT(L, t, "store_max", 0);
        PUSH_INT(L, t, "selected_sa", 0);
        PUSH_INT(L, t, "sa_state", 0);
    }

    // --- Hitboxes ---
    // Push simplified hitbox data for the OSD
    if (wu->h_att) {
        lua_createtable(L, 4, 0);
        for (int i = 0; i < 4; i++) {
            lua_createtable(L, 4, 0);
            for (int j = 0; j < 4; j++) {
                lua_pushinteger(L, wu->h_att->att_box[i][j]);
                lua_rawseti(L, -2, j + 1);
            }
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, t, "attack_boxes");
    }

    if (wu->h_bod) {
        lua_createtable(L, 4, 0);
        for (int i = 0; i < 4; i++) {
            lua_createtable(L, 4, 0);
            for (int j = 0; j < 4; j++) {
                lua_pushinteger(L, wu->h_bod->body_dm[i][j]);
                lua_rawseti(L, -2, j + 1);
            }
            lua_rawseti(L, -2, i + 1);
        }
        lua_setfield(L, t, "body_boxes");
    }

    // --- WORK_CP (command processing) ---
    if (wk->cp) {
        PUSH_INT(L, t, "sw_now", wk->cp->sw_now);
        PUSH_INT(L, t, "sw_new", wk->cp->sw_new);
        PUSH_INT(L, t, "sw_old", wk->cp->sw_old);
        PUSH_INT(L, t, "lever_dir", wk->cp->lever_dir);
    }

    // --- PLW extended ---
    PUSH_INT(L, t, "current_attack", wk->current_attack);
    PUSH_INT(L, t, "running_f", wk->running_f);
    PUSH_INT(L, t, "zuru_flag", wk->zuru_flag ? 1 : 0);
    PUSH_INT(L, t, "att_plus", wk->att_plus);
    PUSH_INT(L, t, "def_plus", wk->def_plus);
    PUSH_INT(L, t, "high_jump_flag", wk->high_jump_flag);

    return 1;
}

// ---- engine.read_globals() ----

static int l_read_globals(lua_State* L) {
    lua_createtable(L, 0, 10);
    int t = lua_gettop(L);

    PUSH_INT(L, t, "frame_number", g_training_state.frame_number);
    PUSH_BOOL(L, t, "is_in_match", g_training_state.is_in_match);

    // Stage
    extern s8 VS_Stage;
    PUSH_INT(L, t, "stage", VS_Stage);

    // Screen scroll position (BG layer 0 target)
    extern s16 Target_BG_X[6];
    PUSH_INT(L, t, "screen_x", Target_BG_X[0]);

    return 1;
}

// ---- Original Phase 1 functions (kept for compatibility) ----

static int l_get_frame_number(lua_State* L) {
    lua_pushinteger(L, g_training_state.frame_number);
    return 1;
}

static int l_is_in_match(lua_State* L) {
    lua_pushboolean(L, g_training_state.is_in_match);
    return 1;
}

/** Push a TrainingPlayerState (derived/computed fields). */
static void push_training_state(lua_State* L, const TrainingPlayerState* p) {
    lua_createtable(L, 0, 25);
    int t = lua_gettop(L);

    PUSH_INT(L, t, "frame_state", (int)p->current_frame_state);

    PUSH_BOOL(L, t, "is_standing", p->is_standing);
    PUSH_BOOL(L, t, "is_crouching", p->is_crouching);
    PUSH_BOOL(L, t, "is_jumping", p->is_jumping);
    PUSH_BOOL(L, t, "is_airborne", p->is_airborne);
    PUSH_BOOL(L, t, "is_grounded", p->is_grounded);
    PUSH_BOOL(L, t, "is_attacking", p->is_attacking);
    PUSH_BOOL(L, t, "has_just_attacked", p->has_just_attacked);
    PUSH_BOOL(L, t, "is_in_recovery", p->is_in_recovery);
    PUSH_BOOL(L, t, "is_blocking", p->is_blocking);
    PUSH_BOOL(L, t, "has_just_blocked", p->has_just_blocked);
    PUSH_BOOL(L, t, "is_idle", p->is_idle);
    PUSH_BOOL(L, t, "has_hitboxes", p->has_hitboxes);
    PUSH_BOOL(L, t, "is_being_thrown", p->is_being_thrown);
    PUSH_BOOL(L, t, "is_stunned", p->is_stunned);

    PUSH_INT(L, t, "stun_timer", p->stun_timer);
    PUSH_INT(L, t, "advantage", p->advantage_value);
    PUSH_INT(L, t, "startup", p->last_startup);
    PUSH_INT(L, t, "active", p->last_active);
    PUSH_INT(L, t, "recovery", p->last_recovery);
    PUSH_INT(L, t, "combo_hits", p->combo_hits);
    PUSH_INT(L, t, "combo_stun", p->combo_stun);
    PUSH_INT(L, t, "wakeup_time", p->remaining_wakeup_time);
}

static int l_get_player_state(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    TrainingPlayerState* p = get_training_player((s16)id);
    if (!p) {
        lua_pushnil(L);
        return 1;
    }
    push_training_state(L, p);
    return 1;
}

static int l_get_input(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    u16 val = (id == 0 || id == 1) ? p1sw_0 : p2sw_0;
    lua_pushinteger(L, val);
    return 1;
}

static int l_set_lever_buff(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    int val = (int)luaL_checkinteger(L, 2);
    if (id >= 0 && id < 2) {
        Lever_Buff[id] = (u16)val;
    }
    return 0;
}

// ---- engine.write_field(id, field, val) ----
// Generic write for gauge refill, health reset, position override.
static int l_write_field(lua_State* L) {
    int id = (int)luaL_checkinteger(L, 1);
    const char* field = luaL_checkstring(L, 2);
    int val = (int)luaL_checkinteger(L, 3);
    if (id < 1 || id > 2)
        return 0;

    PLW* wk = &plw[id - 1];

    if (strcmp(field, "life") == 0 || strcmp(field, "vitality") == 0) {
        wk->wu.vitality = (s16)val;
    } else if (strcmp(field, "pos_x") == 0) {
        wk->wu.xyz[0].disp.pos = (s16)val;
    } else if (strcmp(field, "pos_y") == 0) {
        wk->wu.xyz[1].disp.pos = (s16)val;
    } else if (strcmp(field, "gauge") == 0 && wk->sa) {
        wk->sa->gauge.s.h = (s16)val;
    } else if (strcmp(field, "store") == 0 && wk->sa) {
        wk->sa->store = (s16)val;
    } else if (strcmp(field, "att_plus") == 0) {
        wk->att_plus = (s16)val;
    } else if (strcmp(field, "def_plus") == 0) {
        wk->def_plus = (s16)val;
    } else {
        SDL_Log("[Lua Bridge] write_field: unknown field '%s'", field);
    }
    return 0;
}

// ---- engine.tick() ----
// Called per-frame from C to fire emu.registerbefore() callbacks.
static int l_tick(lua_State* L) {
    // Fire emu._fire_before() if it exists
    lua_getglobal(L, "emu");
    if (lua_istable(L, -1)) {
        lua_getfield(L, -1, "_fire_before");
        if (lua_isfunction(L, -1)) {
            if (lua_pcall(L, 0, 0, 0) != LUA_OK) {
                SDL_Log("[Lua Bridge] emu._fire_before error: %s", lua_tostring(L, -1));
                lua_pop(L, 1);
            }
        } else {
            lua_pop(L, 1);
        }
    }
    lua_pop(L, 1);
    return 0;
}

// ---- Registration ----

static const luaL_Reg engine_funcs[] = {
    // Phase 2: raw struct access
    { "read_player", l_read_player },
    { "read_globals", l_read_globals },
    // Phase 1: derived state
    { "get_frame_number", l_get_frame_number },
    { "is_in_match", l_is_in_match },
    { "get_player_state", l_get_player_state },
    { "get_input", l_get_input },
    { "set_lever_buff", l_set_lever_buff },
    { "write_field", l_write_field },
    { "tick", l_tick },
    { NULL, NULL }
};

void lua_engine_bridge_init(void) {
    lua_State* L = Rml::Lua::Interpreter::GetLuaState();
    if (!L) {
        SDL_Log("[Lua Bridge] ERROR: No Lua state available");
        return;
    }

    luaL_newlib(L, engine_funcs);
    lua_setglobal(L, "engine");

    SDL_Log("[Lua Bridge] Registered engine API (%d functions)",
            (int)(sizeof(engine_funcs) / sizeof(engine_funcs[0]) - 1));
}

void lua_engine_bridge_shutdown(void) {
    // Nothing to clean up -- Lua state is owned by RmlUi
}

void lua_engine_bridge_tick(void) {
    lua_State* L = Rml::Lua::Interpreter::GetLuaState();
    if (!L)
        return;
    l_tick(L);
}
