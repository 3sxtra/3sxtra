/**
 * @file rmlui_stage_config.cpp
 * @brief RmlUi HD stage configuration menu — data model + update logic.
 *
 * Binds the per-layer stage editing UI to the RmlUi "stage_config" data
 * model.  Uses a `selected_layer` index to expose only the active layer's
 * properties, with event callbacks for save/load/reset and tab switching.
 */
#include "port/sdl/rmlui_stage_config.h"
#include "port/sdl/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

#include <cstdio>
#include <string>

// ── C externs ──────────────────────────────────────────────────

extern "C" {
#include "port/modded_stage.h"
#include "port/stage_config.h"
}

// ── Module state ───────────────────────────────────────────────

static Rml::DataModelHandle s_model_handle;
static int s_selected_layer = 0;

// Snapshot for dirty-checking
static struct {
    int stage_idx;
    int selected_layer;
    // Per-layer snapshots (active layer only)
    bool enabled;
    char filename[64];
    int scale_mode;
    float scale_factor_x, scale_factor_y;
    float parallax_x, parallax_y;
    float offset_x, offset_y;
    int original_bg_index;
    int z_index;
    bool loop_x, loop_y;
} s_prev;

// ── Helpers ────────────────────────────────────────────────────

static StageLayerConfig* active_layer() {
    if (s_selected_layer < 0 || s_selected_layer >= MAX_STAGE_LAYERS)
        s_selected_layer = 0;
    return &g_stage_config.layers[s_selected_layer];
}

static void dirty_all_layer_vars() {
    s_model_handle.DirtyVariable("layer_enabled");
    s_model_handle.DirtyVariable("layer_filename");
    s_model_handle.DirtyVariable("layer_scale_mode");
    s_model_handle.DirtyVariable("layer_scale_x");
    s_model_handle.DirtyVariable("layer_scale_y");
    s_model_handle.DirtyVariable("layer_parallax_x");
    s_model_handle.DirtyVariable("layer_parallax_y");
    s_model_handle.DirtyVariable("layer_offset_x");
    s_model_handle.DirtyVariable("layer_offset_y");
    s_model_handle.DirtyVariable("layer_bg_index");
    s_model_handle.DirtyVariable("layer_z_index");
    s_model_handle.DirtyVariable("layer_loop_x");
    s_model_handle.DirtyVariable("layer_loop_y");
}

static void snapshot_active_layer() {
    StageLayerConfig* L = active_layer();
    s_prev.selected_layer = s_selected_layer;
    s_prev.enabled = L->enabled;
    snprintf(s_prev.filename, sizeof(s_prev.filename), "%s", L->filename);
    s_prev.scale_mode = (int)L->scale_mode;
    s_prev.scale_factor_x = L->scale_factor_x;
    s_prev.scale_factor_y = L->scale_factor_y;
    s_prev.parallax_x = L->parallax_x;
    s_prev.parallax_y = L->parallax_y;
    s_prev.offset_x = L->offset_x;
    s_prev.offset_y = L->offset_y;
    s_prev.original_bg_index = L->original_bg_index;
    s_prev.z_index = L->z_index;
    s_prev.loop_x = L->loop_x;
    s_prev.loop_y = L->loop_y;
}

// ── Init ───────────────────────────────────────────────────────

