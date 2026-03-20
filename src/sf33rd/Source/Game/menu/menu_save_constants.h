/**
 * @file menu_save_constants.h
 * Named constants for save menu magic numbers.
 *
 * Extracted from menu_save.c during modernization.
 */

#ifndef MENU_SAVE_CONSTANTS_H
#define MENU_SAVE_CONSTANTS_H

/* ── Effect Z-Depths ─────────────────────────────────────────────── */

/** Z-depth for the save menu cursor element. */
#define SAVE_Z_DEPTH_CURSOR     (-0x7FFD)

/* ── Menu r_no state indices (caller context) ────────────────────── */

/** r_no[1] value for system direction menu. */
#define MENU_RNO1_SYS_DIR       0x05
/** r_no[1] value for load replay menu. */
#define MENU_RNO1_LOAD_REPLAY   0x06
/** r_no[1] value for memory card menu. */
#define MENU_RNO1_MEM_CARD      0x0D
/** r_no[1] value for save replay menu. */
#define MENU_RNO1_SAVE_REPLAY   0x11
/** r_no[1] value for direction menu. */
#define MENU_RNO1_DIR_MENU      0x12
/** r_no[1] value for auto save exit. */
#define MENU_RNO1_SYS_SAVE      0x17

#endif /* MENU_SAVE_CONSTANTS_H */
