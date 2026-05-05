/**
 * @file rmlui_crt_calibration.cpp
 * @brief RmlUi CRT Calibration screen data model.
 */

#include "port/sdl/rmlui/rmlui_crt_calibration.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

#include <RmlUi/Core.h>
#include <SDL3/SDL.h>

static Rml::DataModelHandle s_model_handle;
static bool s_model_registered = false;
static int s_pattern_index = 0;

#define NUM_PATTERNS 6


extern "C" void rmlui_crt_calibration_init(void) {
    Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
    if (!ctx)
        return;

    Rml::DataModelConstructor ctor = ctx->CreateDataModel("crt_calibration");
    if (!ctor)
        return;

    s_pattern_index = 0;

    ctor.Bind("pattern_index", &s_pattern_index);

    s_model_handle = ctor.GetModelHandle();
    s_model_registered = true;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[RmlUi CRTCalibration] Data model registered");
}

extern "C" void rmlui_crt_calibration_update(void) {
    if (!s_model_registered || !s_model_handle)
        return;
    // Fast path: skip updates when hidden
    if (!rmlui_wrapper_is_game_document_visible("crt_calibration"))
        return;
}

extern "C" void rmlui_crt_calibration_show(void) {
    rmlui_wrapper_show_game_document("crt_calibration");
}

extern "C" void rmlui_crt_calibration_hide(void) {
    rmlui_wrapper_hide_game_document("crt_calibration");
}

extern "C" void rmlui_crt_calibration_shutdown(void) {
    if (s_model_registered) {
        rmlui_wrapper_hide_game_document("crt_calibration");
        Rml::Context* ctx = static_cast<Rml::Context*>(rmlui_wrapper_get_game_context());
        if (ctx)
            ctx->RemoveDataModel("crt_calibration");
        s_model_registered = false;
    }
}

static void update_pattern_state(void) {
    if (s_pattern_index < 0) s_pattern_index = NUM_PATTERNS - 1;
    if (s_pattern_index >= NUM_PATTERNS) s_pattern_index = 0;
    
    if (s_model_registered && s_model_handle) {
        s_model_handle.DirtyVariable("pattern_index");
    }
}

extern "C" void rmlui_crt_calibration_next_pattern(void) {
    s_pattern_index++;
    update_pattern_state();
}

extern "C" void rmlui_crt_calibration_prev_pattern(void) {
    s_pattern_index--;
    update_pattern_state();
}
