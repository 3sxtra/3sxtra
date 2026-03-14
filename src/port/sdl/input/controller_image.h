/**
 * @file controller_image.h
 * @brief ControllerImage library wrapper — device lifecycle and image rendering.
 *
 * Wraps the icculus/ControllerImage library to track per-slot gamepad devices
 * and provide on-demand controller button/axis image rendering as SDL_Surface*.
 * This module does NOT handle how images are displayed — that is left to the
 * renderer layer (RmlUi, OpenGL, SDL_GPU, etc.).
 */
#pragma once

#include <SDL3/SDL.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize the ControllerImage library and load the data files.
/// Call after SDL_Init() and SDLPad_Init().
/// Returns true on success, false on failure (logged via SDL_Log).
bool ControllerImage_Module_Init(void);

/// Shut down the ControllerImage library and free all resources.
/// Safe to call even if Init failed.
void ControllerImage_Module_Quit(void);

/// Notify the module that a gamepad was connected in the given input slot.
/// @param gamepad  The SDL_Gamepad* handle (must remain valid while tracked).
/// @param slot     The input source index (0..INPUT_SOURCES_MAX-1).
void ControllerImage_Module_OnGamepadAdded(SDL_Gamepad* gamepad, int slot);

/// Notify the module that a gamepad was disconnected from the given slot.
/// @param slot  The input source index.
void ControllerImage_Module_OnGamepadRemoved(int slot);

/// Get the device type string for the controller in the given slot.
/// Returns a library-internal string (e.g. "xbox360", "ps5", "switchpro")
/// or NULL if no device is tracked in that slot.
const char* ControllerImage_Module_GetDeviceType(int slot);

/// Create an SDL_Surface with the image for a specific button.
/// The caller owns the returned surface and must call SDL_DestroySurface().
/// @param slot    Input source index.
/// @param button  SDL gamepad button enum.
/// @param size    Desired image size in pixels (width and height).
/// @return SDL_Surface* or NULL on failure.
SDL_Surface* ControllerImage_Module_CreateButtonSurface(int slot, SDL_GamepadButton button, int size);

/// Create an SDL_Surface with the image for a specific axis.
/// The caller owns the returned surface and must call SDL_DestroySurface().
/// @param slot  Input source index.
/// @param axis  SDL gamepad axis enum.
/// @param size  Desired image size in pixels (width and height).
/// @return SDL_Surface* or NULL on failure.
SDL_Surface* ControllerImage_Module_CreateAxisSurface(int slot, SDL_GamepadAxis axis, int size);

#ifdef __cplusplus
}
#endif
