/**
 * @file controller_image_overlay.h
 * @brief ControllerImage button-glyph overlay for the native Button Config menu.
 *
 * Manages per-slot cached GPU textures of controller button glyphs.
 * On init, generates textures via ControllerImage_Module_CreateButtonSurface()
 * for each connected controller's 8 button slots (+ 2 trigger axes).
 * The menu rendering code calls GetTexture() to draw controller-specific
 * glyphs instead of the hardcoded PlayStation sprite-sheet icons.
 */
#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_CONTROLLER_IMAGE

/// Number of button-icon rows in the Button Config menu.
#define CONTROLLER_OVERLAY_BUTTON_COUNT 8

/// Generate and cache button glyph textures for all connected controllers.
/// Call when the Button Config screen opens.
void ControllerImageOverlay_Init(void);

/// Free all cached button glyph textures.
/// Call when the Button Config screen closes.
void ControllerImageOverlay_Shutdown(void);

/// Get the opaque GPU texture handle for a specific slot + button row.
/// @param slot   Input source index (0 = P1, 1 = P2, ...).
/// @param button_row  Button row index (0–7, matching scrnAddTex1UV sprite order).
/// @return Opaque texture handle, or NULL if unavailable.
void* ControllerImageOverlay_GetTexture(int slot, int button_row);

/// Check whether overlay textures are available for a given slot.
bool ControllerImageOverlay_HasSlot(int slot);

/// Draw a controller button glyph at CPS3 screen coordinates.
/// Returns true if drawn, false if no texture was available (caller should fallback).
/// In deferred mode, this enqueues the draw command rather than drawing immediately.
/// @param slot       Input source index (0 = P1, 1 = P2).
/// @param button_row Button row index (0–7).
/// @param px, py     Top-left pixel position in CPS3 coords.
/// @param sx, sy     Width and height in pixels.
/// @param pz         Priority/depth (unused in deferred mode).
bool ControllerImageOverlay_DrawButton(int slot, int button_row, int px, int py, int sx, int sy, int pz);

/// Flush all deferred draw commands at full window resolution (GL backend).
/// Call after the CPS3 canvas has been upscaled to the window.
/// @param vp_x, vp_y   Letterbox viewport offset in window pixels.
/// @param vp_w, vp_h   Viewport size in window pixels.
/// @param win_w, win_h Window size in pixels.
void ControllerImageOverlay_FlushGL(float vp_x, float vp_y, float vp_w, float vp_h, int win_w, int win_h);

/// Clear the deferred draw queue without drawing (e.g. when overlay is not used this frame).
void ControllerImageOverlay_ClearQueue(void);

#else

#define CONTROLLER_OVERLAY_BUTTON_COUNT 8
static inline void ControllerImageOverlay_Init(void) {}
static inline void ControllerImageOverlay_Shutdown(void) {}
static inline bool ControllerImageOverlay_HasSlot(int slot) { (void)slot; return false; }
static inline void* ControllerImageOverlay_GetTexture(int slot, int button_row) { (void)slot; (void)button_row; return 0; }
static inline bool ControllerImageOverlay_DrawButton(int slot, int button_row, int px, int py, int sx, int sy, int pz) { (void)slot; (void)button_row; (void)px; (void)py; (void)sx; (void)sy; (void)pz; return false; }
static inline void ControllerImageOverlay_FlushGL(float vp_x, float vp_y, float vp_w, float vp_h, int win_w, int win_h) { (void)vp_x; (void)vp_y; (void)vp_w; (void)vp_h; (void)win_w; (void)win_h; }
static inline void ControllerImageOverlay_ClearQueue(void) {}

#endif


#ifdef __cplusplus
}
#endif
