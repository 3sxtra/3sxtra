/**
 * @file sdl_text_renderer_gl.c
 * @brief OpenGL TrueType text renderer using stb_truetype.
 *
 * Rasterizes bitmap fonts from TTF files, renders text as textured quads
 * with per-character glyph caching, and provides debug overlay text
 * with optional background rectangles.
 */
#include "port/sdl/sdl_text_renderer.h"
#include "port/sdl/sdl_text_renderer_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "types.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "port/imgui_font_8x8.h"
#include "port/sdl/sdl_app.h"

typedef struct {
    GLuint texture_id;
    int width;
    int height;
} FontAtlas;

static FontAtlas s_font_atlas;
static GLuint s_text_vao = 0;
static GLuint s_text_vbo = 0;
static GLuint s_text_shader = 0;
static float s_text_y_offset = 8.0f;
static GLuint s_rect_shader = 0;
static int s_bg_enabled = 1;
static float s_bg_color[4] = { 0.0f, 0.0f, 0.0f, 0.6f };
static float s_bg_padding = 2.0f;

// Cached uniform locations
static GLint s_text_loc_projection = -1;
static GLint s_text_loc_textColor = -1;
static GLint s_rect_loc_projection = -1;
static GLint s_rect_loc_rectColor = -1;

void SDLTextRendererGL_Init(const char* base_path, const char* font_path) {
    (void)font_path; // Unused, we use internal 8x8 font
    SDL_Log("Initializing OpenGL text renderer...");

    s_text_shader = create_shader_program(base_path, "shaders/text.vert", "shaders/text.frag");
    s_rect_shader = create_shader_program(base_path, "shaders/rect.vert", "shaders/rect.frag");

    s_text_loc_projection = glGetUniformLocation(s_text_shader, "projection");
    s_text_loc_textColor = glGetUniformLocation(s_text_shader, "textColor");

    s_rect_loc_projection = glGetUniformLocation(s_rect_shader, "projection");
    s_rect_loc_rectColor = glGetUniformLocation(s_rect_shader, "rectColor");

    s_font_atlas.width = 128; // 16 chars per row * 8 pixels = 128 width
    s_font_atlas.height = 64; // 8 rows of chars * 8 pixels = 64 height
    unsigned char* bitmap = (unsigned char*)calloc(1, s_font_atlas.width * s_font_atlas.height);

    // Unpack font8x8_basic into the texture atlas
    // The font map is 128 characters, each is 8 bytes.
    // We arrange them in a 16x8 grid of characters in the texture.
    for (int ch = 0; ch < 128; ch++) {
        int cx = (ch % 16) * 8;
        int cy = (ch / 16) * 8;

        for (int row = 0; row < 8; row++) {
            uint8_t row_data = font8x8_basic[ch][row];
            for (int col = 0; col < 8; col++) {
                if (row_data & (1 << col)) {
                    // Extract bit (font8x8 stores LSB to MSB for left-to-right columns)
                    bitmap[(cy + row) * s_font_atlas.width + (cx + col)] = 255;
                }
            }
        }
    }

    glGenTextures(1, &s_font_atlas.texture_id);
    glBindTexture(GL_TEXTURE_2D, s_font_atlas.texture_id);
    // ⚡ Bolt: Immutable texture storage for font atlas (created once, never resized)
    glTexStorage2D(GL_TEXTURE_2D, 1, GL_R8, s_font_atlas.width, s_font_atlas.height);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, s_font_atlas.width, s_font_atlas.height, GL_RED, GL_UNSIGNED_BYTE, bitmap);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    free(bitmap);

    glGenVertexArrays(1, &s_text_vao);
    glGenBuffers(1, &s_text_vbo);
    glBindVertexArray(s_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, NULL, GL_DYNAMIC_DRAW);
    // Position attribute
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    // Texture coordinate attribute
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

void SDLTextRendererGL_Shutdown() {
    glDeleteProgram(s_text_shader);
    glDeleteProgram(s_rect_shader);
    glDeleteTextures(1, &s_font_atlas.texture_id);
    glDeleteBuffers(1, &s_text_vbo);
    glDeleteVertexArrays(1, &s_text_vao);
}

