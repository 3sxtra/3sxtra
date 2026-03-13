/**
 * @file sdl_text_renderer_gl.c
 * @brief OpenGL TrueType text renderer using stb_truetype.
 *
 * Rasterizes bitmap fonts from TTF files, renders text as textured quads
 * with per-character glyph caching, and provides debug overlay text
 * with optional background rectangles.
 */
#include "port/sdl/renderer/sdl_text_renderer.h"
#include "port/sdl/renderer/sdl_text_renderer_internal.h"

#include <string.h>

#include "types.h"
#include <SDL3/SDL.h>
#include <glad/gl.h>

#include "port/imgui_font_8x8.h"
#include "port/sdl/app/sdl_app.h"

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
    unsigned char* bitmap = (unsigned char*)SDL_calloc(1, s_font_atlas.width * s_font_atlas.height);

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

    SDL_free(bitmap);

    glGenVertexArrays(1, &s_text_vao);
    glGenBuffers(1, &s_text_vbo);
    glBindVertexArray(s_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
    // ⚡ Pi4: Keep VBO small (1 char = 96 bytes) to avoid V3D buffer orphaning
    // overhead in per-character glBufferSubData calls. Batch rendering in
    // SDLTextRendererGL_DrawDebugChars uses glBufferData(GL_STREAM_DRAW) instead.
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
            glBufferData(GL_ARRAY_BUFFER, sizeof(rect_verts), rect_verts, GL_STREAM_DRAW);
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

    // ⚡ Bolt: Batched character rendering — eliminates O(N) glDrawArrays and glBufferSubData calls
    #define MAX_BATCH_CHARS 1024
    static float batch_vertices[MAX_BATCH_CHARS * 6 * 4];
    int batch_count = 0;

    for (p = text; *p; p++) {
        unsigned char ch = *p;
        if (ch == ' ') {
            current_rx += x_advance;
            continue;
        }
        // Skip non-printable control characters
        if (ch < 32)
            continue;

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

        int offset = batch_count * 24; // 6 vertices * 4 floats

        // Triangle 1
        batch_vertices[offset + 0] = x0;
        batch_vertices[offset + 1] = y1;
        batch_vertices[offset + 2] = u0;
        batch_vertices[offset + 3] = v1;
        batch_vertices[offset + 4] = x1;
        batch_vertices[offset + 5] = y1;
        batch_vertices[offset + 6] = u1;
        batch_vertices[offset + 7] = v1;
        batch_vertices[offset + 8] = x1;
        batch_vertices[offset + 9] = y0;
        batch_vertices[offset + 10] = u1;
        batch_vertices[offset + 11] = v0;

        // Triangle 2
        batch_vertices[offset + 12] = x1;
        batch_vertices[offset + 13] = y0;
        batch_vertices[offset + 14] = u1;
        batch_vertices[offset + 15] = v0;
        batch_vertices[offset + 16] = x0;
        batch_vertices[offset + 17] = y0;
        batch_vertices[offset + 18] = u0;
        batch_vertices[offset + 19] = v0;
        batch_vertices[offset + 20] = x0;
        batch_vertices[offset + 21] = y1;
        batch_vertices[offset + 22] = u0;
        batch_vertices[offset + 23] = v1;

        batch_count++;
        current_rx += x_advance;

        // Flush batch if full
        if (batch_count >= MAX_BATCH_CHARS) {
            glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
            glBufferData(GL_ARRAY_BUFFER, batch_count * 24 * sizeof(float), batch_vertices, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, batch_count * 6);
            batch_count = 0;
        }
    }

    if (batch_count > 0) {
        glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);
        glBufferData(GL_ARRAY_BUFFER, batch_count * 24 * sizeof(float), batch_vertices, GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, batch_count * 6);
    }

    #undef MAX_BATCH_CHARS
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
}

void SDLTextRendererGL_Flush(void) {
    // Batched within DrawText; no external flush needed
}

