/**
 * @file rmlui_game_option.cpp
 * @brief RmlUi Game Option screen data model.
 *
 * Replaces the CPS3 effect_61 labels + effect_64 value columns with an
 * HTML/CSS two-column table. The underlying Game_Option_Sub() input handler
 * still drives the values — the RmlUi document reads Convert_Buff each frame.
 */

#include "port/sdl/rmlui_game_option.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {

/* Game globals — Menu_Cursor_Y, IO_Result, Convert_Buff */
#include "sf33rd/Source/Game/engine/workuser.h"
#include "structs.h"

} // extern "C"

// ─── Value display strings ──────────────────────────────────────
// Each setting maps its raw Convert_Buff value to a display string

static const char* difficulty_str(int v) {
    static const char* tbl[] = { "1", "2", "3", "4", "5", "6", "7", "8" };
    return (v >= 0 && v < 8) ? tbl[v] : "?";
}

static const char* time_limit_str(int v) {
    switch (v) {
    case 0:
        return "30";
    case 1:
        return "60";
    case 2:
        return "99";
    case 3:
        return "NONE";
    default:
        return "?";
    }
}

static const char* rounds_str(int v) {
    static const char* tbl[] = { "1", "2", "3" };
    return (v >= 0 && v < 3) ? tbl[v] : "?";
}

static const char* damage_str(int v) {
    static const char* tbl[] = { "1", "2", "3", "4", "5" };
    return (v >= 0 && v < 5) ? tbl[v] : "?";
}

static const char* guard_str(int v) {
    return v == 0 ? "OLD" : "NEW";
}

static const char* enabled_str(int v) {
    return v == 0 ? "ENABLE" : "DISABLE";
}

static const char* onoff_str(int v) {
    return v == 0 ? "ON" : "OFF";
}

static const char* human_com_str(int v) {
    return v == 0 ? "HUMAN" : "COM";
}

// Row config: label, value formatter
struct OptRow {
    const char* label;
    const char* (*format)(int);
};

static const OptRow s_rows[10] = {
    { "DIFFICULTY", difficulty_str },  { "TIME LIMIT", time_limit_str }, { "ROUNDS (1P)", rounds_str },
    { "ROUNDS (VS)", rounds_str },     { "DAMAGE LEVEL", damage_str },   { "GUARD JUDGMENT", guard_str },
    { "ANALOG STICK", enabled_str },   { "HANDICAP (VS)", onoff_str },   { "PLAYER1 (VS)", human_com_str },
    { "PLAYER2 (VS)", human_com_str },
};

// ─── Data model ──────────────────────────────────────────────────
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

struct GameOptionCache {
    int cursor;
    int values[10];
};
static GameOptionCache s_cache = {};

// ─── Init ─────────────────────────────────────────────────────────
extern "C" void rmlui_game_option_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("game_option");
    if (!ctor)
        return;

    ctor.BindFunc("game_opt_cursor", [](Rml::Variant& v) { v = (int)Menu_Cursor_Y[0]; });

    // Bind each row's label and formatted value
    for (int i = 0; i < 10; i++) {
        // Label (static)
        {
            char name[32];
            snprintf(name, sizeof(name), "game_opt_label_%d", i);
            const char* label = s_rows[i].label;
            ctor.BindFunc(Rml::String(name), [label](Rml::Variant& v) { v = Rml::String(label); });
        }
        // Value (dynamic)
        {
            char name[32];
            snprintf(name, sizeof(name), "game_opt_value_%d", i);
            int idx = i;
            ctor.BindFunc(Rml::String(name), [idx](Rml::Variant& v) {
                int raw = Convert_Buff[0][0][idx];
                v = Rml::String(s_rows[idx].format(raw));
            });
        }
    }

    // Event: select item → IO_Result
    ctor.BindEventCallback("select_item", [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList& args) {
        if (!args.empty()) {
            int idx = args[0].Get<int>();
            Menu_Cursor_Y[0] = (short)idx;
            IO_Result = 0x100;
        }
    });

    ctor.BindEventCallback("cancel",
                           [](Rml::DataModelHandle, Rml::Event&, const Rml::VariantList&) { IO_Result = 0x200; });

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_Log("[RmlUi GameOption] Data model registered");
}

// ─── Per-frame update ─────────────────────────────────────────────
extern "C" void rmlui_game_option_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    int cur = (int)Menu_Cursor_Y[0];
    if (cur != s_cache.cursor) {
        s_cache.cursor = cur;
        s_model_handle.DirtyVariable("game_opt_cursor");
    }

    for (int i = 0; i < 10; i++) {
        int v = Convert_Buff[0][0][i];
        if (v != s_cache.values[i]) {
            s_cache.values[i] = v;
            char name[32];
            snprintf(name, sizeof(name), "game_opt_value_%d", i);
            s_model_handle.DirtyVariable(name);
        }
    }
}

// ─── Show / Hide ──────────────────────────────────────────────────
extern "C" void rmlui_game_option_show(void) {
    rmlui_wrapper_show_game_document("game_option");
    if (s_model_handle) {
        s_model_handle.DirtyVariable("game_opt_cursor");
        for (int i = 0; i < 10; i++) {
            char name[32];
            snprintf(name, sizeof(name), "game_opt_value_%d", i);
            s_model_handle.DirtyVariable(name);
        }
    }
}

extern "C" void rmlui_game_option_hide(void) {
    rmlui_wrapper_hide_game_document("game_option");
}

// ─── Shutdown ─────────────────────────────────────────────────────
extern "C" void rmlui_game_option_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("game_option");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("game_option");
        s_model_registered = false;
    }
}
