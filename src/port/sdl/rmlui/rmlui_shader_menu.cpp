/**
 * @file rmlui_shader_menu.cpp
 * @brief RmlUi shader configuration menu — data model + update logic.
 *
 * Binds shader mode, scale mode, and preset list to the RmlUi
 * "shaders" data model.  The preset list is filtered on the C++ side
 * to avoid iterating 2000+ items in the DOM.
 */
#include "port/sdl/rmlui/rmlui_shader_menu.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

// ── C externs ──────────────────────────────────────────────────

extern "C" {
#include "port/sdl/app/sdl_app_shader_config.h"

int SDLApp_GetScaleMode();
void SDLApp_SetScaleMode(int mode);
const char* SDLApp_GetScaleModeName(int mode);
}

// ── Filtered Preset struct ─────────────────────────────────────

struct FilteredPreset {
    Rml::String name;
    int index;
};

// ── Chain Pass struct ──────────────────────────────────────────

struct ChainPass {
    Rml::String name;   // Shader filename (basename of path)
    Rml::String source; // Source preset basename
    int index;          // Pass index in the chain (-1 = sentinel/hidden)
};

// ── Shader Parameter struct ────────────────────────────────────

struct ShaderParam {
    Rml::String name;
    Rml::String desc;
    Rml::String value_str; // Formatted value for display
    float value;
    float initial;
    float min_val;
    float max_val;
    float step;
    int index; // Index into the param array (-1 = sentinel/hidden)
};

// ── Module state ───────────────────────────────────────────────

static Rml::DataModelHandle s_model_handle;
static std::vector<FilteredPreset> s_filtered_presets;
static Rml::String s_search_filter;
static bool s_filter_dirty = true;

static constexpr int CHAIN_PASSES_MAX = 32;
static std::vector<ChainPass> s_chain_passes;
static int s_chain_pass_count = 0;
static bool s_chain_dirty = false; // Set by event callbacks, consumed by per-frame update

static constexpr int SHADER_PARAMS_MAX = 64;
static std::vector<ShaderParam> s_shader_params;
static int s_shader_param_count = 0;
static bool s_params_dirty = false; // Set when chain/preset changes

// Snapshot for dirty-checking
static struct {
    bool is_libretro;
    int scale_mode;
    int current_preset;
    int preset_count;
    int chain_pass_count;
    int shader_param_count;
} s_prev;

// ── Helpers ────────────────────────────────────────────────────

static Rml::String to_lower(const Rml::String& s) {
    Rml::String out = s;
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return out;
}

static constexpr int FILTERED_PRESETS_MAX = 50;
static int s_filtered_count = 0;

static void rebuild_filtered_presets() {
    /* Build into a temp list first. */
    int match_count = 0;
    int count = SDLAppShader_GetAvailableCount();
    Rml::String filter_lower = to_lower(s_search_filter);

    /* Ensure the vector is always exactly FILTERED_PRESETS_MAX entries.
     * RmlUi's data-for creates DOM nodes based on vector size; if the
     * size changes, stale nodes evaluate out-of-bounds indices before
     * the DOM is reconciled.  Keeping a fixed size avoids this. */
    s_filtered_presets.resize(FILTERED_PRESETS_MAX);

    for (int i = 0; i < count && match_count < FILTERED_PRESETS_MAX; i++) {
        const char* name = SDLAppShader_GetPresetName(i);
        if (!name)
            continue;

        if (!filter_lower.empty()) {
            Rml::String name_lower = to_lower(name);
            if (name_lower.find(filter_lower) == Rml::String::npos)
                continue;
        }
        s_filtered_presets[match_count] = { name, i };
        match_count++;
    }

    /* Fill remaining slots with sentinel (index = -1, hidden in template). */
    for (int i = match_count; i < FILTERED_PRESETS_MAX; i++) {
        s_filtered_presets[i] = { Rml::String(), -1 };
    }

    s_filtered_count = match_count;
    s_filter_dirty = false;
}

