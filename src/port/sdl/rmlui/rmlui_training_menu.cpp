/**
 * @file rmlui_training_menu.cpp
 * @brief RmlUi modded training overlay — full Dummy + Display settings.
 *
 * Opened via F7 during training mode.  Provides two tabs:
 *   DUMMY  – blocking, parrying, mash, tech throw, fast wakeup
 *   DISPLAY – hitboxes, stun, advantage, inputs, frame meter
 *
 * Dummy settings are written directly to g_dummy_settings so the
 * Lua dummy (via engine.get_dummy_settings()) picks them up each frame.
 * Display settings are written to g_training_menu_settings as before.
 * Both groups persist to the INI config file.
 *
 * Label strings come from training_dummy.h _str() functions (single
 * source of truth).  List items cycle on click via event callbacks.
 */
#include "port/sdl/rmlui/rmlui_training_menu.h"
#include "port/config/config.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"
#include "port/training_menu.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

extern "C" {
#include "sf33rd/Source/Game/training/training_dummy.h"
}

// -------------------------------------------------------------------
// Data model state
// -------------------------------------------------------------------
static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;

// Active tab index (0 = DUMMY, 1 = DISPLAY)
static int s_active_tab = 0;

// Cached snapshot for dirty detection
struct TrainingSnapshot {
    // Display tab
    bool hitboxes, pushboxes, hurtboxes, attackboxes, throwboxes;
    bool advantage, stun, inputs, frame_meter;
    // Dummy tab
    int block_type, parry_type, stun_mash, wakeup_mash;
    bool wakeup_reversal, guard_low;
    int tech_throw, fast_wakeup;
    // Tab
    int active_tab;
};
static TrainingSnapshot s_cache = {};

// -------------------------------------------------------------------
// Cycling helper — wraps an int enum value forward by 1
// -------------------------------------------------------------------
static inline int cycle_enum(int current, int count) {
    return (current + 1) % count;
}

// -------------------------------------------------------------------
// Event listener for cycling list-type dummy settings on click
// -------------------------------------------------------------------
class DummyCycleListener : public Rml::EventListener {
  public:
    // field_id selects which dummy setting to cycle
    enum Field { BLOCK, PARRY, STUN_MASH, WAKEUP_MASH, TECH_THROW, FAST_WAKEUP };

    DummyCycleListener(Field f) : field(f) {}

    void ProcessEvent(Rml::Event& /*event*/) override {
        switch (field) {
        case BLOCK:
            g_dummy_settings.block_type =
                (DummyBlockType)cycle_enum((int)g_dummy_settings.block_type, DummyBlockType_count());
            Config_SetInt(CFG_KEY_DUMMY_BLOCK, (int)g_dummy_settings.block_type);
            break;
        case PARRY:
            g_dummy_settings.parry_type =
                (DummyParryType)cycle_enum((int)g_dummy_settings.parry_type, DummyParryType_count());
            Config_SetInt(CFG_KEY_DUMMY_PARRY, (int)g_dummy_settings.parry_type);
            break;
        case STUN_MASH:
            g_dummy_settings.stun_mash =
                (DummyMashType)cycle_enum((int)g_dummy_settings.stun_mash, DummyMashType_count());
            Config_SetInt(CFG_KEY_DUMMY_STUN_MASH, (int)g_dummy_settings.stun_mash);
            break;
        case WAKEUP_MASH:
            g_dummy_settings.wakeup_mash =
                (DummyMashType)cycle_enum((int)g_dummy_settings.wakeup_mash, DummyMashType_count());
            Config_SetInt(CFG_KEY_DUMMY_WAKEUP_MASH, (int)g_dummy_settings.wakeup_mash);
            break;
        case TECH_THROW:
            g_dummy_settings.tech_throw_type =
                (DummyTechThrowType)cycle_enum((int)g_dummy_settings.tech_throw_type, DummyTechThrowType_count());
            Config_SetInt(CFG_KEY_DUMMY_TECH_THROW, (int)g_dummy_settings.tech_throw_type);
            break;
        case FAST_WAKEUP:
            g_dummy_settings.fast_wakeup =
                (DummyFastWakeupType)cycle_enum((int)g_dummy_settings.fast_wakeup, DummyFastWakeupType_count());
            Config_SetInt(CFG_KEY_DUMMY_FAST_WAKEUP, (int)g_dummy_settings.fast_wakeup);
            break;
        }
        Config_Save();
    }

