/**
 * @file cmd_constants.h
 * @brief Named constants for input parsing bitmasks used by the command system.
 *
 * These replace raw hex literals in cmd_main.c for readability.
 * All values correspond to the CPS3 input encoding.
 */

#ifndef CMD_CONSTANTS_H
#define CMD_CONSTANTS_H

/* ── Lever Direction Masks ─────────────────────────────────────────── */

#define CMD_LEVER_MASK       0xF     /* Lower 4 bits: joystick direction */
#define CMD_LEVER_LR         0xC     /* Left/right lever bits */
#define CMD_LEVER_DOWN       0x8     /* Down direction bit */

/* ── Button Masks ──────────────────────────────────────────────────── */

#define CMD_BTN_LP           0x10    /* Light punch */
#define CMD_BTN_PUNCHES      0x70    /* All 3 punch buttons (LP|MP|HP) */
#define CMD_BTN_LK           0x100   /* Light kick */
#define CMD_BTN_KICKS        0x700   /* All 3 kick buttons (LK|MK|HK) */
#define CMD_BTN_ATTACKS      0x770   /* All 6 attack buttons */
#define CMD_BTN_KICKS_ALT    0x780   /* Kick group (alternate mask, check_7) */

/* ── Flip / Attribute Flags ────────────────────────────────────────── */

#define CMD_FLIP_X           0x8000  /* Horizontal flip */

/* ── Multi-Press Detection ─────────────────────────────────────────── */

#define CMD_MULTI_PUNCH      0x80    /* Multi-punch press flag */
#define CMD_MULTI_KICK       0x800   /* Multi-kick press flag */
#define CMD_MULTI_PUNCH_CLR  0xFF7F  /* Clear multi-punch flag */
#define CMD_MULTI_KICK_CLR   0xF7FF  /* Clear multi-kick flag */

/* ── Simultaneous Button Pairs (pl_lvr_set combo detection) ────────── */

#define CMD_SW_2BTN_LP_LK    0x110   /* LP+LK simultaneous */
#define CMD_SW_2BTN_MP_MK    0x220   /* MP+MK simultaneous */
#define CMD_SW_2BTN_HP_HK    0x440   /* HP+HK simultaneous */

/* ── Switch Word Sub-Masks (pl_lvr_set) ────────────────────────────── */

#define CMD_SW_PUNCH_MASK    0xF0    /* Punch group in combined switch word */
#define CMD_SW_KICK_MASK     0xF00   /* Kick group in combined switch word */
#define CMD_SW_LR_CLR        0xFF3   /* Clear left/right bits mask */

/* ── Charge / Holding Thresholds ───────────────────────────────────── */

#define CMD_BTN_MP           0x20    /* Medium punch */
#define CMD_BTN_HP           0x40    /* Hard punch */
#define CMD_BTN_MK           0x200   /* Medium kick */
#define CMD_BTN_HK           0x400   /* Hard kick */

/* ── Multi-Press Switch Patterns (pl_lvr_set) ──────────────────────── */

#define CMD_SW_2P_30         0x30    /* Two-punch combo pattern */
#define CMD_SW_2P_50         0x50    /* Two-punch combo pattern */
#define CMD_SW_2P_60         0x60    /* Two-punch combo pattern */
#define CMD_SW_3P_70         0x70    /* Three-punch combo pattern (= CMD_BTN_PUNCHES) */
#define CMD_SW_2K_300        0x300   /* Two-kick combo pattern */
#define CMD_SW_2K_500        0x500   /* Two-kick combo pattern */
#define CMD_SW_2K_600        0x600   /* Two-kick combo pattern */
#define CMD_SW_3K_700        0x700   /* Three-kick combo pattern (= CMD_BTN_KICKS) */

#endif /* CMD_CONSTANTS_H */