static Rml::String extract_basename(const char* path) {
    if (!path || !path[0])
        return Rml::String();
    const char* slash = strrchr(path, '/');
    const char* bslash = strrchr(path, '\\');
    const char* last = nullptr;
    if (slash && bslash)
        last = (slash > bslash) ? slash : bslash;
    else if (slash)
        last = slash;
    else
        last = bslash;
    return Rml::String(last ? last + 1 : path);
}

static void rebuild_chain_passes() {
    int count = SDLAppShader_ChainGetPassCount();
    s_chain_passes.resize(CHAIN_PASSES_MAX);

    for (int i = 0; i < count && i < CHAIN_PASSES_MAX; i++) {
        const char* shader_path = SDLAppShader_ChainGetPassShaderPath(i);
        const char* source_preset = SDLAppShader_ChainGetPassSourcePreset(i);
        s_chain_passes[i] = { extract_basename(shader_path), extract_basename(source_preset), i };
    }

    // Fill remaining with sentinels
    for (int i = count; i < CHAIN_PASSES_MAX; i++) {
        s_chain_passes[i] = { Rml::String(), Rml::String(), -1 };
    }

    s_chain_pass_count = count;
}

static void rebuild_shader_params() {
    int count = SDLAppShader_GetParamCount();
    s_shader_params.resize(SHADER_PARAMS_MAX);

    int visible = (count < SHADER_PARAMS_MAX) ? count : SHADER_PARAMS_MAX;
    for (int i = 0; i < visible; i++) {
        const char* name = SDLAppShader_GetParamName(i);
        const char* desc = SDLAppShader_GetParamDesc(i);
        float val = SDLAppShader_GetParamValue(i);
        char val_buf[32];
        snprintf(val_buf, sizeof(val_buf), "%.2f", val);
        s_shader_params[i] = { Rml::String(name ? name : ""),
                               Rml::String(desc ? desc : ""),
                               Rml::String(val_buf),
                               val,
                               SDLAppShader_GetParamInitial(i),
                               SDLAppShader_GetParamMin(i),
                               SDLAppShader_GetParamMax(i),
                               SDLAppShader_GetParamStep(i),
                               i };
    }

    // Fill remaining with sentinels
    for (int i = visible; i < SHADER_PARAMS_MAX; i++) {
        s_shader_params[i] = { Rml::String(), Rml::String(), Rml::String(), 0, 0, 0, 0, 0, -1 };
    }

    s_shader_param_count = visible;
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
        handle.RegisterMember("name", &FilteredPreset::name);
        handle.RegisterMember("index", &FilteredPreset::index);
    }
    constructor.RegisterArray<std::vector<FilteredPreset>>();

    // ── Register ChainPass struct + array ──
    if (auto handle = constructor.RegisterStruct<ChainPass>()) {
        handle.RegisterMember("name", &ChainPass::name);
        handle.RegisterMember("source", &ChainPass::source);
        handle.RegisterMember("index", &ChainPass::index);
    }
    constructor.RegisterArray<std::vector<ChainPass>>();

    // ── Register ShaderParam struct + array ──
    if (auto handle = constructor.RegisterStruct<ShaderParam>()) {
        handle.RegisterMember("name", &ShaderParam::name);
        handle.RegisterMember("desc", &ShaderParam::desc);
        handle.RegisterMember("value_str", &ShaderParam::value_str);
        handle.RegisterMember("value", &ShaderParam::value);
        handle.RegisterMember("initial", &ShaderParam::initial);
        handle.RegisterMember("min_val", &ShaderParam::min_val);
        handle.RegisterMember("max_val", &ShaderParam::max_val);
        handle.RegisterMember("step", &ShaderParam::step);
        handle.RegisterMember("index", &ShaderParam::index);
    }
    constructor.RegisterArray<std::vector<ShaderParam>>();

    // ── Bind the filtered preset vector + chain passes ──
    constructor.Bind("filtered_presets", &s_filtered_presets);
    constructor.Bind("chain_passes", &s_chain_passes);
    constructor.Bind("shader_params", &s_shader_params);

    constructor.BindFunc("chain_pass_count", [](Rml::Variant& v) { v = SDLAppShader_ChainGetPassCount(); });
    constructor.BindFunc("shader_param_count", [](Rml::Variant& v) { v = SDLAppShader_GetParamCount(); });

    // ── Scalar BindFunc bindings ──

    constructor.BindFunc(
        "is_libretro",
        [](Rml::Variant& v) { v = SDLAppShader_IsLibretroMode(); },
        [](const Rml::Variant& v) { SDLAppShader_SetMode(v.Get<bool>()); });

    constructor.BindFunc(
        "scale_mode",
        [](Rml::Variant& v) { v = SDLApp_GetScaleMode(); },
        [](const Rml::Variant& v) { SDLApp_SetScaleMode(v.Get<int>()); });

    // Scale mode names (read-only)
    for (int i = 0; i < 5; i++) {
        Rml::String var_name = "scale_mode_name_" + Rml::ToString(i);
        constructor.BindFunc(var_name, [i](Rml::Variant& v) {
            const char* name = SDLApp_GetScaleModeName(i);
            v = Rml::String(name ? name : "");
        });
    }

    constructor.BindFunc("preset_count", [](Rml::Variant& v) { v = SDLAppShader_GetAvailableCount(); });

    constructor.BindFunc(
        "current_preset",
        [](Rml::Variant& v) { v = SDLAppShader_GetCurrentIndex(); },
        [](const Rml::Variant& v) {
            int idx = v.Get<int>();
            SDLAppShader_SetCurrentIndex(idx);
            SDLAppShader_LoadPreset(idx);
        });

    constructor.BindFunc(
        "search_filter",
        [](Rml::Variant& v) { v = s_search_filter; },
        [](const Rml::Variant& v) {
            Rml::String new_val = v.Get<Rml::String>();
            if (new_val != s_search_filter) {
                s_search_filter = new_val;
                rebuild_filtered_presets();
                if (s_model_handle) {
                    s_model_handle.DirtyVariable("filtered_presets");
                }
            }
        });

    // ── Event callbacks ──

    constructor.BindEventCallback("select_preset",
                                  [](Rml::DataModelHandle handle, Rml::Event& /*event*/, const Rml::VariantList& args) {
                                      if (args.empty())
                                          return;
                                      int idx = args[0].Get<int>();
                                      SDLAppShader_SetCurrentIndex(idx);
                                      // Initialize chain with this preset — chain apply handles shader loading
                                      SDLAppShader_ChainClear();
                                      SDLAppShader_ChainAppend(idx);
                                      s_chain_dirty = true;
                                      handle.DirtyVariable("current_preset");
                                  });

    // ── Chain event callbacks ──

    constructor.BindEventCallback(
        "chain_append", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            SDLAppShader_ChainAppend(args[0].Get<int>());
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_prepend", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            SDLAppShader_ChainPrepend(args[0].Get<int>());
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_remove_pass", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            SDLAppShader_ChainRemovePass(args[0].Get<int>());
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_move_up", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            int idx = args[0].Get<int>();
            if (idx > 0)
                SDLAppShader_ChainMovePass(idx, idx - 1);
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_move_down", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            int idx = args[0].Get<int>();
            if (idx < SDLAppShader_ChainGetPassCount() - 1)
                SDLAppShader_ChainMovePass(idx, idx + 1);
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_clear", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& /*args*/) {
            SDLAppShader_ChainClear();
            s_chain_dirty = true;
        });

    constructor.BindEventCallback(
        "chain_save", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& /*args*/) {
            // Generate a timestamped filename
            char filename[256];
            Uint64 ticks = SDL_GetTicks();
            snprintf(filename, sizeof(filename), "chain_%llu.slangp", (unsigned long long)ticks);

            // Build full path in the libretro shaders directory
            char path[1024];
            const char* base = SDL_GetBasePath();
            snprintf(path, sizeof(path), "%sshaders/libretro/%s", base ? base : "", filename);

            if (SDLAppShader_ChainSaveAsPreset(path)) {
                SDL_Log("[Shader Chain] Saved as '%s'", path);
            } else {
                SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "[Shader Chain] Failed to save '%s'", path);
            }
        });

    // ── Shader parameter event callbacks ──

    constructor.BindEventCallback(
        "param_dec", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            int idx = args[0].Get<int>();
            float val = SDLAppShader_GetParamValue(idx);
            float step = SDLAppShader_GetParamStep(idx);
            float min_v = SDLAppShader_GetParamMin(idx);
            float new_val = val - step;
            if (new_val < min_v)
                new_val = min_v;
            SDLAppShader_SetParamValue(idx, new_val);
            s_params_dirty = true;
        });

    constructor.BindEventCallback(
        "param_inc", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            int idx = args[0].Get<int>();
            float val = SDLAppShader_GetParamValue(idx);
            float step = SDLAppShader_GetParamStep(idx);
            float max_v = SDLAppShader_GetParamMax(idx);
            float new_val = val + step;
            if (new_val > max_v)
                new_val = max_v;
            SDLAppShader_SetParamValue(idx, new_val);
            s_params_dirty = true;
        });

    constructor.BindEventCallback(
        "param_reset", [](Rml::DataModelHandle /*handle*/, Rml::Event& /*event*/, const Rml::VariantList& args) {
            if (args.empty())
                return;
            int idx = args[0].Get<int>();
            SDLAppShader_ResetParam(idx);
            s_params_dirty = true;
        });

    s_model_handle = constructor.GetModelHandle();

    // Initial builds
    rebuild_filtered_presets();
    rebuild_chain_passes();
    rebuild_shader_params();

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi Shaders] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────

