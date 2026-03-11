#ifndef GLSLP_PARSER_H
#define GLSLP_PARSER_H

#include <stdbool.h>

#define MAX_SHADERS 32
#define MAX_TEXTURES 32
#define MAX_PARAMETERS 128
#define MAX_PATH 1024

typedef enum { GLSLP_SCALE_SOURCE, GLSLP_SCALE_VIEWPORT, GLSLP_SCALE_ABSOLUTE } GLSLP_ScaleType;

typedef struct {
    char path[MAX_PATH];
    bool filter_linear;
    GLSLP_ScaleType scale_type_x;
    GLSLP_ScaleType scale_type_y;
    float scale_x;
    float scale_y;
    bool srgb_framebuffer;
    bool float_framebuffer;
    char alias[64];
    bool mipmap_input;
    char wrap_mode[32];
    int frame_count_mod;
    char source_preset[MAX_PATH]; // Tracks which preset this pass originated from
} GLSLP_ShaderPass;

typedef struct {
    char name[64];
    char path[MAX_PATH];
    char wrap_mode[32];
    bool linear;
    bool mipmap;
} GLSLP_Texture;

typedef struct {
    char name[64];
    float value;
} GLSLP_Parameter;

typedef struct {
    int pass_count;
    GLSLP_ShaderPass passes[MAX_SHADERS];

    int texture_count;
    GLSLP_Texture textures[MAX_TEXTURES];

    int parameter_count;
    GLSLP_Parameter parameters[MAX_PARAMETERS];
} GLSLP_Preset;

// Loads a preset from disk. Returns NULL on failure.
// The caller is responsible for freeing the returned pointer with GLSLP_Free.
GLSLP_Preset* GLSLP_Load(const char* path);

// Frees a preset allocated by GLSLP_Load.
void GLSLP_Free(GLSLP_Preset* preset);

// Writes a preset to disk in .slangp/.glslp format.
// Returns true on success, false on failure.
bool GLSLP_Write(const GLSLP_Preset* preset, const char* path);

// Appends all passes, textures, and parameters from src into dst.
// Returns true on success, false if limits exceeded.
bool GLSLP_Append(GLSLP_Preset* dst, const GLSLP_Preset* src);

// Removes a pass at the given index (0-based). Shifts subsequent passes down.
void GLSLP_RemovePass(GLSLP_Preset* preset, int index);

// Moves a pass from one index to another. Shifts other passes accordingly.
void GLSLP_MovePass(GLSLP_Preset* preset, int from, int to);

#endif