extern "C" void rmlui_stage_config_init() {
    void* raw_ctx = rmlui_wrapper_get_context();
    if (!raw_ctx) {
        SDL_Log("[RmlUi StageConfig] No context available");
        return;
    }
    Rml::Context* ctx = static_cast<Rml::Context*>(raw_ctx);

    Rml::DataModelConstructor c = ctx->CreateDataModel("stage_config");
    if (!c) {
        SDL_Log("[RmlUi StageConfig] Failed to create data model");
        return;
    }

    // ── Stage index (read-only) ──
    c.BindFunc("stage_idx", [](Rml::Variant& v) { v = ModdedStage_GetLoadedStageIndex(); });

    c.BindFunc("has_stage", [](Rml::Variant& v) { v = (ModdedStage_GetLoadedStageIndex() >= 0); });

    // ── Selected tab ──
    c.BindFunc(
        "selected_layer",
        [](Rml::Variant& v) { v = s_selected_layer; },
        [](const Rml::Variant& v) { s_selected_layer = v.Get<int>(); });

    // ── Active layer properties (BindFunc — read/write active layer) ──

    c.BindFunc(
        "layer_enabled",
        [](Rml::Variant& v) { v = active_layer()->enabled; },
        [](const Rml::Variant& v) { active_layer()->enabled = v.Get<bool>(); });

    c.BindFunc(
        "layer_filename",
        [](Rml::Variant& v) { v = Rml::String(active_layer()->filename); },
        [](const Rml::Variant& v) {
            Rml::String s = v.Get<Rml::String>();
            snprintf(active_layer()->filename, sizeof(active_layer()->filename), "%s", s.c_str());
        });

    c.BindFunc(
        "layer_scale_mode",
        [](Rml::Variant& v) { v = (int)active_layer()->scale_mode; },
        [](const Rml::Variant& v) { active_layer()->scale_mode = (LayerScaleMode)v.Get<int>(); });

    c.BindFunc("is_manual_scale", [](Rml::Variant& v) { v = (active_layer()->scale_mode == SCALE_MODE_MANUAL); });

    c.BindFunc("is_fit_height", [](Rml::Variant& v) { v = (active_layer()->scale_mode == SCALE_MODE_FIT_HEIGHT); });

    c.BindFunc(
        "layer_scale_x",
        [](Rml::Variant& v) { v = active_layer()->scale_factor_x; },
        [](const Rml::Variant& v) { active_layer()->scale_factor_x = v.Get<float>(); });

    c.BindFunc(
        "layer_scale_y",
        [](Rml::Variant& v) { v = active_layer()->scale_factor_y; },
        [](const Rml::Variant& v) { active_layer()->scale_factor_y = v.Get<float>(); });

    c.BindFunc(
        "layer_parallax_x",
        [](Rml::Variant& v) { v = active_layer()->parallax_x; },
        [](const Rml::Variant& v) { active_layer()->parallax_x = v.Get<float>(); });

    c.BindFunc(
        "layer_parallax_y",
        [](Rml::Variant& v) { v = active_layer()->parallax_y; },
        [](const Rml::Variant& v) { active_layer()->parallax_y = v.Get<float>(); });

    c.BindFunc(
        "layer_offset_x",
        [](Rml::Variant& v) { v = active_layer()->offset_x; },
        [](const Rml::Variant& v) { active_layer()->offset_x = v.Get<float>(); });

    c.BindFunc(
        "layer_offset_y",
        [](Rml::Variant& v) { v = active_layer()->offset_y; },
        [](const Rml::Variant& v) { active_layer()->offset_y = v.Get<float>(); });

    c.BindFunc(
        "layer_bg_index",
        [](Rml::Variant& v) { v = active_layer()->original_bg_index; },
        [](const Rml::Variant& v) { active_layer()->original_bg_index = v.Get<int>(); });

    c.BindFunc(
        "layer_z_index",
        [](Rml::Variant& v) { v = active_layer()->z_index; },
        [](const Rml::Variant& v) { active_layer()->z_index = v.Get<int>(); });

    c.BindFunc(
        "layer_loop_x",
        [](Rml::Variant& v) { v = active_layer()->loop_x; },
        [](const Rml::Variant& v) { active_layer()->loop_x = v.Get<bool>(); });

    c.BindFunc(
        "layer_loop_y",
        [](Rml::Variant& v) { v = active_layer()->loop_y; },
        [](const Rml::Variant& v) { active_layer()->loop_y = v.Get<bool>(); });

    // ── Event callbacks ──

    c.BindEventCallback("save_config", [](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList&) {
        int idx = ModdedStage_GetLoadedStageIndex();
        if (idx >= 0) {
            StageConfig_Save(idx);
            SDL_Log("[RmlUi StageConfig] Saved config for stage %02d", idx);
        }
    });

    c.BindEventCallback("load_config", [](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList&) {
        int idx = ModdedStage_GetLoadedStageIndex();
        if (idx >= 0) {
            StageConfig_Load(idx);
            dirty_all_layer_vars();
            SDL_Log("[RmlUi StageConfig] Reloaded config for stage %02d", idx);
        }
    });

    c.BindEventCallback("reset_config", [](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList&) {
        StageConfig_Init();
        dirty_all_layer_vars();
        SDL_Log("[RmlUi StageConfig] Reset to defaults");
    });

    c.BindEventCallback("select_tab", [](Rml::DataModelHandle handle, Rml::Event&, const Rml::VariantList& args) {
        if (args.empty())
            return;
        s_selected_layer = args[0].Get<int>();
        if (s_selected_layer < 0)
            s_selected_layer = 0;
        if (s_selected_layer >= MAX_STAGE_LAYERS)
            s_selected_layer = MAX_STAGE_LAYERS - 1;
        handle.DirtyVariable("selected_layer");
        dirty_all_layer_vars();
    });

    s_model_handle = c.GetModelHandle();
    snapshot_active_layer();

    SDL_Log("[RmlUi StageConfig] Data model registered");
}