extern "C" void rmlui_shader_menu_update() {
    if (!s_model_handle)
        return;

    bool dirty = false;

    bool is_libretro = SDLAppShader_IsLibretroMode();
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

    int current_preset = SDLAppShader_GetCurrentIndex();
    if (current_preset != s_prev.current_preset) {
        s_prev.current_preset = current_preset;
        s_model_handle.DirtyVariable("current_preset");
        dirty = true;
    }

    int preset_count = SDLAppShader_GetAvailableCount();
    if (preset_count != s_prev.preset_count) {
        s_prev.preset_count = preset_count;
        s_model_handle.DirtyVariable("preset_count");
        s_filter_dirty = true;
        dirty = true;
    }

    // Rebuild filtered presets when filter or source data changes
    if (s_filter_dirty) {
        rebuild_filtered_presets();
        s_model_handle.DirtyVariable("filtered_presets");
        dirty = true;
    }

    // Chain dirty-check: count change OR explicit dirty flag from event callbacks
    int chain_pass_count = SDLAppShader_ChainGetPassCount();
    if (chain_pass_count != s_prev.chain_pass_count || s_chain_dirty) {
        s_prev.chain_pass_count = chain_pass_count;
        s_chain_dirty = false;
        rebuild_chain_passes();
        s_model_handle.DirtyVariable("chain_passes");
        s_model_handle.DirtyVariable("chain_pass_count");
        s_params_dirty = true; // Chain changed — params may have changed too
        dirty = true;
    }

    // Shader parameters dirty-check
    int param_count = SDLAppShader_GetParamCount();
    if (param_count != s_prev.shader_param_count || s_params_dirty) {
        s_prev.shader_param_count = param_count;
        s_params_dirty = false;
        rebuild_shader_params();
        s_model_handle.DirtyVariable("shader_params");
        s_model_handle.DirtyVariable("shader_param_count");
        dirty = true;
    }

    (void)dirty;
}

// ── Shutdown ───────────────────────────────────────────────────

extern "C" void rmlui_shader_menu_shutdown() {
    s_model_handle = Rml::DataModelHandle();
    s_filtered_presets.clear();
    s_chain_passes.clear();
    s_shader_params.clear();
    s_search_filter.clear();
}
