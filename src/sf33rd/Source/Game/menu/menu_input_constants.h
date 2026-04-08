/**
 * @file menu_input_constants.h
 * Named constants for menu input handling magic numbers.
 *
 * Extracted from menu_input.c during modernization task #44.
 * Pure definitions — no logic, no dependencies beyond stdint-compatible types.
 */

#ifndef MENU_INPUT_CONSTANTS_H
#define MENU_INPUT_CONSTANTS_H

/* ── Effect Order slot indices ───────────────────────────────────── */

/** Menu header banner (effect_57). */
#define EFF_SLOT_HEADER 0x4E
/** Extra option header label (effect_57). */
#define EFF_SLOT_EX_HEADER 0x73
/** Sys-dir label "GAME" (effect_66). */
#define EFF_SLOT_DIR_GAME 0x5B
/** Sys-dir label row 1 / Ex-option row 1 (effect_66). */
#define EFF_SLOT_DIR_ROW1 0x5C
/** Sys-dir label row 2 / Ex-option row 2 (effect_66). */
#define EFF_SLOT_DIR_ROW2 0x5D
/** Cursor highlight / selection background (effect_66). */
#define EFF_SLOT_CURSOR_BG 0x8A
/** Pause button config column 1 (effect_66). */
#define EFF_SLOT_PAUSE_BTN1 0x8B
/** Pause button config column 2 (effect_66). */
#define EFF_SLOT_PAUSE_BTN2 0x8C
/** Mode select screen header (effect_57 transition-out). */
#define EFF_SLOT_SCREEN_ADJ 0x65
/** Game option sub-menu indicator. */
#define EFF_SLOT_GAME_OPT 0x6A
/** Button config sub-menu indicator. */
#define EFF_SLOT_BTN_CFG 0x6B
/** Memory card sub-menu indicator. */
#define EFF_SLOT_MEM_CARD 0x69
/** Replay list header. */
#define EFF_SLOT_REPLAY_HDR 0x6E
/** Replay/direction mode header. */
#define EFF_SLOT_DIR_HDR 0x70
/** Save/load completion indicator. */
#define EFF_SLOT_SAVE_DONE 0x78
/** P1 character portrait (replay). */
#define EFF_SLOT_P1_PORTRAIT 0x0B
/** P2 character portrait (replay). */
#define EFF_SLOT_P2_PORTRAIT 0x0C
/** P1 SA gauge frame (replay). */
#define EFF_SLOT_P1_SA 0x23
/** P2 SA gauge frame (replay). */
#define EFF_SLOT_P2_SA 0x24
/** P1 name plate (replay). */
#define EFF_SLOT_P1_NAME 0x11
/** P2 name plate (replay). */
#define EFF_SLOT_P2_NAME 0x12
/** P1 win mark frame (replay). */
#define EFF_SLOT_P1_WIN 0x1D
/** P2 win mark frame (replay). */
#define EFF_SLOT_P2_WIN 0x1E
/** Stage name banner (replay). */
#define EFF_SLOT_STAGE_NAME 0x2A
/** Replay marker base slot. */
#define EFF_SLOT_REPLAY_MARKER 0x50

/* ── Effect sprite / layer IDs (2nd arg to effect_66_init etc.) ── */

/** Sys-dir page indicator sprite. */
#define EFF_SPRITE_PAGE_IND 0x13
/** Sys-dir "GAME" label sprite. */
#define EFF_SPRITE_DIR_GAME 0x14
/** Sys-dir "DIFFICULTY" label sprite. */
#define EFF_SPRITE_DIR_DIFF 0x15
/** Sys-dir "ROUND" label sprite. */
#define EFF_SPRITE_DIR_ROUND 0x16
/** Extra-option "SOUND" label sprite. */
#define EFF_SPRITE_EX_SOUND 0x27
/** Extra-option "DISPLAY" label sprite. */
#define EFF_SPRITE_EX_DISP 0x28

/* ── Effect_40 selector slots ────────────────────────────────────── */

#define EFF40_SEL_0 0x48
#define EFF40_SEL_1 0x49
#define EFF40_SEL_2 0x4A
#define EFF40_SEL_3 0x4B

/* ── Z-order / layer depths ──────────────────────────────────────── */

/** Message Z-order for menu header. */
#define MENU_Z_HEADER 0x45
/** Label list Z-order. */
#define MENU_Z_LABEL_LIST 0x47
/** Page number Z-order. */
#define MENU_Z_PAGE_NUM 0x40
/** Extra-option header Z-order. */
#define MENU_Z_EX_HEADER 0x3F

/* ── effect_66 Z-order for pause overlay ─────────────────────────── */

/** Pause dim overlay Z-depth. */
#define EFF66_ZORDER_PAUSE (-0x8000)
/** In-game button config overlay Z-depth. */
#define EFF66_ZORDER_INGAME (-0x3FFC)
/** In-game button config column Z-depth. */
#define EFF66_ZORDER_INGAME_COL (-0x3FFB)

/* ── Effect sprite counts / Order_Dir values ─────────────────────── */

/** Order_Dir value: "title" label group. */
#define ORDERDIR_TITLE_LABEL 0x0A
/** Order_Dir value: "sub" label group. */
#define ORDERDIR_SUB_LABEL 0x0B

/* ── Message position Y ──────────────────────────────────────────── */

