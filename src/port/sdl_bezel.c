/**
 * @file sdl_bezel.c
 * @brief Character-specific bezel overlay system.
 *
 * Loads left/right bezel textures for each character, calculates their
 * layout alongside the game viewport, and supports hot-swapping bezels
 * when characters change between rounds.
 */
#include "port/sdl_bezel.h"
#include "port/paths.h"
#include "port/sdl/sdl_app.h"
#include "port/sdl/sdl_texture_util.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>
#include <stdio.h>
#include <string.h>

static BezelTextures current_textures = { NULL, NULL };
static bool bezel_visible = true;

// Character names matching assets/bezels/bezel_[name]_left/right.png
// Note: gill (0) maps to common as no gill-specific bezel exists
static const char* bezel_char_names[] = { "common", "alex",   "ryu",    "yun",  "dudley", "necro", "hugo",
                                          "ibuki",  "elena",  "oro",    "yang", "ken",    "sean",  "urien",
                                          "akuma",  "chunli", "makoto", "q",    "twelve", "remy" };

/** @brief Set a texture to use nearest-neighbor (pixel-perfect) filtering. */
static void SetTextureNearest(void* texture_ptr) {
    if (!texture_ptr)
        return;

    // SDL_GPU handles sampling via sampler objects, not texture parameters
    if (SDLApp_GetRenderer() == RENDERER_SDLGPU)
        return;

    // SDL2D: use SDL_SetTextureScaleMode instead of GL calls (no GL context)
    if (SDLApp_GetRenderer() == RENDERER_SDL2D) {
        SDL_SetTextureScaleMode((SDL_Texture*)texture_ptr, SDL_SCALEMODE_NEAREST);
        return;
    }

    GLuint tex = (GLuint)(intptr_t)texture_ptr;
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
}

/** @brief Initialize the bezel system (reset textures to NULL). */
void BezelSystem_Init() {
    current_textures.left = NULL;
    current_textures.right = NULL;
    bezel_visible = true;
}

/** @brief Shut down the bezel system (release texture references). */
void BezelSystem_Shutdown() {
    current_textures.left = NULL;
    current_textures.right = NULL;
}

/** @brief Set bezel overlay visibility. */
void BezelSystem_SetVisible(bool visible) {
    bezel_visible = visible;
}

/** @brief Check if bezels are currently visible. */
bool BezelSystem_IsVisible() {
    return bezel_visible;
}

/** @brief Load the default (common) left/right bezel textures. */
bool BezelSystem_LoadTextures() {
    char left_path[512], right_path[512];
    if (!BezelSystem_GetDefaultPaths(left_path, right_path, sizeof(left_path))) {
        return false;
    }

    current_textures.left = TextureUtil_Load(left_path);
    current_textures.right = TextureUtil_Load(right_path);

    SetTextureNearest(current_textures.left);
    SetTextureNearest(current_textures.right);

    return (current_textures.left != NULL && current_textures.right != NULL);
}

/** @brief Write the current bezel textures into the caller's struct. */
void BezelSystem_GetTextures(BezelTextures* out) {
    if (out) {
        *out = current_textures;
    }
}

/** @brief Directly set the left/right bezel textures. */
void BezelSystem_SetTextures(void* left, void* right) {
    current_textures.left = left;
    current_textures.right = right;
}

/** @brief Build default bezel file paths (common art). */
bool BezelSystem_GetDefaultPaths(char* left_out, char* right_out, size_t buffer_size) {
    const char* base = Paths_GetBasePath();
    if (!base) {
        return false;
    }

    const char* left_fn = "assets/bezels/bezel_common_left.png";
    const char* right_fn = "assets/bezels/bezel_common_right.png";

    SDL_snprintf(left_out, buffer_size, "%s%s", base, left_fn);
    SDL_snprintf(right_out, buffer_size, "%s%s", base, right_fn);

    return true;
}

/** @brief Hot-swap bezel textures for the given P1/P2 character IDs. */
void BezelSystem_SetCharacters(int p1_char, int p2_char) {
    const char* base = Paths_GetBasePath();
    if (!base)
        return;

    char left_path[512], right_path[512];
    const char* p1_name = BezelSystem_GetCharacterAssetPrefix(p1_char);
    const char* p2_name = BezelSystem_GetCharacterAssetPrefix(p2_char);

    SDL_snprintf(left_path, sizeof(left_path), "%sassets/bezels/bezel_%s_left.png", base, p1_name);
    SDL_snprintf(right_path, sizeof(right_path), "%sassets/bezels/bezel_%s_right.png", base, p2_name);

    void* l_tex = TextureUtil_Load(left_path);
    void* r_tex = TextureUtil_Load(right_path);

    // Fallback if character specific bezel is missing
    if (!l_tex || !r_tex) {
        char def_l[512], def_r[512];
        BezelSystem_GetDefaultPaths(def_l, def_r, sizeof(def_l));
        if (!l_tex)
            l_tex = TextureUtil_Load(def_l);
        if (!r_tex)
            r_tex = TextureUtil_Load(def_r);
    }

    SetTextureNearest(l_tex);
    SetTextureNearest(r_tex);

    current_textures.left = l_tex;
    current_textures.right = r_tex;
}

/** @brief Map a character ID to its bezel asset name prefix. */
const char* BezelSystem_GetCharacterAssetPrefix(int char_id) {
    if (char_id >= 0 && char_id < (int)(sizeof(bezel_char_names) / sizeof(bezel_char_names[0]))) {
        return bezel_char_names[char_id];
    }
    return "common";
}

/** @brief Calculate bezel rectangles positioned beside the game viewport. */
void BezelSystem_CalculateLayout(int window_w, int window_h, const SDL_FRect* game_rect, SDL_FRect* left_dst,
                                 SDL_FRect* right_dst) {
    if (!left_dst || !right_dst)
        return;

    SDL_zero(*left_dst);
    SDL_zero(*right_dst);

    int w = 0, h = 0;
    if (current_textures.left) {
        TextureUtil_GetSize(current_textures.left, &w, &h);
        if (h > 0) {
            float aspect = (float)w / (float)h;
            left_dst->h = (float)window_h;
            left_dst->w = left_dst->h * aspect;
            left_dst->y = 0.0f;
            left_dst->x = game_rect->x - left_dst->w;
        }
    }

    if (current_textures.right) {
        TextureUtil_GetSize(current_textures.right, &w, &h);
        if (h > 0) {
            float aspect = (float)w / (float)h;
            right_dst->h = (float)window_h;
            right_dst->w = right_dst->h * aspect;
            right_dst->y = 0.0f;
            right_dst->x = game_rect->x + game_rect->w;
        }
    }
}
