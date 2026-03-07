#ifndef SDL_TEXT_RENDERER_INTERNAL_H
#define SDL_TEXT_RENDERER_INTERNAL_H

// OpenGL Backend
void SDLTextRendererGL_Init(const char* base_path, const char* font_path);
void SDLTextRendererGL_Shutdown(void);
void SDLTextRendererGL_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                float target_width, float target_height);
void SDLTextRendererGL_Flush(void);
void SDLTextRendererGL_SetYOffset(float y_offset);
void SDLTextRendererGL_SetBackgroundEnabled(int enabled);
void SDLTextRendererGL_SetBackgroundColor(float r, float g, float b, float a);
void SDLTextRendererGL_SetBackgroundPadding(float px);

// GPU Backend
void SDLTextRendererGPU_Init(const char* base_path, const char* font_path);
void SDLTextRendererGPU_Shutdown(void);
void SDLTextRendererGPU_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                 float target_width, float target_height);
void SDLTextRendererGPU_Flush(void);
void SDLTextRendererGPU_SetYOffset(float y_offset);
void SDLTextRendererGPU_SetBackgroundEnabled(int enabled);
void SDLTextRendererGPU_SetBackgroundColor(float r, float g, float b, float a);
void SDLTextRendererGPU_SetBackgroundPadding(float px);

// SDL2D Text Backend
void SDLTextRendererSDL_Init(const char* base_path, const char* font_path);
void SDLTextRendererSDL_Shutdown(void);
void SDLTextRendererSDL_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                 float target_width, float target_height);
void SDLTextRendererSDL_Flush(void);
void SDLTextRendererSDL_SetYOffset(float y_offset);
void SDLTextRendererSDL_SetBackgroundEnabled(int enabled);
void SDLTextRendererSDL_SetBackgroundColor(float r, float g, float b, float a);
void SDLTextRendererSDL_SetBackgroundPadding(float px);

#endif
