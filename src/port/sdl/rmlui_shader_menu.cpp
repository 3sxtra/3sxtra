/**
 * @file rmlui_shader_menu.cpp
 * @brief RmlUi shader configuration menu — data model + update logic.
 *
 * Binds shader mode, scale mode, preset list, VSync, and broadcast
 * settings to the RmlUi "shaders" data model.  The preset list is
 * filtered on the C++ side to avoid iterating 2000+ items in the DOM.
 */
#include "port/sdl/rmlui_shader_menu.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// ── C externs ──────────────────────────────────────────────────

extern "C" {
#include "include/port/broadcast.h"
extern BroadcastConfig broadcast_config;

int  SDLApp_GetScaleMode();
void SDLApp_SetScaleMode(int mode);
const char* SDLApp_GetScaleModeName(int mode);

bool SDLApp_GetShaderModeLibretro();
void SDLApp_SetShaderModeLibretro(bool libretro);

int  SDLApp_GetCurrentPresetIndex();
void SDLApp_SetCurrentPresetIndex(int index);
int  SDLApp_GetAvailablePresetCount();
const char* SDLApp_GetPresetName(int index);
void SDLApp_LoadPreset(int index);

void SDLApp_SetVSync(bool enabled);
bool SDLApp_IsVSyncEnabled();
}

// ── Filtered Preset struct ─────────────────────────────────────

struct FilteredPreset {
    Rml::String name;
    int         index;
};

// ── Module state ───────────────────────────────────────────────

static Rml::DataModelHandle s_model_handle;
static std::vector<FilteredPreset> s_filtered_presets;
static Rml::String s_search_filter;
static bool s_filter_dirty = true;

// Snapshot for dirty-checking
static struct {
    bool  is_libretro;
    int   scale_mode;
    int   current_preset;
    int   preset_count;
    bool  vsync;
    bool  broadcast_enabled;
    int   broadcast_source;
} s_prev;

// ── Helpers ────────────────────────────────────────────────────

