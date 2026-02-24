#ifndef SDL_BEZEL_H
#define SDL_BEZEL_H

#include <SDL3/SDL_rect.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void BezelSystem_Init();
void BezelSystem_Shutdown();

typedef struct BezelTextures {
    void* left;
    void* right;
} BezelTextures;

bool BezelSystem_LoadTextures();
void BezelSystem_GetTextures(BezelTextures* out);
void BezelSystem_SetTextures(void* left, void* right); // Added for testing

void BezelSystem_SetVisible(bool visible);
bool BezelSystem_IsVisible();

void BezelSystem_SetCharacters(int p1_char, int p2_char);

/// Get the asset prefix for a character ID.
/// @param char_id Character ID (0-19)
/// @return String prefix (e.g. "ryu") or "cmn" if invalid.
const char* BezelSystem_GetCharacterAssetPrefix(int char_id);

/// Get paths to the default (common) bezels.
/// Returns true if paths were filled.
bool BezelSystem_GetDefaultPaths(char* left_out, char* right_out, size_t buffer_size);

/// Calculate destination rectangles for bezels.
/// @param window_w Window width
/// @param window_h Window height
/// @param game_rect The game viewport rectangle
/// @param left_dst Output for left bezel destination
/// @param right_dst Output for right bezel destination
void BezelSystem_CalculateLayout(int window_w, int window_h, const SDL_FRect* game_rect, SDL_FRect* left_dst,
                                 SDL_FRect* right_dst);

#ifdef __cplusplus
}
#endif

#endif
