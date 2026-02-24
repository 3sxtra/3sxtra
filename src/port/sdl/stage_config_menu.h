#ifndef STAGE_CONFIG_MENU_H
#define STAGE_CONFIG_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

// Initialize the menu system (optional, mostly for style setup if needed)
void stage_config_menu_init(void);

// Render the config menu
void stage_config_menu_render(int window_width, int window_height);

// Shutdown/cleanup
void stage_config_menu_shutdown(void);

// Accessor for the toggle state (from main app)
extern bool show_stage_config_menu;

#ifdef __cplusplus
}
#endif

#endif // STAGE_CONFIG_MENU_H
