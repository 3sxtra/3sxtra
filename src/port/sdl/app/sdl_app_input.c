/**
 * @file sdl_app_input.c
 * @brief SDL application input event dispatch.
 *
 * Central input event handler: routes SDL events to gamepad, keyboard,
 * UI toggle, and window management handlers. Split from sdl_app.c for
 * modularity.
 */
#include "port/sdl/app/sdl_app_input.h"
#include "netplay/netplay.h"
#include "port/config/config.h"
#include "port/mods/modded_stage.h"
#include "port/rendering/sdl_bezel.h"
#include "port/sdl/app/sdl_app.h"
#include "port/sdl/app/sdl_app_config.h"
#include "port/sdl/app/sdl_app_internal.h"
#include "port/sdl/app/sdl_app_shader_config.h"
#include "port/sdl/input/control_mapping.h"
#include "port/sdl/input/sdl_pad.h"
#include "port/sdl/netplay/sdl_netplay_ui.h"
#include "port/sdl/renderer/sdl_game_renderer.h"
#include "port/sdl/rmlui/rmlui_dev_overlay.h"
#include "port/sdl/rmlui/rmlui_phase3_toggles.h"
#include "port/sdl/rmlui/rmlui_wrapper.h"

// Key handlers
static void handle_menu_toggle(SDL_KeyboardEvent* event) {
    if (event->key == SDLK_F1 && event->down && !event->repeat) {
        SDLApp_ToggleMenu();
    }
}

static void handle_mods_menu_toggle(SDL_KeyboardEvent* event) {
    if (event->key == SDLK_F3 && event->down && !event->repeat) {
        SDLApp_ToggleModsMenu();
    }
}

static void handle_shader_menu_toggle(SDL_KeyboardEvent* event) {
    if (event->key == SDLK_F2 && event->down && !event->repeat) {
        SDLApp_ToggleShaderMenu();
    }
}

static void handle_close_all_menus(SDL_KeyboardEvent* event) {
    if (event->key == SDLK_ESCAPE && event->down && !event->repeat) {
        SDLApp_CloseAllMenus();
    }
}

static void set_screenshot_flag_if_needed(SDL_KeyboardEvent* event) {
    if ((event->key == SDLK_GRAVE) && event->down && !event->repeat) {
        SDLApp_SaveScreenshot();
    }
}

static void handle_fullscreen_toggle(SDL_KeyboardEvent* event) {
    const bool is_alt_enter = (event->key == SDLK_RETURN) && (event->mod & SDL_KMOD_ALT);
    const bool is_f11 = (event->key == SDLK_F11);
    const bool correct_key = (is_alt_enter || is_f11);

    if (correct_key && event->down && !event->repeat) {
        SDLApp_ToggleFullscreen();
    }
}

static void handle_scale_mode_toggle(SDL_KeyboardEvent* event) {
    if ((event->key == SDLK_F8) && event->down && !event->repeat) {
        SDLApp_CycleScaleMode();
    }
}

static void handle_texture_dump(SDL_KeyboardEvent* event) {
    if (event->key == SDLK_F12 && event->down && !event->repeat) {
        SDL_Log("[TextureDump] F12 pressed — dumping textures to textures/");
        SDLGameRenderer_DumpTextures();
    }
}

