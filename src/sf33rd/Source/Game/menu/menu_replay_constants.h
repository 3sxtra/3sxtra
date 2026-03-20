/**
 * @file menu_replay_constants.h
 * Named constants for replay menu magic numbers.
 *
 * Extracted from menu_replay.c during modernization.
 */

#ifndef MENU_REPLAY_CONSTANTS_H
#define MENU_REPLAY_CONSTANTS_H

/* ── UI Rendering Parameters ─────────────────────────────────────── */

/** Normal tone-down darken value for replay menus. */
#define REPLAY_TONE_NORMAL          192

/** Darker tone-down darken value for replay transitions. */
#define REPLAY_TONE_DARK            232

/* ── UI Element Positioning ──────────────────────────────────────── */

/** X coordinate for the pause menu group 1. */
#define REPLAY_PAUSE_X1             0x82
/** Y coordinate for the pause menu group 1. */
#define REPLAY_PAUSE_Y1             0x48

/** X coordinate for the pause menu group 2. */
#define REPLAY_PAUSE_X2             0x88
/** Y coordinate for the pause menu group 2. */
#define REPLAY_PAUSE_Y2             0x58

/** Header Z-order for replay setups. */
#define REPLAY_Z_HEADER             0x3F

/** First parameter for replay pause menu rendering (effect_A3). */
#define REPLAY_PAUSE_PARAM1         0x17

/** Second parameter for replay pause menu rendering (effect_A3). */
#define REPLAY_PAUSE_PARAM2         0x63

/* ── Effect Sprites ──────────────────────────────────────────────── */

/** Sprite ID for setup background. */
#define REPLAY_SPRITE_SETUP_BG      8

/** Sprite ID for after-match background. */
#define REPLAY_SPRITE_AFTER_BG      38

/** Sprite ID for replay picker background. */
#define REPLAY_SPRITE_PICKER_BG     41

/** Sprite ID for pause menu background. */
#define REPLAY_SPRITE_PAUSE_BG      0x0A

/* ── Effect Z-Depths ─────────────────────────────────────────────── */

/** Z-depth for replay setup cursor element. */
#define REPLAY_Z_DEPTH_SETUP        (-0x7FF4)

/** Z-depth for post-replay cursor element. */
#define REPLAY_Z_DEPTH_AFTER        (-0x7FF7)

/** Z-depth for post-replay marker element. */
#define REPLAY_Z_DEPTH_MARKER       0x7047

/** Z-depth for replay picker background element. */
#define REPLAY_Z_DEPTH_PICKER       (-0x7FF3)

/** Z-depth for replay pause cursor element. */
#define REPLAY_Z_DEPTH_PAUSE        (-0x3FF6)

/* ── Input and State ─────────────────────────────────────────────── */

/** Mask used for reading menu lever inputs in MC_Move_Sub. */
#define REPLAY_RESULT_MASK          0xFF

/** Special file property property to set for all saves. */
#define REPLAY_FILE_PROPERTY_ALL    0xFF

#endif /* MENU_REPLAY_CONSTANTS_H */