void SDLTextRendererGL_DrawText(const char* text, float x, float y, float scale, float r, float g, float b,
                                float target_width, float target_height) {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(s_text_shader);

    // Projection matrix based on target dimensions
    const float projection[4][4] = { { 2.0f / target_width, 0.0f, 0.0f, 0.0f },
                                     { 0.0f, -2.0f / target_height, 0.0f, 0.0f },
                                     { 0.0f, 0.0f, -1.0f, 0.0f },
                                     { -1.0f, 1.0f, 0.0f, 1.0f } };
    glUniformMatrix4fv(s_text_loc_projection, 1, GL_FALSE, &projection[0][0]);
    glUniform3f(s_text_loc_textColor, r, g, b);

    glBindTexture(GL_TEXTURE_2D, s_font_atlas.texture_id);
    glBindVertexArray(s_text_vao);

    // Apply global Y offset so callers can shift all text up/down easily
    y += s_text_y_offset;

    // Helper to calculate character bounding box in unscaled coordinates
    float glyph_w = 8.0f;
    float glyph_h = 10.0f;
    float x_advance = 7.0f;

    // If background enabled, compute bounding box for the full string and draw a rect behind it
    if (s_bg_enabled) {
        float minx = 1e9f, miny = 1e9f, maxx = -1e9f, maxy = -1e9f;
        const char* pp;
        float tx = 0, ty = 0;
        for (pp = text; *pp; pp++) {
            if (*pp < 32 || *pp >= 127 || *pp == ' ') {
                // Non-printable or unmapped character
                if (*pp == ' ')
                    tx += x_advance;
            } else {
                if (tx < minx)
                    minx = tx;
                if (ty < miny)
                    miny = ty;
                if (tx + glyph_w > maxx)
                    maxx = tx + glyph_w;
                if (ty + glyph_h > maxy)
                    maxy = ty + glyph_h;
                tx += x_advance;
            }
        }

        if (minx <= maxx && miny <= maxy) {
            // apply scale and padding
            float px = s_bg_padding;
            float x0 = x + (minx * scale) - px;
            float y0 = y + (miny * scale) - px;
            float x1 = x + (maxx * scale) + px;
            float y1 = y + (maxy * scale) + px;

            // Build rectangle vertices (with dummy texcoords)
            float rect_verts[] = {
                x0, y1, 0.0f, 0.0f, x1, y1, 0.0f, 0.0f, x1, y0, 0.0f, 0.0f,

                x1, y0, 0.0f, 0.0f, x0, y0, 0.0f, 0.0f, x0, y1, 0.0f, 0.0f,
            };

            // Draw rect using rect shader
            glUseProgram(s_rect_shader);
            glUniformMatrix4fv(s_rect_loc_projection, 1, GL_FALSE, &projection[0][0]);
            glUniform4f(s_rect_loc_rectColor, s_bg_color[0], s_bg_color[1], s_bg_color[2], s_bg_color[3]);

            glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
            glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(rect_verts), rect_verts);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            glBindVertexArray(s_text_vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            // restore text shader before drawing glyphs
            glUseProgram(s_text_shader);
            glUniformMatrix4fv(s_text_loc_projection, 1, GL_FALSE, &projection[0][0]);
            glUniform3f(s_text_loc_textColor, r, g, b);
            glBindTexture(GL_TEXTURE_2D, s_font_atlas.texture_id);
            glBindVertexArray(s_text_vao);
        }
    }

    const char* p;
    float current_rx = 0;
    float current_ry = 0;

    for (p = text; *p; p++) {
        unsigned char ch = *p;
        if (ch == ' ') {
            current_rx += x_advance;
            continue;
        }

        // Map to 128-char atlas (ASCII only)
        if (ch >= 128)
            ch = 127;

        float u0 = (ch % 16) * 8.0f / s_font_atlas.width;
        float v0 = (ch / 16) * 8.0f / s_font_atlas.height;
        float u1 = u0 + (8.0f / s_font_atlas.width);
        float v1 = v0 + (8.0f / s_font_atlas.height);

        float x0 = x + (current_rx * scale);
        float y0 = y + (current_ry * scale);
        float x1 = x + ((current_rx + glyph_w) * scale);
        float y1 = y + ((current_ry + glyph_h) * scale);

        float vertices[] = {
            x0, y1, u0, v1, x1, y1, u1, v1, x1, y0, u1, v0,

            x1, y0, u1, v0, x0, y0, u0, v0, x0, y1, u0, v1,
        };

        glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
        glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(vertices), vertices);
        glBindBuffer(GL_ARRAY_BUFFER, 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);

        current_rx += x_advance;
    }

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void SDLTextRendererGL_Flush(void) {
    // Immediate mode; nothing to flush
}

void SDLTextRendererGL_SetYOffset(float y_offset) {
    s_text_y_offset = y_offset;
}

void SDLTextRendererGL_SetBackgroundEnabled(int enabled) {
    s_bg_enabled = enabled ? 1 : 0;
}

void SDLTextRendererGL_SetBackgroundColor(float r, float g, float b, float a) {
    s_bg_color[0] = r;
    s_bg_color[1] = g;
    s_bg_color[2] = b;
    s_bg_color[3] = a;
}

void SDLTextRendererGL_SetBackgroundPadding(float px) {
    s_bg_padding = px;
}