// ── Per-frame update ───────────────────────────────────────────

extern "C" void rmlui_stage_config_update() {
    if (!s_model_handle)
        return;

    int stage_idx = ModdedStage_GetLoadedStageIndex();
    if (stage_idx != s_prev.stage_idx) {
        s_prev.stage_idx = stage_idx;
        s_model_handle.DirtyVariable("stage_idx");
        s_model_handle.DirtyVariable("has_stage");
        dirty_all_layer_vars();
        snapshot_active_layer();
        return;
    }

    if (s_selected_layer != s_prev.selected_layer) {
        snapshot_active_layer();
        dirty_all_layer_vars();
        s_model_handle.DirtyVariable("selected_layer");
        return;
    }

    // Check active layer for external changes
    StageLayerConfig* L = active_layer();

#define CHECK_DIRTY(field, var_name)                                                                                   \
    if (L->field != s_prev.field) {                                                                                    \
        s_prev.field = L->field;                                                                                       \
        s_model_handle.DirtyVariable(var_name);                                                                        \
    }

    CHECK_DIRTY(enabled, "layer_enabled");
    CHECK_DIRTY(scale_factor_x, "layer_scale_x");
    CHECK_DIRTY(scale_factor_y, "layer_scale_y");
    CHECK_DIRTY(parallax_x, "layer_parallax_x");
    CHECK_DIRTY(parallax_y, "layer_parallax_y");
    CHECK_DIRTY(offset_x, "layer_offset_x");
    CHECK_DIRTY(offset_y, "layer_offset_y");
    CHECK_DIRTY(original_bg_index, "layer_bg_index");
    CHECK_DIRTY(z_index, "layer_z_index");
    CHECK_DIRTY(loop_x, "layer_loop_x");
    CHECK_DIRTY(loop_y, "layer_loop_y");

    if ((int)L->scale_mode != s_prev.scale_mode) {
        s_prev.scale_mode = (int)L->scale_mode;
        s_model_handle.DirtyVariable("layer_scale_mode");
        s_model_handle.DirtyVariable("is_manual_scale");
        s_model_handle.DirtyVariable("is_fit_height");
    }

    if (strcmp(L->filename, s_prev.filename) != 0) {
        snprintf(s_prev.filename, sizeof(s_prev.filename), "%s", L->filename);
        s_model_handle.DirtyVariable("layer_filename");
    }

#undef CHECK_DIRTY
}

// ── Shutdown ───────────────────────────────────────────────────

extern "C" void rmlui_stage_config_shutdown() {
    s_model_handle = Rml::DataModelHandle();
    s_selected_layer = 0;
}
