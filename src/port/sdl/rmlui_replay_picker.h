#pragma once
/**
 * @file rmlui_replay_picker.h
 * @brief RmlUi Replay Save/Load picker — replaces the ImGui replay
 *        file list and confirmation dialogs with an HTML/CSS overlay.
 */

#ifdef __cplusplus
extern "C" {
#endif

void rmlui_replay_picker_init(void);
void rmlui_replay_picker_update(void);
void rmlui_replay_picker_show(void);
void rmlui_replay_picker_hide(void);
void rmlui_replay_picker_shutdown(void);

/** Open the picker (0=load, 1=save). Shows document + populates slot data. */
void rmlui_replay_picker_open(int mode);

/** Poll the picker state. Returns 1=active, 0=slot selected, -1=cancelled. */
int rmlui_replay_picker_poll(void);

/** After poll() returns 0, get the selected slot index (0..19). */
int rmlui_replay_picker_get_slot(void);

#ifdef __cplusplus
}
#endif
