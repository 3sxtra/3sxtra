/**
 * @file rmlui_wrapper.h
 * @brief C-callable RmlUi context management and document API.
 */
#pragma once

#include <stdbool.h>

// Forward declare SDL types to avoid including SDL headers in C code
struct SDL_Window;
union SDL_Event;

#ifdef __cplusplus
extern "C" {
#endif

#ifdef ENABLE_RMLUI

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

/// Hide all window-context RmlUi documents
void rmlui_wrapper_hide_all_documents(void);

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

// ─── Game Context (Phase 3 screens — renders at window resolution) ───

void* rmlui_wrapper_get_game_context(void);
void rmlui_wrapper_show_game_document(const char* name);
void rmlui_wrapper_hide_game_document(const char* name);
void rmlui_wrapper_hide_all_game_documents(void);
bool rmlui_wrapper_is_game_document_visible(const char* name);
bool rmlui_wrapper_any_game_visible(void);
bool rmlui_wrapper_any_window_visible(void);
void rmlui_wrapper_set_window_popup_visible(bool visible);
void rmlui_wrapper_close_game_document(const char* name);
void rmlui_wrapper_update_game(void);
void rmlui_wrapper_render_game(int win_w, int win_h, float view_x, float view_y, float view_w, float view_h);
float rmlui_wrapper_get_par_correct_y(void);

#else /* !ENABLE_RMLUI — inline no-ops */

static inline void  rmlui_wrapper_init(struct SDL_Window* w, void* gl) { (void)w; (void)gl; }
static inline void  rmlui_wrapper_shutdown(void) {}
static inline void  rmlui_wrapper_process_event(union SDL_Event* e) { (void)e; }
static inline void  rmlui_wrapper_new_frame(void) {}
static inline void  rmlui_wrapper_render(void) {}
static inline bool  rmlui_wrapper_want_capture_mouse(void) { return false; }
static inline bool  rmlui_wrapper_want_capture_keyboard(void) { return false; }
static inline void* rmlui_wrapper_get_context(void) { return (void*)0; }
static inline void  rmlui_wrapper_show_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_hide_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_hide_all_documents(void) {}
static inline bool  rmlui_wrapper_is_document_visible(const char* n) { (void)n; return false; }
static inline void  rmlui_wrapper_close_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_reload_stylesheets(void) {}
static inline void  rmlui_wrapper_reload_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_reload_all_documents(void) {}
static inline void  rmlui_wrapper_release_textures(void) {}
static inline void* rmlui_wrapper_get_game_context(void) { return (void*)0; }
static inline void  rmlui_wrapper_show_game_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_hide_game_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_hide_all_game_documents(void) {}
static inline bool  rmlui_wrapper_is_game_document_visible(const char* n) { (void)n; return false; }
static inline bool  rmlui_wrapper_any_game_visible(void) { return false; }
static inline bool  rmlui_wrapper_any_window_visible(void) { return false; }
static inline void  rmlui_wrapper_set_window_popup_visible(bool v) { (void)v; }
static inline void  rmlui_wrapper_close_game_document(const char* n) { (void)n; }
static inline void  rmlui_wrapper_update_game(void) {}
static inline void  rmlui_wrapper_render_game(int ww, int wh, float vx, float vy, float vw, float vh) {
    (void)ww; (void)wh; (void)vx; (void)vy; (void)vw; (void)vh;
}
static inline float rmlui_wrapper_get_par_correct_y(void) { return 1.0f; }

#endif /* ENABLE_RMLUI */

#ifdef __cplusplus
}
#endif
