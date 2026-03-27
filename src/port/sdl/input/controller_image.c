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

#include "controllerimage/controllerimage.h"
#include "port/config/paths.h"

#define CONTROLLER_IMAGE_MAX_SLOTS 4

static bool s_initialized = false;
static ControllerImage_GamepadDevice* s_devices[CONTROLLER_IMAGE_MAX_SLOTS] = { 0 };
static ControllerImage_KeyboardDevice* s_keyboard_device = NULL;

bool ControllerImage_Module_Init(void) {
    if (s_initialized) {
        return true;
    }

    if (ControllerImage_Init() < 0) {
        SDL_Log("[ControllerImage] Init failed: %s", SDL_GetError());
        return false;
    }

    /* Load the standard data file from assets/ (deployed alongside the binary) */
    const char* base_path = Paths_GetBasePath();

    char kenney_path[1024];
    SDL_snprintf(kenney_path, sizeof(kenney_path), "%sassets/controllers/controllerimage-kenney.bin", base_path);
    if (ControllerImage_AddDataFromFile(kenney_path) < 0) {
        if (ControllerImage_AddDataFromFile("assets/controllers/controllerimage-kenney.bin") < 0) {
            SDL_Log("[ControllerImage] Failed to load kenney data: %s", SDL_GetError());
        }
    }

    char ordinary_path[1024];
    SDL_snprintf(ordinary_path, sizeof(ordinary_path), "%sassets/controllers/controllerimage-ordinary.bin", base_path);
    if (ControllerImage_AddDataFromFile(ordinary_path) < 0) {
        if (ControllerImage_AddDataFromFile("assets/controllers/controllerimage-ordinary.bin") < 0) {
            SDL_Log("[ControllerImage] Failed to load ordinary data: %s", SDL_GetError());
        }
    }

    char data_path[1024];
    SDL_snprintf(data_path, sizeof(data_path), "%sassets/controllers/controllerimage-standard.bin", base_path);
    if (ControllerImage_AddDataFromFile(data_path) < 0) {
        SDL_Log("[ControllerImage] Failed to load data from '%s': %s", data_path, SDL_GetError());
        /* Try without the base_path prefix (running from the assets/ parent dir) */
        if (ControllerImage_AddDataFromFile("assets/controllers/controllerimage-standard.bin") < 0) {
            SDL_Log("[ControllerImage] Failed to load standard data (fallback): %s", SDL_GetError());
            ControllerImage_Quit();
            return false;
        }
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[ControllerImage] Initialized successfully, data loaded from '%s', '%s', '%s'", kenney_path, ordinary_path, data_path);

    s_keyboard_device = ControllerImage_CreateKeyboardDevice();
    if (!s_keyboard_device) {
        SDL_Log("[ControllerImage] Failed to create keyboard device: %s", SDL_GetError());
    }

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
            ControllerImage_DestroyGamepadDevice(s_devices[i]);
            s_devices[i] = NULL;
        }
    }

    if (s_keyboard_device) {
        ControllerImage_DestroyKeyboardDevice(s_keyboard_device);
        s_keyboard_device = NULL;
    }

    ControllerImage_Quit();
    s_initialized = false;
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[ControllerImage] Shut down");
}

void ControllerImage_Module_OnGamepadAdded(SDL_Gamepad* gamepad, int slot) {
    if (!gamepad) {
        return;
    }
    // Lazy-init: load data on first gamepad connection instead of boot
    if (!s_initialized) {
        if (!ControllerImage_Module_Init()) {
            return;
        }
    }
    if (slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return;
    }

    /* Clean up any previous device in this slot */
    if (s_devices[slot]) {
        ControllerImage_DestroyGamepadDevice(s_devices[slot]);
        s_devices[slot] = NULL;
    }

    ControllerImage_GamepadDevice* dev = ControllerImage_CreateGamepadDevice(gamepad);
    if (!dev) {
        SDL_Log("[ControllerImage] Failed to create device for slot %d: %s", slot, SDL_GetError());
        return;
    }

    s_devices[slot] = dev;

    const char* device_type = ControllerImage_GetDeviceType(dev);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[ControllerImage] Slot %d: device type = '%s'", slot, device_type ? device_type : "unknown");
}

void ControllerImage_Module_OnGamepadRemoved(int slot) {
    if (!s_initialized) {
        return;
    }
    if (slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return;
    }

    if (s_devices[slot]) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[ControllerImage] Slot %d: device removed", slot);
        ControllerImage_DestroyGamepadDevice(s_devices[slot]);
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

SDL_Surface* ControllerImage_Module_CreateButtonSurface(int slot, SDL_GamepadButton button, int size) {
    if (!s_initialized || slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return NULL;
    }
    if (!s_devices[slot]) {
        return NULL;
    }
    return ControllerImage_CreateSurfaceForButton(s_devices[slot], button, size, 0);
}

SDL_Surface* ControllerImage_Module_CreateAxisSurface(int slot, SDL_GamepadAxis axis, int size) {
    if (!s_initialized || slot < 0 || slot >= CONTROLLER_IMAGE_MAX_SLOTS) {
        return NULL;
    }
    if (!s_devices[slot]) {
        return NULL;
    }
    return ControllerImage_CreateSurfaceForAxis(s_devices[slot], axis, size, 0);
}

SDL_Surface* ControllerImage_Module_CreateScancodeSurface(SDL_Scancode scancode, int size) {
    // Lazy init for keyboard-only users
    if (!s_initialized) {
        if (!ControllerImage_Module_Init()) {
            return NULL;
        }
    }
    if (!s_keyboard_device) {
        return NULL;
    }
    return ControllerImage_CreateSurfaceForScancode(s_keyboard_device, scancode, size, 0);
}
