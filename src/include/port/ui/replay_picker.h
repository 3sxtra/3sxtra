#ifndef PORT_UI_REPLAY_PICKER_H
#define PORT_UI_REPLAY_PICKER_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Open the replay picker overlay.
 * @param mode 0=load, 1=save
 */
void ReplayPicker_Open(int mode);

/**
 * @brief Render one frame of the replay picker. Call from the ImGui render loop.
 * @return 1=still active, 0=completed (slot selected), -1=cancelled
 */
int ReplayPicker_Update(void);

/**
 * @brief Get the slot chosen by the user after Update() returns 0.
 * @return Slot index 0-19, or -1 if none selected.
 */
int ReplayPicker_GetSelectedSlot(void);

/**
 * @brief Check if the replay picker is currently open.
 */
int ReplayPicker_IsOpen(void);

#ifdef __cplusplus
}
#endif

#endif /* PORT_UI_REPLAY_PICKER_H */
