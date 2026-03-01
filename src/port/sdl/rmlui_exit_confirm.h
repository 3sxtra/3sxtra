#pragma once
/**
 * @file rmlui_exit_confirm.h
 * @brief RmlUi Exit Confirmation screen — replaces CPS3 sprite-based
 *        "Select Game" / exit-to-desktop buttons in toSelectGame().
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the exit confirm data model and document. */
void rmlui_exit_confirm_init(void);

/** Per-frame dirty-check sync. */
void rmlui_exit_confirm_update(void);

/** Show the exit confirm document. */
void rmlui_exit_confirm_show(void);

/** Hide the exit confirm document. */
void rmlui_exit_confirm_hide(void);

/** Destroy the data model. */
void rmlui_exit_confirm_shutdown(void);

#ifdef __cplusplus
}
#endif
