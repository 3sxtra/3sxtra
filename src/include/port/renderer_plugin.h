#ifndef RENDERER_PLUGIN_H
#define RENDERER_PLUGIN_H

/**
 * Renderer Plugin System
 *
 * Function-pointer-table plugin architecture for swappable rendering via DLL/shared library.
 * The base code defines the interface; a plugin DLL implements it.
 * If no plugin is loaded, all function pointers are NULL and the
 * base code renders normally using the standard tile-based path.
 */

#include <stdbool.h>

#define RENDERER_PLUGIN_API_VERSION 3

/* ================================================================
 * renderer_export_t — Functions the plugin DLL provides
 * ================================================================ */
typedef struct renderer_export_t {
    int api_version;

    /* Lifecycle — receives argc/argv so the plugin can parse its own args */
    bool (*Init)(int argc, const char** argv);
    void (*Shutdown)(void);

    /* Configuration — set by the plugin, read by the base code after Init. */
    int render_scale; /* Desired canvas scale (e.g. 4 for HD). */

    /**
     * Try to render a full-sprite override for the given group/cg.
     * Loads the override texture and pushes it to the render queue if found.
     *
     * @param group_index  Texture group index.
     * @param cg_number    CG animation frame number.
     * @param screen_x     Screen-space X position (post-transform).
     * @param screen_y     Screen-space Y position (post-transform).
     * @param z            Z depth for sorting.
     * @param flip_x       Non-zero to flip horizontally.
     * @param color        ARGB vertex color.
     * @param screen_w     Expected CPS3-space quad width.
     * @param screen_h     Expected CPS3-space quad height.
     * @return true if an override was rendered, false to fall through to standard rendering.
     */
    bool (*TryRenderSprite)(int group_index, int cg_number, float screen_x, float screen_y, float z, int flip_x,
                            unsigned int color, float screen_w, float screen_h);

    /* Background tile overrides */
    void* (*LoadBGTileOverride)(int type, int stage, int gbix);
    void (*DrawBGTile)(void* texture, float x, float y, float w, float h, float z, unsigned int vtxCol);
    void (*ClearBGTileCache)(void);

    /* Sprite cache management */
    void (*ClearSpriteCache)(void);

    /**
     * Try to override a texture by its engine handle pair.
     * Called from SetTexture() before normal texture binding.
     * The override PNG is loaded at native resolution (premultiplied).
     *
     * @param texture_handle  Low 16 bits of the combined texture code (1-based).
     * @param palette_handle  High 16 bits of the combined texture code (0 = no palette).
     * @return Opaque TextureUtil handle to use instead, or NULL to fall through.
     */
    void* (*TryOverrideTexture)(unsigned int texture_handle, unsigned int palette_handle);
    void (*ClearTextureOverrideCache)(void);

} renderer_export_t;

/* ================================================================
 * renderer_import_t — Functions/data the base code provides to the DLL
 * ================================================================ */
typedef struct renderer_import_t {
    /* Logging */
    void (*Log)(const char* fmt, ...);

    /* Coordinate conversion (wraps flPS2ConvScreenFZ) */
    float (*ConvScreenFZ)(float z);

    /**
     * Engine's texture utility functions.
     * The plugin must use these to load and draw textures so they are
     * compatible with all rendering backends (OpenGL, SDLGPU, Classic, etc).
     */
    void* (*TextureLoad)(const char* path);
    void* (*TextureLoadScaled)(const char* path, float scale);
    void (*TextureFree)(void* texture_id);
    void (*TextureGetSize)(void* texture_id, int* w, int* h);
    void (*TextureDrawQuadEx)(void* texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y);

    /* Constants */
    int cps3_width;
    int cps3_height;
} renderer_import_t;

/* ================================================================
 * DLL entry point signature
 * ================================================================ */
typedef renderer_export_t* (*GetRendererAPI_fn)(const renderer_import_t* import);

/* ================================================================
 * Plugin loader API (base code side)
 * ================================================================ */

/** Global plugin state. NULL when no plugin is loaded. */
extern renderer_export_t* g_renderer_plugin;

/** Returns true if a renderer plugin is loaded and active. */
#define RENDERER_HAS_PLUGIN() (g_renderer_plugin != NULL)

/**
 * Attempt to load a renderer plugin DLL.
 *
 * @param plugin_name   Plugin name (e.g. "renderer_hd"). Loads lib<name>.dll/.so next to executable.
 * @param argc          Original argument count from main.
 * @param argv          Original argument values from main (unmodified copy).
 * @return true if the plugin was loaded and initialized successfully.
 */
bool RendererPlugin_Load(const char* plugin_name, int argc, const char** argv);

/** Unload the current renderer plugin, if any. */
void RendererPlugin_Unload(void);

#endif /* RENDERER_PLUGIN_H */
