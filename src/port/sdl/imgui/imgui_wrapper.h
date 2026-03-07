#pragma once

#include <stdbool.h>

// Forward declare SDL types to avoid including SDL headers in C code
struct SDL_Window;
struct SDL_Renderer;
union SDL_Event;
struct SDL_Texture;

#ifdef __cplusplus
extern "C" {
#endif

void imgui_wrapper_init(struct SDL_Window* window, void* gl_context);
void imgui_wrapper_shutdown();
void imgui_wrapper_process_event(union SDL_Event* event);
void imgui_wrapper_new_frame();
void imgui_wrapper_render();
#include "control_mapping.h"

void imgui_wrapper_show_demo_window();
void imgui_wrapper_show_control_mapping_window(int window_width, int window_height);
bool imgui_wrapper_want_capture_mouse();
bool imgui_wrapper_want_capture_keyboard();
void imgui_wrapper_capture_input(bool);

void* imgui_wrapper_load_texture(const char* filename);
void imgui_wrapper_free_texture(void* texture_id);
void imgui_wrapper_get_texture_size(void* texture_id, int* w, int* h);
void imgui_wrapper_load_capcom_icons();
void* imgui_wrapper_get_capcom_icons_texture();

#ifdef __cplusplus
}
#endif