  private:
    Field field;
};

// Static listeners (lifetime matches the program)
static DummyCycleListener s_cycle_block(DummyCycleListener::BLOCK);
static DummyCycleListener s_cycle_parry(DummyCycleListener::PARRY);
static DummyCycleListener s_cycle_stun_mash(DummyCycleListener::STUN_MASH);
static DummyCycleListener s_cycle_wakeup_mash(DummyCycleListener::WAKEUP_MASH);
static DummyCycleListener s_cycle_tech_throw(DummyCycleListener::TECH_THROW);
static DummyCycleListener s_cycle_fast_wakeup(DummyCycleListener::FAST_WAKEUP);

// -------------------------------------------------------------------
// Document loaded callback — attach event listeners to list rows
// -------------------------------------------------------------------
static void attach_cycle_listeners(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx)
        return;

    // Find the training document
    for (int i = 0; i < ctx->GetNumDocuments(); i++) {
        Rml::ElementDocument* doc = ctx->GetDocument(i);
        if (!doc || doc->GetTitle() != "Training Options")
            continue;

        auto bind = [&](const char* id, Rml::EventListener* listener) {
            Rml::Element* el = doc->GetElementById(id);
            if (el)
                el->AddEventListener(Rml::EventId::Click, listener);
        };

        bind("cycle-block", &s_cycle_block);
        bind("cycle-parry", &s_cycle_parry);
        bind("cycle-stun-mash", &s_cycle_stun_mash);
        bind("cycle-wakeup-mash", &s_cycle_wakeup_mash);
        bind("cycle-tech-throw", &s_cycle_tech_throw);
        bind("cycle-fast-wakeup", &s_cycle_fast_wakeup);
        break;
    }
}

