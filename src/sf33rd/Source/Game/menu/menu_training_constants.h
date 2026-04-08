/**
 * @file menu_training_constants.h
 * Named constants for training menu magic numbers.
 *
 * Extracted from menu_training.c during modernization.
 */

#ifndef MENU_TRAINING_CONSTANTS_H
#define MENU_TRAINING_CONSTANTS_H

/* ── UI Rendering Parameters ─────────────────────────────────────── */

/** Normal tone-down darken value for training menu backgrounds. */
#define TRAINING_TONE_NORMAL 0xAA

/** Darker tone-down value for initial menu opening. */
#define TRAINING_TONE_DARK 0x80

/** Y coordinate for the training menu header text. */
#define TRAINING_HEADER_POS_Y 0x18

/** X coordinate for Dummy Setting effect alignment. */
#define TRAINING_DUMMY_SETTING_X 0xE6

/* ── Game State and Control ──────────────────────────────────────── */

/** Control player index indicating "no player" or disabled control. */
#define TRAINING_CONTROL_NONE 0x63

/** Mask used for reading menu lever inputs in MC_Move_Sub. */
#define TRAINING_RESULT_MASK 0xFF

/** Frames for cover timer when starting match from training. */
#define TRAINING_COVER_TIMER 0x17

/* ── Menu Layout / Coordinates ───────────────────────────────────── */

#define TRAINING_NORMAL_START_X 120
#define TRAINING_NORMAL_START_Y 56

#define TRAINING_DUMMY_SETTING_LBL_X 48
#define TRAINING_DUMMY_SETTING_START_Y 80

#define TRAINING_OPTION_LBL_X 48
#define TRAINING_OPTION_VAL_X 230
#define TRAINING_OPTION_START_Y 72

#define TRAINING_BLOCKING_START_X 112
#define TRAINING_BLOCKING_START_Y 72

#define TRAINING_BLOCKING_OPT_HDR_X 51
#define TRAINING_BLOCKING_OPT_HDR_1_Y 56
#define TRAINING_BLOCKING_OPT_HDR_2_Y 106
#define TRAINING_BLOCKING_OPT_LBL_X 64
#define TRAINING_BLOCKING_OPT_VAL_X 264

#define TRAINING_CTRL_REMOVED_MSG_X 132
#define TRAINING_CTRL_REMOVED_MSG_Y 82
#define TRAINING_CTRL_REMOVED_MSG_COLOR 16

#define TRAINING_SPACING_Y 16

/* ── Timers ──────────────────────────────────────────────────────── */

#define TRAINING_CHARACTER_CHANGE_TIMER 10
#define TRAINING_RESET_WAIT_TIMER 2
#define TRAINING_RESET_G_TIMER 10
#define TRAINING_RESET_COVER_TIMER 5

#endif /* MENU_TRAINING_CONSTANTS_H */
