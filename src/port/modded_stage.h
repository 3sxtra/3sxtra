/**
 * @file modded_stage.h
 * @brief Modded stage background system — HD multi-layer parallax replacement.
 *
 * Provides an optional cosmetic-only replacement for the original PS2 tile-based
 * stage backgrounds. When enabled and assets exist for the current stage, the
 * tile rendering pipeline (scr_trans) is bypassed and HD PNG layers are drawn
 * with parallax scrolling driven by the live bg_w engine values.
 *
 * All original engine state (scroll, zoom, limits, animations) remains fully
 * active — this system only replaces the visual output.
 */
#ifndef MODDED_STAGE_H
#define MODDED_STAGE_H

#include "sf33rd/Source/Game/stage/bg.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the modded stage system. Call once at app startup.
 */
void ModdedStage_Init(void);

/**
 * @brief Shut down the modded stage system and free all resources.
 */
void ModdedStage_Shutdown(void);

/**
 * @brief Enable or disable modded stage backgrounds globally.
 * When disabled, the original tile renderer is used even if HD assets exist.
 */
void ModdedStage_SetEnabled(bool enabled);

/**
 * @brief Check if modded stages are globally enabled.
 */
bool ModdedStage_IsEnabled(void);

/**
 * @brief Disable all stage rendering (tiles + animations).
 * When enabled, the stage is completely blank — useful for testing
 * modded backgrounds against a clean canvas.
 */
void ModdedStage_SetDisableRendering(bool disabled);

/**
 * @brief Check if stage rendering is fully disabled.
 */
bool ModdedStage_IsRenderingDisabled(void);

/**
 * @brief Disable stage animations (crowd, fire, birds, etc.) independently.
 * Tiles/HD layers still render; only the animated sprite objects are suppressed.
 */
void ModdedStage_SetAnimationsDisabled(bool disabled);

/**
 * @brief Check if stage animations are disabled.
 */
bool ModdedStage_IsAnimationsDisabled(void);

/**
 * @brief Scan for and load HD layer assets for the given stage index.
 *
 * Looks for assets/stages/stage_XX/layer_0.png through layer_3.png.
 * If layer_0.png does not exist, the stage has no mod and
 * ModdedStage_IsActiveForCurrentStage() will return false.
 *
 * @param stage_index The bg_w.stage index (0-21).
 */
void ModdedStage_LoadForStage(int stage_index);

/**
 * @brief Free any loaded modded stage textures.
 */
void ModdedStage_Unload(void);

/**
 * @brief Check if modded rendering should be used for the current stage.
 *
 * Returns true only if: globally enabled AND assets are loaded for the current stage.
 */
bool ModdedStage_IsActiveForCurrentStage(void);

/**
 * @brief Get the number of loaded layers for the current modded stage.
 * Returns 0 if no modded stage is loaded.
 */
int ModdedStage_GetLayerCount(void);

/**
 * @brief Get the stage index that modded assets are currently loaded for.
 * Returns -1 if nothing is loaded.
 */
int ModdedStage_GetLoadedStageIndex(void);

/**
 * @brief Render the HD parallax layers at native viewport resolution.
 *
 * Reads scroll position and limits directly from the BG struct.
 * Called from SDLApp_EndFrame() into the backbuffer before the canvas blit.
 * Caller must set glViewport to the letterboxed game area.
 *
 * @param bg Pointer to the live bg_w struct (read-only).
 */
void ModdedStage_Render(const BG* bg);

#ifdef __cplusplus
}
#endif

#endif /* MODDED_STAGE_H */
