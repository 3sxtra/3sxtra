#ifndef SHADER_MANAGER_H
#define SHADER_MANAGER_H

#include "glslp_parser.h"
#include <glad/gl.h>

typedef struct {
    GLuint program;
    GLuint fbo;
    GLuint texture;
    int width;
    int height;
    GLSLP_ShaderPass* pass_info;
    GLint loc_MVPMatrix;
    GLint loc_projection;
    GLint loc_Source;
    GLint loc_Texture;
    GLint loc_Original;
    GLint loc_OriginalHistory0;
    GLint loc_SourceSize;
    GLint loc_OriginalSize;
    GLint loc_OriginalHistorySize0;
    GLint loc_OutputSize;
    GLint loc_TextureSize;
    GLint loc_InputSize;
    GLint loc_FrameCount;
    GLint loc_FrameDirection;
} ShaderPassRuntime;

typedef struct {
    GLuint id;
    char name[64];
    int width;
    int height;
} ShaderTexture;

typedef struct {
    char name[64];
    float value;
} ShaderParameter;

typedef struct {
    GLSLP_Preset* preset;
    ShaderPassRuntime* passes;
    int pass_count;

    ShaderTexture textures[MAX_TEXTURES];
    int texture_count;

    ShaderParameter parameters[MAX_PARAMETERS];
    int parameter_count;

    GLuint vao;
    GLuint vbo;
    int frame_count;

    // History
    GLuint history_textures[8]; // MAX_HISTORY 8
    int history_width[8];
    int history_height[8];
    int history_index;
    GLuint history_fbo;
    GLuint blit_program;
    GLint loc_blit_source;
} ShaderManager;

// Initialize the manager with a preset.
// Returns NULL on failure.
ShaderManager* ShaderManager_Init(GLSLP_Preset* preset, const char* base_path);

// Render the pipeline.
// input_texture: The original game texture.
// input_w, input_h: Dimensions of the input texture.
// viewport_w, viewport_h: Dimensions of the final output viewport.
void ShaderManager_Render(ShaderManager* manager, GLuint input_texture, int input_w, int input_h, int viewport_w,
                          int viewport_h);

// Free resources.
void ShaderManager_Free(ShaderManager* manager);

#endif
