/**
 * @file staff_constants.h
 * Named constants for staff credits roll magic numbers.
 *
 * Extracted from staff.c during modernization task #43.
 * Pure definitions — no logic, no dependencies beyond stdint-compatible types.
 */

#ifndef STAFF_CONSTANTS_H
#define STAFF_CONSTANTS_H

/* ── Credit string markers ───────────────────────────────────────── */

/** Section-break marker character ('?') in credit name string. */
#define STAFF_MARKER_SECTION 0x3F
/** Consumer-section marker character ('`') in credit name string. */
#define STAFF_MARKER_CONSUMER 0x60

/* ── BG layer configuration ─────────────────────────────────────── */

/** BG work layer index for the credits scroll. */
#define STAFF_BGW_LAYER 5
/** Cal-enable flag written to bgw[].xy[].cal. */
#define STAFF_CAL_ENABLE 0x01000000
/** Base X position for the credits BG (256 px). */
#define STAFF_POS_X_BASE 0x100
/** Horizontal centering reference (pixels). */
#define STAFF_CENTER_X 192
/** Family layer index paired with the credits scroll. */
#define STAFF_FAMILY_LAYER 6
/** Family Y scroll range. */
#define STAFF_FAMILY_Y 0x300
/** 16-bit coordinate mask for scroll calculations. */
#define STAFF_MASK_16 0xFFFF

/* ── Timing constants ────────────────────────────────────────────── */

/** Initial name display timer (max s16). */
#define STAFF_TIMER_MAX 0x7FFF
/** Fallback timer for triple-line burst (240 frames). */
#define STAFF_TIMER_TRIPLE 0xF0
/** Default credit-line display lifetime (frames). */
#define STAFF_NAME_LIFETIME 240

/* ── BGM fade rates ──────────────────────────────────────────────── */

/** BGM fade-out rate at end of credits. */
#define STAFF_BGM_FADE_END 0x88
/** BGM fade-out rate at arcade → consumer section transition. */
#define STAFF_BGM_FADE_SECTION 0x4E

#endif /* STAFF_CONSTANTS_H */
