#pragma once

#include <stdbool.h>

// Forward declare SDL types to avoid including SDL headers in C code
struct SDL_Window;
union SDL_Event;

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize RmlUi context and backend (renderer selected based on SDLApp_GetRenderer())
void rmlui_wrapper_init(struct SDL_Window* window, void* gl_context);

/// Shut down RmlUi and free all resources
void rmlui_wrapper_shutdown(void);

/// Route an SDL event to the RmlUi context
void rmlui_wrapper_process_event(union SDL_Event* event);

/// Begin a new RmlUi frame (update context)
void rmlui_wrapper_new_frame(void);

/// Render the current RmlUi frame
void rmlui_wrapper_render(void);

/// Whether RmlUi wants to capture mouse input
bool rmlui_wrapper_want_capture_mouse(void);

/// Whether RmlUi wants to capture keyboard input
bool rmlui_wrapper_want_capture_keyboard(void);

/// Get the RmlUi context (returns Rml::Context* as void* for C compatibility)
void* rmlui_wrapper_get_context(void);

/// Load and show a named RmlUi document from assets/ui/
void rmlui_wrapper_show_document(const char* name);

/// Hide a named RmlUi document
void rmlui_wrapper_hide_document(const char* name);

/// Check if a named RmlUi document is currently visible
bool rmlui_wrapper_is_document_visible(const char* name);

/// Close and destroy a named RmlUi document (frees resources)
void rmlui_wrapper_close_document(const char* name);

/// Reload stylesheets for all loaded documents (preserves document state)
void rmlui_wrapper_reload_stylesheets(void);

/// Fully reload a named document from disk (resets state)
void rmlui_wrapper_reload_document(const char* name);

/// Fully reload all loaded documents from disk (resets state)
void rmlui_wrapper_reload_all_documents(void);

/// Release and force reload of all textures
void rmlui_wrapper_release_textures(void);

#ifdef __cplusplus
}
#endif
