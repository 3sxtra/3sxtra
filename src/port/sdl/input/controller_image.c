/**
 * @file controller_image.c
 * @brief ControllerImage library wrapper — device lifecycle and image rendering.
 *
 * Manages the ControllerImage library lifecycle, per-slot device handles, and
 * provides SDL_Surface* image rendering for controller buttons and axes.
 */
#include "port/sdl/input/controller_image.h"

#include <SDL3/SDL.h>
#include <string.h>

#include "controllerimage.h"

#define CONTROLLER_IMAGE_MAX_SLOTS 4

static bool s_initialized = false;
static ControllerImage_Device* s_devices[CONTROLLER_IMAGE_MAX_SLOTS] = { 0 };

bool ControllerImage_Module_Init(void) {
    if (s_initialized) {
        return true;
    }

    if (!ControllerImage_Init()) {
        SDL_Log("[ControllerImage] Init failed: %s", SDL_GetError());
        return false;
    }

    /* Load the standard data file from assets/ (deployed alongside the binary) */
    const char* base_path = SDL_GetBasePath();
    if (!base_path) {
        base_path = "";
    }

    char data_path[1024];
    SDL_snprintf(data_path, sizeof(data_path), "%sassets/controllerimage-standard.bin", base_path);

    if (!ControllerImage_AddDataFromFile(data_path)) {
        SDL_Log("[ControllerImage] Failed to load data from '%s': %s", data_path, SDL_GetError());
        /* Try without the base_path prefix (running from the assets/ parent dir) */
        if (!ControllerImage_AddDataFromFile("assets/controllerimage-standard.bin")) {
            SDL_Log("[ControllerImage] Failed to load data (fallback): %s", SDL_GetError());
            ControllerImage_Quit();
            return false;
        }
    }

    SDL_Log("[ControllerImage] Initialized successfully, data loaded from '%s'", data_path);

    memset(s_devices, 0, sizeof(s_devices));
    s_initialized = true;
    return true;
}

void ControllerImage_Module_Quit(void) {
    if (!s_initialized) {
        return;
    }

    for (int i = 0; i < CONTROLLER_IMAGE_MAX_SLOTS; i++) {
        if (s_devices[i]) {
            ControllerImage_DestroyDevice(s_devices[i]);
            s_devices[i] = NULL;
        }
    }

    ControllerImage_Quit();
    s_initialized = false;
    SDL_Log("[ControllerImage] Shut down");
}

void ControllerImage_Module_OnGamepadAdded(SDL_Gamepad* gamepad, int slot) {
    if (!s_initialized || !gamepad) {
        return;
    }
    if (slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return;
    }

    /* Clean up any previous device in this slot */
    if (s_devices[slot]) {
        ControllerImage_DestroyDevice(s_devices[slot]);
        s_devices[slot] = NULL;
    }

    ControllerImage_Device* dev = ControllerImage_CreateGamepadDevice(gamepad);
    if (!dev) {
        SDL_Log("[ControllerImage] Failed to create device for slot %d: %s", slot, SDL_GetError());
        return;
    }

    s_devices[slot] = dev;

    const char* device_type = ControllerImage_GetDeviceType(dev);
    SDL_Log("[ControllerImage] Slot %d: device type = '%s'", slot, device_type ? device_type : "unknown");
}

void ControllerImage_Module_OnGamepadRemoved(int slot) {
    if (!s_initialized) {
        return;
    }
    if (slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return;
    }

    if (s_devices[slot]) {
        SDL_Log("[ControllerImage] Slot %d: device removed", slot);
        ControllerImage_DestroyDevice(s_devices[slot]);
        s_devices[slot] = NULL;
    }
}

const char* ControllerImage_Module_GetDeviceType(int slot) {
    if (!s_initialized || slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return NULL;
    }
    if (!s_devices[slot]) {
        return NULL;
    }
    return ControllerImage_GetDeviceType(s_devices[slot]);
}

SDL_Surface* ControllerImage_Module_CreateButtonSurface(int slot,
                                                         SDL_GamepadButton button,
                                                         int size) {
    if (!s_initialized || slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return NULL;
    }
    if (!s_devices[slot]) {
        return NULL;
    }
    return ControllerImage_CreateSurfaceForButton(s_devices[slot], button, size);
}

SDL_Surface* ControllerImage_Module_CreateAxisSurface(int slot,
                                                       SDL_GamepadAxis axis,
                                                       int size) {
    if (!s_initialized || slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return NULL;
    }
    if (!s_devices[slot]) {
        return NULL;
    }
    return ControllerImage_CreateSurfaceForAxis(s_devices[slot], axis, size);
}
