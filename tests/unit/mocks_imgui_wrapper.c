#include <stddef.h>

/* ─── TextureUtil (new standalone API) ─── */

void* TextureUtil_Load(const char* filename) {
    if (filename == NULL) return NULL;
    return (void*)0x1234; // Dummy texture handle
}

void TextureUtil_Free(void* texture_id) {
    (void)texture_id; // No-op in test mock
}

void TextureUtil_GetSize(void* texture_id, int* w, int* h) {
    if (texture_id == (void*)0x1) { // Left mock (from test_bezel_layout)
        if (w) *w = 100;
        if (h) *h = 200; // Aspect 0.5
    } else if (texture_id == (void*)0x2) { // Right mock (from test_bezel_layout)
        if (w) *w = 150;
        if (h) *h = 200; // Aspect 0.75
    } else {
        if (w) *w = 500;
        if (h) *h = 1080;
    }
}

void TextureUtil_Shutdown(void) {
    // No-op in test mock
}

/* ─── Legacy imgui_wrapper names (backward compat wrappers) ─── */

void* imgui_wrapper_load_texture(const char* filename) {
    return TextureUtil_Load(filename);
}

void imgui_wrapper_free_texture(void* texture_id) {
    TextureUtil_Free(texture_id);
}

void imgui_wrapper_get_texture_size(void* texture_id, int* w, int* h) {
    TextureUtil_GetSize(texture_id, w, h);
}