/** Y position for single-line description text. */
#define MSG_POS_Y_SINGLE 0x36
/** Y position for multi-line description text. */
#define MSG_POS_Y_MULTI 0x3E

/* ── Message request offsets ─────────────────────────────────────── */

/** Offset added to direction page value for message request. */
#define MSG_DIR_PAGE_OFFSET 0x74
/** Stride per page in sys-dir message table. */
#define DIR_PAGE_STRIDE 0x0C

/* ── BG scroll offsets and speeds ────────────────────────────────── */

/** Standard horizontal BG slide distance (512 px). */
#define BG_SLIDE_X_FULL 0x200
/** Medium horizontal BG slide (384 px). */
#define BG_SLIDE_X_MEDIUM 0x180
/** Fixed-point BG scroll speed (forward). */
#define BG_SPEED_FORWARD 0x333333
/** Fixed-point BG scroll speed (backward, negated). */
#define BG_SPEED_BACKWARD (-0x333333)
/** Fixed-point BG move speed (forward, secondary). */
#define BG_MOVE_FORWARD 0x266666
/** Fixed-point BG move speed (backward, secondary). */
#define BG_MOVE_BACKWARD 0xFFD9999A

/* ── Fade parameters ─────────────────────────────────────────────── */

/** Fully opaque alpha target. */
#define FADE_OPAQUE 0xFF
/** Slow fade-in speed. */
#define FADE_SPEED_SLOW 0x19

/* ── Palette offsets ─────────────────────────────────────────────── */

/** BG palette code offset for replay match screen. */
#define PAL_OFFSET_REPLAY 0x90

/* ── Purge / resource keys ───────────────────────────────────────── */

/** Memory purge key for replay-related resources. */
#define PURGE_KEY_REPLAY 0x0C

/* ── Replay info ─────────────────────────────────────────────────── */

/** Slot index in Rep_Game_Infor array for current replay data. */
#define REPLAY_INFO_SLOT 0x0A

/* ── Input bitmasks ──────────────────────────────────────────────── */

/** Mask to strip pause/select bits from pad input. */
#define INPUT_MASK_NO_PAUSE 0x4FFF
/** Mask for move-sub result (up/down/button flags). */
#define RESULT_MASK 0x20F

/* ── Menu r_no state indices ─────────────────────────────────────── */

/** r_no[1] value for extra-option mode. */
#define MENU_RNO1_EXTRA_OPTION 0x0E
/** r_no[0] value for replay-exit branch. */
#define MENU_RNO0_REPLAY_EXIT 0x0C
/** r_no[2] value for pause-exit confirmation. */
#define MENU_RNO2_PAUSE_EXIT 0x63

/* ── Menu max cursor positions (function params) ─────────────────── */

/** Max cursor row for direction menu. */
#define DIR_CURSOR_MAX 4
/** Max cursor row for game options. */
#define GAME_OPTION_MAX 0x0B
/** Max cursor row for button config. */
#define BUTTON_CONFIG_MAX 0x0A
/** Max cursor row for screen adjust. */
#define SCREEN_ADJUST_MAX 6
/** Max cursor row for sound test. */
#define SOUND_TEST_MAX 6
/** Max cursor row for memory card. */
#define MEMORY_CARD_MAX 3

/* ── Game pause state ────────────────────────────────────────────── */

/** Game_pause value: paused with dim overlay active. */
#define GAME_PAUSE_ACTIVE 0x81

/* ── dispButtonImage2 parameters ─────────────────────────────────── */

/** X position for "START" button icon. */
#define BTN_IMG_POS_X 0xB2
/** Y position for "START" button icon. */
#define BTN_IMG_POS_Y 0x5B

/* ── effect_10 positional IDs ────────────────────────────────────── */

/* Pause menu text positions (effect_10_init pos_id / layer args). */
#define EFF10_PAUSE_CONTINUE 0x14
#define EFF10_PAUSE_EXIT_VS 0x10
#define EFF10_PAUSE_EXIT_RPL 0x15
#define EFF10_PAUSE_EXIT_DEF 0x11
#define EFF10_PAUSE_BTNCFG 0x16

#define EFF10_PAUSE_CONFIRM 0x13
#define EFF10_PAUSE_YES 0x14
#define EFF10_PAUSE_NO 0x1A

/* Layer values for effect_10_init. */
#define EFF10_LAYER_CONTINUE 0x0C
#define EFF10_LAYER_EXIT 0x0E
#define EFF10_LAYER_BTNCFG 0x10
#define EFF10_LAYER_CONFIRM 0x0C
#define EFF10_LAYER_YESNO 0x0F

/* ── Replay timer ────────────────────────────────────────────────── */

/** Initial replay-load timer value. */
#define REPLAY_LOAD_TIMER 0x0A

/* ── Extra-option message request offset ─────────────────────────── */

/** Message request offset for extra-option page tab descriptions. */
#define MSG_EX_PAGE_OFFSET 32

/* ── BGM request codes ───────────────────────────────────────────── */

/** BGM code for mode-select screen. */
#define BGM_CODE_MODE_SELECT 0x41

/* ── effect_58 sub-type ──────────────────────────────────────────── */

/** Effect_58 sub-type for scroll transition. */
#define EFF58_SCROLL_TYPE 0x0E

#endif /* MENU_INPUT_CONSTANTS_H */
