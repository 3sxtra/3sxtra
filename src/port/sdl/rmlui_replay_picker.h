#pragma once
/**
 * @file rmlui_replay_picker.h
 * @brief RmlUi Replay Save/Load picker — replaces the CPS3 replay
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

#ifdef __cplusplus
}
#endif
