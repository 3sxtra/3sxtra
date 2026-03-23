#include "port/renderer_plugin.h"
#include "port/sdl/renderer/sdl_texture_util.h"
#include "sf33rd/AcrSDK/ps2/flps2render.h"

#include <SDL3/SDL.h>
#include <stdarg.h>

/* ================================================================
 * Global state
 * ================================================================ */

renderer_export_t* g_renderer_plugin = NULL;

static SDL_SharedObject* plugin_handle = NULL;

/* ================================================================
 * Import function implementations
 * ================================================================ */

static void import_log(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char buf[1024];
    SDL_vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    SDL_Log("[renderer] %s", buf);
}

static float import_conv_screen_fz(float z) {
    return flPS2ConvScreenFZ(z);
}

/* ================================================================
 * Plugin loading
 * ================================================================ */

bool RendererPlugin_Load(const char* plugin_name, int argc, const char** argv) {
    if (g_renderer_plugin != NULL) {
        SDL_Log("Renderer plugin already loaded");
        return true;
    }

    if (plugin_name == NULL || plugin_name[0] == '\0') {
        return false;
    }

    /* Build DLL path from plugin name */
    char dll_path[512];
    const char* base = SDL_GetBasePath();
#ifdef _WIN32
    SDL_snprintf(dll_path, sizeof(dll_path), "%s%s.dll", base ? base : "", plugin_name);
    /* MinGW adds a 'lib' prefix by default — try that too */
    if (!SDL_GetPathInfo(dll_path, NULL)) {
        SDL_snprintf(dll_path, sizeof(dll_path), "%slib%s.dll", base ? base : "", plugin_name);
    }
#else
    SDL_snprintf(dll_path, sizeof(dll_path), "%slib%s.so", base ? base : "", plugin_name);
    // macOS fallback
    #ifdef __APPLE__
    if (!SDL_GetPathInfo(dll_path, NULL)) {
        SDL_snprintf(dll_path, sizeof(dll_path), "%slib%s.dylib", base ? base : "", plugin_name);
    }
    #endif
#endif

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "Loading renderer plugin: %s", dll_path);

    /* Load the shared library */
    plugin_handle = SDL_LoadObject(dll_path);
    if (plugin_handle == NULL) {
        SDL_Log("Renderer plugin not found or failed to load: %s (%s)", dll_path, SDL_GetError());
        return false;
    }

    /* Look up the entry point */
    GetRendererAPI_fn get_api = (GetRendererAPI_fn)SDL_LoadFunction(plugin_handle, "GetRendererAPI");
    if (get_api == NULL) {
        SDL_Log("Renderer plugin missing GetRendererAPI: %s", SDL_GetError());
        SDL_UnloadObject(plugin_handle);
        plugin_handle = NULL;
        return false;
    }

    static renderer_import_t imports = {
        .Log = import_log,
        .ConvScreenFZ = import_conv_screen_fz,
        .TextureLoad = TextureUtil_Load,
        .TextureLoadScaled = TextureUtil_LoadScaled,
        .TextureFree = TextureUtil_Free,
        .TextureGetSize = TextureUtil_GetSize,
        .TextureDrawQuadEx = TextureUtil_DrawQuadEx,
        .cps3_width = 384,
        .cps3_height = 224,
    };

    g_renderer_plugin = get_api(&imports);

    if (g_renderer_plugin == NULL || g_renderer_plugin->api_version != RENDERER_PLUGIN_API_VERSION) {
        SDL_Log("Renderer plugin failed initialization or version mismatch");
        SDL_UnloadObject(plugin_handle);
        plugin_handle = NULL;
        g_renderer_plugin = NULL;
        return false;
    }

    if (!g_renderer_plugin->Init(argc, argv)) {
        SDL_Log("Renderer plugin Init() returned false");
        SDL_UnloadObject(plugin_handle);
        plugin_handle = NULL;
        g_renderer_plugin = NULL;
        return false;
    }

    SDL_Log("Renderer plugin initialized successfully (scale=%d)", g_renderer_plugin->render_scale);
    return true;
}

void RendererPlugin_Unload(void) {
    if (g_renderer_plugin != NULL) {
        if (g_renderer_plugin->Shutdown) {
            g_renderer_plugin->Shutdown();
        }
        g_renderer_plugin = NULL;
    }

    if (plugin_handle != NULL) {
        SDL_UnloadObject(plugin_handle);
        plugin_handle = NULL;
        SDL_Log("Renderer plugin unloaded");
    }
}
