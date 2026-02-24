#ifndef TRAINING_MENU_H
#define TRAINING_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

extern bool show_training_menu;

typedef struct {
    bool show_hitboxes; // Main toggle
    bool show_pushboxes;
    bool show_hurtboxes;
    bool show_attackboxes;
    bool show_throwboxes;
    bool show_advantage;
    bool show_stun;
    bool show_inputs;
    bool show_frame_meter;
} TrainingMenuSettings;

extern TrainingMenuSettings g_training_menu_settings;

void training_menu_init(void);
void training_menu_render(int window_width, int window_height);
void training_menu_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif // TRAINING_MENU_H
