#ifndef SDL_TEXT_RENDERER_H
#define SDL_TEXT_RENDERER_H

#ifdef __cplusplus
extern "C" {
#endif

void SDLTextRenderer_Init(const char* base_path, const char* font_path);
void SDLTextRenderer_Shutdown(void);
void SDLTextRenderer_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                              float target_width, float target_height);
void SDLTextRenderer_DrawDebugBuffer(float target_width, float target_height);
void SDLTextRenderer_SetYOffset(float y_offset);
void SDLTextRenderer_SetBackgroundEnabled(int enabled);
void SDLTextRenderer_SetBackgroundColor(float r, float g, float b, float a);
void SDLTextRenderer_SetBackgroundPadding(float px);
void SDLTextRenderer_Flush(void);

#ifdef __cplusplus
}
#endif

#endif
