/**
 * @file sdl_game_renderer_gl_context.c
 * @brief OpenGL renderer global state definitions.
 *
 * Defines the global GLRendererState instance and CPS3 canvas texture
 * handle used by the OpenGL rendering backend.
 */
#include "port/sdl/sdl_game_renderer_gl_internal.h"

// Define the global state instance
GLRendererState gl_state = { 0 };
unsigned int cps3_canvas_texture = 0;