// ⚡ Pi4 PERF FIX: Batched debug text rendering.
// Renders the entire PS2 debug text buffer with just 2 draw calls (shadow + foreground)
// instead of 2×N individual DrawText calls (each with ~20 GL state changes).
// On Pi4 V3D with 200 characters, this cuts GL calls from ~8,000 to ~30.
void SDLTextRendererGL_DrawDebugChars(const void* buffer, int count, float scale,
                                      float target_width, float target_height) {
    if (count <= 0 || !buffer) return;

    // Match the PS2 flPrint buffer layout
    typedef struct { uint16_t x, y; uint32_t code, col; } DebugChar;
    const DebugChar* chars = (const DebugChar*)buffer;

    // --- ONE-TIME GL STATE SETUP ---
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(s_text_shader);

    const float projection[4][4] = {
        { 2.0f / target_width, 0.0f, 0.0f, 0.0f },
        { 0.0f, -2.0f / target_height, 0.0f, 0.0f },
        { 0.0f, 0.0f, -1.0f, 0.0f },
        { -1.0f, 1.0f, 0.0f, 1.0f }
    };
    glUniformMatrix4fv(s_text_loc_projection, 1, GL_FALSE, &projection[0][0]);
    glBindTexture(GL_TEXTURE_2D, s_font_atlas.texture_id);
    glBindVertexArray(s_text_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_text_vbo);

    float glyph_w = 8.0f, glyph_h = 10.0f;
    float y_offset = s_text_y_offset;

    // --- SHADOW PASS (all black, offset +1,+1) ---
    {
        glUniform3f(s_text_loc_textColor, 0.0f, 0.0f, 0.0f);
        static float verts[1024 * 6 * 4]; // 1024 chars max per batch

        int n = 0;
        for (int i = 0; i < count; i++) {
            unsigned char ch = (unsigned char)chars[i].code;
            if (ch < 0x20 || ch > 0x7F) continue;
            if (ch >= 128) ch = 127;

            float u0 = (ch % 16) * 8.0f / s_font_atlas.width;
            float v0 = (ch / 16) * 8.0f / s_font_atlas.height;
            float u1 = u0 + (8.0f / s_font_atlas.width);
            float v1 = v0 + (8.0f / s_font_atlas.height);

            float x0 = (float)chars[i].x * scale + 1.0f;
            float y0 = (float)chars[i].y * scale + y_offset + 1.0f;
            float x1 = x0 + glyph_w * scale;
            float y1 = y0 + glyph_h * scale;

            int o = n * 24;
            verts[o+ 0]=x0; verts[o+ 1]=y1; verts[o+ 2]=u0; verts[o+ 3]=v1;
            verts[o+ 4]=x1; verts[o+ 5]=y1; verts[o+ 6]=u1; verts[o+ 7]=v1;
            verts[o+ 8]=x1; verts[o+ 9]=y0; verts[o+10]=u1; verts[o+11]=v0;
            verts[o+12]=x1; verts[o+13]=y0; verts[o+14]=u1; verts[o+15]=v0;
            verts[o+16]=x0; verts[o+17]=y0; verts[o+18]=u0; verts[o+19]=v0;
            verts[o+20]=x0; verts[o+21]=y1; verts[o+22]=u0; verts[o+23]=v1;
            n++;

            if (n >= 1024) {
                glBufferData(GL_ARRAY_BUFFER, n * 24 * sizeof(float), verts, GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, n * 6);
                n = 0;
            }
        }
        if (n > 0) {
            glBufferData(GL_ARRAY_BUFFER, n * 24 * sizeof(float), verts, GL_STREAM_DRAW);
            glDrawArrays(GL_TRIANGLES, 0, n * 6);
        }
    }

    // --- FOREGROUND PASS (per-character color) ---
    // Most debug text uses the same color, so we batch runs of identical colors.
    {
        static float verts[1024 * 6 * 4];
        int n = 0;
        uint32_t current_col = 0xFFFFFFFF; // impossible sentinel
        int started = 0;

        for (int i = 0; i <= count; i++) {
            uint32_t col = 0;
            int printable_char = 0;

            if (i < count) {
                unsigned char ch = (unsigned char)chars[i].code;
                if (ch >= 0x20 && ch <= 0x7F) {
                    col = chars[i].col;
                    printable_char = 1;
                }
            }

            // Flush batch on color change or end-of-buffer
            if (started && (i == count || (printable_char && col != current_col))) {
                glBufferData(GL_ARRAY_BUFFER, n * 24 * sizeof(float), verts, GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, n * 6);
                n = 0;
            }

            if (i == count || !printable_char) continue;

            // Set color uniform on change
            if (col != current_col) {
                current_col = col;
                uint8_t r = (col >> 16) & 0xFF;
                uint8_t g = (col >> 8) & 0xFF;
                uint8_t b = col & 0xFF;
                r = (r < 128) ? r * 2 : 255;
                g = (g < 128) ? g * 2 : 255;
                b = (b < 128) ? b * 2 : 255;
                glUniform3f(s_text_loc_textColor, r / 255.0f, g / 255.0f, b / 255.0f);
                started = 1;
            }

            unsigned char ch = (unsigned char)chars[i].code;
            if (ch >= 128) ch = 127;
            if (n >= 1024) {
                glBufferData(GL_ARRAY_BUFFER, n * 24 * sizeof(float), verts, GL_STREAM_DRAW);
                glDrawArrays(GL_TRIANGLES, 0, n * 6);
                n = 0;
            }

            float u0 = (ch % 16) * 8.0f / s_font_atlas.width;
            float v0 = (ch / 16) * 8.0f / s_font_atlas.height;
            float u1 = u0 + (8.0f / s_font_atlas.width);
            float v1 = v0 + (8.0f / s_font_atlas.height);

            float x0 = (float)chars[i].x * scale;
            float y0 = (float)chars[i].y * scale + y_offset;
            float x1 = x0 + glyph_w * scale;
            float y1 = y0 + glyph_h * scale;

            int o = n * 24;
            verts[o+ 0]=x0; verts[o+ 1]=y1; verts[o+ 2]=u0; verts[o+ 3]=v1;
            verts[o+ 4]=x1; verts[o+ 5]=y1; verts[o+ 6]=u1; verts[o+ 7]=v1;
            verts[o+ 8]=x1; verts[o+ 9]=y0; verts[o+10]=u1; verts[o+11]=v0;
            verts[o+12]=x1; verts[o+13]=y0; verts[o+14]=u1; verts[o+15]=v0;
            verts[o+16]=x0; verts[o+17]=y0; verts[o+18]=u0; verts[o+19]=v0;
            verts[o+20]=x0; verts[o+21]=y1; verts[o+22]=u0; verts[o+23]=v1;
            n++;
        }
    }

    // --- ONE-TIME GL STATE TEARDOWN ---
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDisable(GL_BLEND);
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