// -------------------------------------------------------------------
// Init — register data model + load persisted config
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
    if (!ctx) {
        SDL_Log("[RmlUi Training] No context available");
        return;
    }

    // Load persisted dummy settings from config
    if (Config_HasKey(CFG_KEY_DUMMY_BLOCK))
        g_dummy_settings.block_type = (DummyBlockType)Config_GetInt(CFG_KEY_DUMMY_BLOCK);
    if (Config_HasKey(CFG_KEY_DUMMY_PARRY))
        g_dummy_settings.parry_type = (DummyParryType)Config_GetInt(CFG_KEY_DUMMY_PARRY);
    if (Config_HasKey(CFG_KEY_DUMMY_STUN_MASH))
        g_dummy_settings.stun_mash = (DummyMashType)Config_GetInt(CFG_KEY_DUMMY_STUN_MASH);
    if (Config_HasKey(CFG_KEY_DUMMY_WAKEUP_MASH))
        g_dummy_settings.wakeup_mash = (DummyMashType)Config_GetInt(CFG_KEY_DUMMY_WAKEUP_MASH);
    if (Config_HasKey(CFG_KEY_DUMMY_WAKEUP_REVERSAL))
        g_dummy_settings.wakeup_reversal = Config_GetBool(CFG_KEY_DUMMY_WAKEUP_REVERSAL);
    if (Config_HasKey(CFG_KEY_DUMMY_GUARD_LOW))
        g_dummy_settings.guard_low_default = Config_GetBool(CFG_KEY_DUMMY_GUARD_LOW);
    if (Config_HasKey(CFG_KEY_DUMMY_TECH_THROW))
        g_dummy_settings.tech_throw_type = (DummyTechThrowType)Config_GetInt(CFG_KEY_DUMMY_TECH_THROW);
    if (Config_HasKey(CFG_KEY_DUMMY_FAST_WAKEUP))
        g_dummy_settings.fast_wakeup = (DummyFastWakeupType)Config_GetInt(CFG_KEY_DUMMY_FAST_WAKEUP);

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("training");
    if (!ctor) {
        SDL_Log("[RmlUi Training] Failed to create data model");
        return;
    }

    // ═══════════════════════════════════════════════════════════════
    // Tab navigation
    // ═══════════════════════════════════════════════════════════════
    ctor.BindFunc(
        "active_tab",
        [](Rml::Variant& v) { v = s_active_tab; },
        [](const Rml::Variant& v) { s_active_tab = v.Get<int>(); });

    // ═══════════════════════════════════════════════════════════════
    // DUMMY TAB — labels use _str() from training_dummy.h
    // Values are cycled via click event listeners, not data setters.
    // ═══════════════════════════════════════════════════════════════
    ctor.BindFunc("dummy_block_label",
                  [](Rml::Variant& v) { v = Rml::String(DummyBlockType_str((int)g_dummy_settings.block_type)); });
    ctor.BindFunc("dummy_parry_label",
                  [](Rml::Variant& v) { v = Rml::String(DummyParryType_str((int)g_dummy_settings.parry_type)); });
    ctor.BindFunc("dummy_stun_mash_label",
                  [](Rml::Variant& v) { v = Rml::String(DummyMashType_str((int)g_dummy_settings.stun_mash)); });
    ctor.BindFunc("dummy_wakeup_mash_label",
                  [](Rml::Variant& v) { v = Rml::String(DummyMashType_str((int)g_dummy_settings.wakeup_mash)); });
    ctor.BindFunc("dummy_tech_throw_label", [](Rml::Variant& v) {
        v = Rml::String(DummyTechThrowType_str((int)g_dummy_settings.tech_throw_type));
    });
    ctor.BindFunc("dummy_fast_wakeup_label",
                  [](Rml::Variant& v) { v = Rml::String(DummyFastWakeupType_str((int)g_dummy_settings.fast_wakeup)); });

    // Bool dummy settings (interactive via checkbox)
    ctor.BindFunc(
        "dummy_wakeup_reversal",
        [](Rml::Variant& v) { v = g_dummy_settings.wakeup_reversal; },
        [](const Rml::Variant& v) {
            g_dummy_settings.wakeup_reversal = v.Get<bool>();
            Config_SetBool(CFG_KEY_DUMMY_WAKEUP_REVERSAL, g_dummy_settings.wakeup_reversal);
            Config_Save();
        });
    ctor.BindFunc(
        "dummy_guard_low",
        [](Rml::Variant& v) { v = g_dummy_settings.guard_low_default; },
        [](const Rml::Variant& v) {
            g_dummy_settings.guard_low_default = v.Get<bool>();
            Config_SetBool(CFG_KEY_DUMMY_GUARD_LOW, g_dummy_settings.guard_low_default);
            Config_Save();
        });

    // ═══════════════════════════════════════════════════════════════
    // DISPLAY TAB — boolean bindings (unchanged from original)
    // ═══════════════════════════════════════════════════════════════
#define BIND_DISPLAY_BOOL(name, field, cfg_key)                                                                        \
    ctor.BindFunc(                                                                                                     \
        name,                                                                                                          \
        [](Rml::Variant& v) { v = g_training_menu_settings.field; },                                                   \
        [](const Rml::Variant& v) {                                                                                    \
            g_training_menu_settings.field = v.Get<bool>();                                                            \
            Config_SetBool(cfg_key, g_training_menu_settings.field);                                                   \
            Config_Save();                                                                                             \
        })

    BIND_DISPLAY_BOOL("hitboxes", show_hitboxes, CFG_KEY_TRAINING_HITBOXES);
    BIND_DISPLAY_BOOL("pushboxes", show_pushboxes, CFG_KEY_TRAINING_PUSHBOXES);
    BIND_DISPLAY_BOOL("hurtboxes", show_hurtboxes, CFG_KEY_TRAINING_HURTBOXES);
    BIND_DISPLAY_BOOL("attackboxes", show_attackboxes, CFG_KEY_TRAINING_ATTACKBOXES);
    BIND_DISPLAY_BOOL("throwboxes", show_throwboxes, CFG_KEY_TRAINING_THROWBOXES);
    BIND_DISPLAY_BOOL("advantage", show_advantage, CFG_KEY_TRAINING_ADVANTAGE);
    BIND_DISPLAY_BOOL("stun", show_stun, CFG_KEY_TRAINING_STUN);
    BIND_DISPLAY_BOOL("inputs", show_inputs, CFG_KEY_TRAINING_INPUTS);
    BIND_DISPLAY_BOOL("frame_meter", show_frame_meter, CFG_KEY_TRAINING_FRAME_METER);