static Rml::String to_lower(const Rml::String& s) {
    Rml::String out = s;
    std::transform(out.begin(), out.end(), out.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

static void rebuild_filtered_presets() {
    s_filtered_presets.clear();
    int count = SDLApp_GetAvailablePresetCount();
    Rml::String filter_lower = to_lower(s_search_filter);

    for (int i = 0; i < count; i++) {
        const char* name = SDLApp_GetPresetName(i);
        if (!name) continue;

        if (!filter_lower.empty()) {
            Rml::String name_lower = to_lower(name);
            if (name_lower.find(filter_lower) == Rml::String::npos)
                continue;
        }
        s_filtered_presets.push_back({name, i});
    }
    s_filter_dirty = false;
}

// ── Init ───────────────────────────────────────────────────────

extern "C" void rmlui_shader_menu_init() {
    void* raw_ctx = rmlui_wrapper_get_context();
    if (!raw_ctx) {
        SDL_Log("[RmlUi Shaders] No context available");
        return;
    }
    Rml::Context* ctx = static_cast<Rml::Context*>(raw_ctx);

    Rml::DataModelConstructor constructor = ctx->CreateDataModel("shaders");
    if (!constructor) {
        SDL_Log("[RmlUi Shaders] Failed to create data model");
        return;
    }

    // ── Register FilteredPreset struct + array ──
    if (auto handle = constructor.RegisterStruct<FilteredPreset>()) {
        handle.RegisterMember("name",  &FilteredPreset::name);
        handle.RegisterMember("index", &FilteredPreset::index);
    }
    constructor.RegisterArray<std::vector<FilteredPreset>>();

    // ── Bind the filtered preset vector ──
    constructor.Bind("filtered_presets", &s_filtered_presets);

    // ── Scalar BindFunc bindings ──

    constructor.BindFunc("is_libretro",
        [](Rml::Variant& v) { v = SDLApp_GetShaderModeLibretro(); },
        [](const Rml::Variant& v) { SDLApp_SetShaderModeLibretro(v.Get<bool>()); }
    );

    constructor.BindFunc("scale_mode",
        [](Rml::Variant& v) { v = SDLApp_GetScaleMode(); },
        [](const Rml::Variant& v) { SDLApp_SetScaleMode(v.Get<int>()); }
    );

    // Scale mode names (read-only)
    for (int i = 0; i < 5; i++) {
        Rml::String var_name = "scale_mode_name_" + Rml::ToString(i);
        constructor.BindFunc(var_name,
            [i](Rml::Variant& v) {
                const char* name = SDLApp_GetScaleModeName(i);
                v = Rml::String(name ? name : "");
            }
        );
    }

    constructor.BindFunc("preset_count",
        [](Rml::Variant& v) { v = SDLApp_GetAvailablePresetCount(); }
    );

    constructor.BindFunc("current_preset",
        [](Rml::Variant& v) { v = SDLApp_GetCurrentPresetIndex(); },
        [](const Rml::Variant& v) {
            int idx = v.Get<int>();
            SDLApp_SetCurrentPresetIndex(idx);
            SDLApp_LoadPreset(idx);
        }
    );

    constructor.BindFunc("search_filter",
        [](Rml::Variant& v) { v = s_search_filter; },
        [](const Rml::Variant& v) {
            Rml::String new_val = v.Get<Rml::String>();
            if (new_val != s_search_filter) {
                s_search_filter = new_val;
                s_filter_dirty = true;
            }
        }
    );

    constructor.BindFunc("vsync",
        [](Rml::Variant& v) { v = SDLApp_IsVSyncEnabled(); },
        [](const Rml::Variant& v) { SDLApp_SetVSync(v.Get<bool>()); }
    );

    constructor.BindFunc("broadcast_enabled",
        [](Rml::Variant& v) { v = broadcast_config.enabled; },
        [](const Rml::Variant& v) { broadcast_config.enabled = v.Get<bool>(); }
    );

    constructor.BindFunc("broadcast_source",
        [](Rml::Variant& v) { v = (int)broadcast_config.source; },
        [](const Rml::Variant& v) { broadcast_config.source = (BroadcastSource)v.Get<int>(); }
    );

    // ── Event callbacks ──

    constructor.BindEventCallback("select_preset",
        [](Rml::DataModelHandle handle, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty()) return;
            int idx = args[0].Get<int>();
            SDLApp_SetCurrentPresetIndex(idx);
            SDLApp_LoadPreset(idx);
            handle.DirtyVariable("current_preset");
        }
    );

    s_model_handle = constructor.GetModelHandle();

    // Initial preset list build
    rebuild_filtered_presets();

    SDL_Log("[RmlUi Shaders] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────

extern "C" void rmlui_shader_menu_update() {
    if (!s_model_handle) return;

    bool dirty = false;

    bool is_libretro = SDLApp_GetShaderModeLibretro();
    if (is_libretro != s_prev.is_libretro) {
        s_prev.is_libretro = is_libretro;
        s_model_handle.DirtyVariable("is_libretro");
        dirty = true;
    }

    int scale_mode = SDLApp_GetScaleMode();
    if (scale_mode != s_prev.scale_mode) {
        s_prev.scale_mode = scale_mode;
        s_model_handle.DirtyVariable("scale_mode");
        dirty = true;
    }

    int current_preset = SDLApp_GetCurrentPresetIndex();
    if (current_preset != s_prev.current_preset) {
        s_prev.current_preset = current_preset;
        s_model_handle.DirtyVariable("current_preset");
        dirty = true;
    }

    int preset_count = SDLApp_GetAvailablePresetCount();
    if (preset_count != s_prev.preset_count) {
        s_prev.preset_count = preset_count;
        s_model_handle.DirtyVariable("preset_count");
        s_filter_dirty = true;
        dirty = true;
    }

    bool vsync = SDLApp_IsVSyncEnabled();
    if (vsync != s_prev.vsync) {
        s_prev.vsync = vsync;
        s_model_handle.DirtyVariable("vsync");
        dirty = true;
    }

    bool broadcast_enabled = broadcast_config.enabled;
    if (broadcast_enabled != s_prev.broadcast_enabled) {
        s_prev.broadcast_enabled = broadcast_enabled;
        s_model_handle.DirtyVariable("broadcast_enabled");
        dirty = true;
    }

    int broadcast_source = (int)broadcast_config.source;
    if (broadcast_source != s_prev.broadcast_source) {
        s_prev.broadcast_source = broadcast_source;
        s_model_handle.DirtyVariable("broadcast_source");
        dirty = true;
    }

    // Rebuild filtered presets when filter or source data changes
    if (s_filter_dirty) {
        rebuild_filtered_presets();
        s_model_handle.DirtyVariable("filtered_presets");
        dirty = true;
    }

    (void)dirty;
}

// ── Shutdown ───────────────────────────────────────────────────

extern "C" void rmlui_shader_menu_shutdown() {
    s_model_handle = Rml::DataModelHandle();
    s_filtered_presets.clear();
    s_search_filter.clear();
}