bool SDLAppInput_HandleEvent(SDL_Event* event) {
    bool request_quit = false;

    // SDL2D mode: no NetplayUI — skip UI processing
    if (!is_sdl2d_backend(SDLApp_GetRenderer())) {
        // Process UI events — route to RmlUi for window resize, display-scale,
        // and debug keybinds. The expensive InputEventHandler call is gated
        // internally by s_any_window_visible.
        rmlui_wrapper_process_event(event);
        SDLNetplayUI_ProcessEvent(event);

        // Global Key Toggles
        if (event->type == SDL_EVENT_KEY_DOWN) {
            handle_menu_toggle(&event->key);
            handle_shader_menu_toggle(&event->key);
            handle_mods_menu_toggle(&event->key);
            handle_close_all_menus(&event->key);
            set_screenshot_flag_if_needed(&event->key);
            handle_fullscreen_toggle(&event->key);
            handle_scale_mode_toggle(&event->key);
            // handle_texture_dump(&event->key); // F12 reserved for RmlUi native debugger

            if (event->key.key == SDLK_F7 && event->key.down && !event->key.repeat) {
                SDLApp_ToggleTrainingMenu();
            }

            if (event->key.key == SDLK_F6 && event->key.down && !event->key.repeat) {
                SDLApp_ToggleStageConfigMenu();
            }

            if (event->key.key == SDLK_F9 && event->key.down && !event->key.repeat) {
                SDLApp_ToggleDevOverlay();
            }

            if (event->key.key == SDLK_F4 && event->key.down && !event->key.repeat) {
                SDLAppShader_ToggleMode();
            }
            if (event->key.key == SDLK_F5 && event->key.down && !event->key.repeat) {
                if (!Netplay_IsEnabled()) {
                    SDLApp_ToggleFrameRateUncap();
                }
            }
        }

        // Input Capture for UI — only query when docs are visible
        bool ui_wants_mouse = false;
        bool ui_wants_keyboard = false;
        if (rmlui_wrapper_any_window_visible()) {
            ui_wants_mouse = rmlui_wrapper_want_capture_mouse();
            ui_wants_keyboard = rmlui_wrapper_want_capture_keyboard();
        }
        // When control mapping (F1) is open, disable UI capture so all
        // input reaches SDLPad handlers.
        if (SDLApp_IsMenuVisible()) {
            ui_wants_mouse = false;
            ui_wants_keyboard = false;
        }

        // Block events based on what the UI actually wants to capture.
        // Mouse capture blocks only mouse events; keyboard capture blocks
        // only keyboard events.  Gamepad/joystick always passes through.
        if (event->type == SDL_EVENT_QUIT) {
            // Always handle quit
        } else if (ui_wants_mouse &&
                   (event->type == SDL_EVENT_MOUSE_MOTION || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                    event->type == SDL_EVENT_MOUSE_BUTTON_UP || event->type == SDL_EVENT_MOUSE_WHEEL)) {
            return false; // UI consumed mouse
        } else if (ui_wants_keyboard && (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP ||
                                         event->type == SDL_EVENT_TEXT_INPUT)) {
            return false; // UI consumed keyboard
        }
        // Gamepad, joystick, window events etc. always fall through
    } else {
        // SDL2D mode: no NetplayUI — route to RmlUi (InputEventHandler
        // gated internally by s_any_window_visible).
        rmlui_wrapper_process_event(event);

        if (event->type == SDL_EVENT_KEY_DOWN) {
            handle_menu_toggle(&event->key);
            handle_shader_menu_toggle(&event->key);
            handle_mods_menu_toggle(&event->key);
            handle_close_all_menus(&event->key);
            set_screenshot_flag_if_needed(&event->key);
            handle_fullscreen_toggle(&event->key);
            handle_scale_mode_toggle(&event->key);
            handle_texture_dump(&event->key);

            if (event->key.key == SDLK_F7 && event->key.down && !event->key.repeat) {
                SDLApp_ToggleTrainingMenu();
            }

            if (event->key.key == SDLK_F6 && event->key.down && !event->key.repeat) {
                SDLApp_ToggleStageConfigMenu();
            }

            if (event->key.key == SDLK_F5 && event->key.down && !event->key.repeat) {
                if (!Netplay_IsEnabled()) {
                    SDLApp_ToggleFrameRateUncap();
                }
            }
            if (event->key.key == SDLK_F4 && event->key.down && !event->key.repeat) {
                SDLAppShader_ToggleMode();
            }
            if (event->key.key == SDLK_F9 && event->key.down && !event->key.repeat) {
                if (Netplay_IsEnabled()) {
                    SDLNetplayUI_SetDiagnosticsVisible(!SDLNetplayUI_IsDiagnosticsVisible());
                } else {
                    SDLApp_ToggleDevOverlay();
                }
            }
            if (event->key.key == SDLK_F10 && event->key.down && !event->key.repeat) {
                SDLNetplayUI_SetDiagnosticsVisible(!SDLNetplayUI_IsDiagnosticsVisible());
            }
        }

        // Input capture for RmlUi in SDL2D mode — only when docs visible
        {
            bool ui_wants_mouse = false;
            bool ui_wants_keyboard = false;
            if (rmlui_wrapper_any_window_visible()) {
                ui_wants_mouse = rmlui_wrapper_want_capture_mouse();
                ui_wants_keyboard = rmlui_wrapper_want_capture_keyboard();
            }
            if (SDLApp_IsMenuVisible()) {
                ui_wants_mouse = false;
                ui_wants_keyboard = false;
            }
            if (event->type != SDL_EVENT_QUIT) {
                if (ui_wants_mouse &&
                    (event->type == SDL_EVENT_MOUSE_MOTION || event->type == SDL_EVENT_MOUSE_BUTTON_DOWN ||
                     event->type == SDL_EVENT_MOUSE_BUTTON_UP || event->type == SDL_EVENT_MOUSE_WHEEL)) {
                    return false;
                }
                if (ui_wants_keyboard && (event->type == SDL_EVENT_KEY_DOWN || event->type == SDL_EVENT_KEY_UP ||
                                          event->type == SDL_EVENT_TEXT_INPUT)) {
                    return false;
                }
            }
        }
    }

    switch (event->type) {
    case SDL_EVENT_GAMEPAD_ADDED:
    case SDL_EVENT_GAMEPAD_REMOVED:
        SDLPad_HandleGamepadDeviceEvent(&event->gdevice);
        break;

    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_GAMEPAD_BUTTON_UP:
        SDLPad_HandleGamepadButtonEvent(&event->gbutton);
        break;

    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        SDLPad_HandleGamepadAxisMotionEvent(&event->gaxis);
        break;

    case SDL_EVENT_JOYSTICK_ADDED:
    case SDL_EVENT_JOYSTICK_REMOVED:
        SDLPad_HandleJoystickDeviceEvent(&event->jdevice);
        break;

    case SDL_EVENT_JOYSTICK_BUTTON_DOWN:
    case SDL_EVENT_JOYSTICK_BUTTON_UP:
        SDLPad_HandleJoystickButtonEvent(&event->jbutton);
        break;

    case SDL_EVENT_JOYSTICK_AXIS_MOTION:
        SDLPad_HandleJoystickAxisEvent(&event->jaxis);
        break;

    case SDL_EVENT_JOYSTICK_HAT_MOTION:
        SDLPad_HandleJoystickHatEvent(&event->jhat);
        break;

    case SDL_EVENT_KEY_DOWN:
        // F-keys are handled globally under UI events
        SDLPad_HandleKeyboardEvent(&event->key);
        break;

    case SDL_EVENT_KEY_UP:
        SDLPad_HandleKeyboardEvent(&event->key);
        break;

    case SDL_EVENT_MOUSE_MOTION:
        SDLApp_HandleMouseMotion();
        break;

    case SDL_EVENT_WINDOW_RESIZED: {
        // Need to pass data to App
        // But the event structure is generic.
        // SDLApp_HandleResize() call requires implementation in sdl_app.c
        // Since we can't easily pass args through a void function pointer if we used callbacks,
        // but here we call functions directly.
        // However, SDLApp_HandleResize is not yet exposed.
        // Let's assume SDLApp_PollEvents still handles the loop and this function returns control.
        // Actually, this function processes ONE event.
        // Wait, SDLApp_HandleResize is internal.
        // I need to add it to sdl_app_internal.h
        // But for now, I'll rely on SDLApp_PollEvents calling this function.
        // Window resize logic is complex (glViewport etc).
        // Let's expose SDLApp_HandleWindowResize(int w, int h) in internal header.
        SDLApp_HandleWindowResize(event->window.data1, event->window.data2);
    } break;

    case SDL_EVENT_WINDOW_MOVED: {
        SDLApp_HandleWindowMove(event->window.data1, event->window.data2);
    } break;

    case SDL_EVENT_QUIT:
        request_quit = true;
        break;
    }

    return request_quit;
}