#undef BIND_DISPLAY_BOOL

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    // Attach click-to-cycle listeners after document is loaded
    attach_cycle_listeners();

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi Training] Data model registered (9 display + 8 dummy bindings)");
}

// -------------------------------------------------------------------
// Per-frame update — dirty check
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;

    // Display booleans
#define DIRTY_BOOL(name, field)                                                                                        \
    do {                                                                                                               \
        bool _cur = g_training_menu_settings.field;                                                                    \
        if (_cur != s_cache.name) {                                                                                    \
            s_cache.name = _cur;                                                                                       \
            s_model_handle.DirtyVariable(#name);                                                                       \
        }                                                                                                              \
    } while (0)

    DIRTY_BOOL(hitboxes, show_hitboxes);
    DIRTY_BOOL(pushboxes, show_pushboxes);
    DIRTY_BOOL(hurtboxes, show_hurtboxes);
    DIRTY_BOOL(attackboxes, show_attackboxes);
    DIRTY_BOOL(throwboxes, show_throwboxes);
    DIRTY_BOOL(advantage, show_advantage);
    DIRTY_BOOL(stun, show_stun);
    DIRTY_BOOL(inputs, show_inputs);
    DIRTY_BOOL(frame_meter, show_frame_meter);
#undef DIRTY_BOOL

    // Dummy enums — dirty both the value and the label
#define DIRTY_ENUM(cache_field, src_expr, label_name)                                                                  \
    do {                                                                                                               \
        int _cur = (int)(src_expr);                                                                                    \
        if (_cur != s_cache.cache_field) {                                                                             \
            s_cache.cache_field = _cur;                                                                                \
            s_model_handle.DirtyVariable(label_name);                                                                  \
        }                                                                                                              \
    } while (0)

    DIRTY_ENUM(block_type, g_dummy_settings.block_type, "dummy_block_label");
    DIRTY_ENUM(parry_type, g_dummy_settings.parry_type, "dummy_parry_label");
    DIRTY_ENUM(stun_mash, g_dummy_settings.stun_mash, "dummy_stun_mash_label");
    DIRTY_ENUM(wakeup_mash, g_dummy_settings.wakeup_mash, "dummy_wakeup_mash_label");
    DIRTY_ENUM(tech_throw, g_dummy_settings.tech_throw_type, "dummy_tech_throw_label");
    DIRTY_ENUM(fast_wakeup, g_dummy_settings.fast_wakeup, "dummy_fast_wakeup_label");
#undef DIRTY_ENUM

    // Dummy bools
    if (g_dummy_settings.wakeup_reversal != s_cache.wakeup_reversal) {
        s_cache.wakeup_reversal = g_dummy_settings.wakeup_reversal;
        s_model_handle.DirtyVariable("dummy_wakeup_reversal");
    }
    if (g_dummy_settings.guard_low_default != s_cache.guard_low) {
        s_cache.guard_low = g_dummy_settings.guard_low_default;
        s_model_handle.DirtyVariable("dummy_guard_low");
    }

    // Tab
    if (s_active_tab != s_cache.active_tab) {
        s_cache.active_tab = s_active_tab;
        s_model_handle.DirtyVariable("active_tab");
    }
}

// -------------------------------------------------------------------
// Shutdown
// -------------------------------------------------------------------
extern "C" void rmlui_training_menu_shutdown(void) {
    if (s_model_registered) {
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_context());
        if (ctx)
            ctx->RemoveDataModel("training");
        s_model_registered = false;
    }
    SDL_Log("[RmlUi Training] Shut down");
}
