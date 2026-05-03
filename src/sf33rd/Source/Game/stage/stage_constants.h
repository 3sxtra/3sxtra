/**
 * @file bg_constants.h
 * Named constants for background/camera subsystem magic numbers.
 *
 * Extracted from bg_sub.c during modernization task #36.
 * Pure definitions — no logic, no dependencies beyond stdint-compatible types.
 */

#ifndef STAGE_CONSTANTS_H
#define STAGE_CONSTANTS_H

/* ── Zoom request bitmasks (cg_zoom field) ───────────────────────── */

/** Zoom disabled for this player. */
#define ZOOM_DISABLE 0x4000
/** Camera biased left. */
#define ZOOM_LOOK_LEFT 0x2000
/** Camera biased right. */
#define ZOOM_LOOK_RIGHT 0x0200
/** Camera centered on both players. */
#define ZOOM_LOOK_BOTH 0x2200
/** Horizontal zoom field mask (p1zoom & ZOOM_X_MASK). */
#define ZOOM_X_MASK 0xE200
/** Camera biased up. */
#define ZOOM_Y_LOOK_UP 0x1000
/** Camera biased down. */
#define ZOOM_Y_LOOK_DOWN 0x0100
/** Vertical centered on both. */
#define ZOOM_Y_LOOK_BOTH 0x1100
/** Vertical zoom field mask (p1zoom & ZOOM_Y_MASK). */
#define ZOOM_Y_MASK 0xD100
/** Zoom level magnitude mask. */
#define ZOOM_LEVEL_MASK 0x00FF
/** Bit-shift for look-direction nibble. */
#define ZOOM_LOOK_SHIFT 8
/** 2-bit mask for look-direction after shifting. */
#define ZOOM_LOOK_FIELD 3
/** Zoom X-chase requested flag. */
#define ZOOM_REQ_X 0x0100
/** Zoom Y-chase requested flag. */
#define ZOOM_REQ_Y 0x1000
/** X-request field mask. */
#define ZOOM_REQ_X_FIELD 0x0F00
/** Y-request field mask. */
#define ZOOM_REQ_Y_FIELD 0xF000
/** Zoom active bit. */
#define ZOOM_REQ_ACTIVE 0x0001
/** No-zoom frame level (fully zoomed out). */
#define ZOOM_FRAME_DEFAULT 64

/* ── Chase flag bitmasks ─────────────────────────────────────────── */

/** Lower nibble: X-axis chase flags. */
#define CHASE_X_MASK 0x0F
/** X-chase is actively tracking a target. */
#define CHASE_X_ACTIVE 0x01
/** X-chase is returning to neutral. */
#define CHASE_X_RETURN 0x02
/** Upper nibble: Y-axis chase flags. */
#define CHASE_Y_MASK 0xF0
/** Y-chase is actively tracking a target. */
#define CHASE_Y_ACTIVE 0x10
/** Y-chase is returning to neutral. */
#define CHASE_Y_RETURN 0x20

/* ── Scroll zone thresholds (bg_base_x_move_sub) ─────────────────── */

/** Left scroll trigger zone boundary. */
#define SCR_ZONE_LEFT 0x40
/** Left zone adjustment (SCR_ZONE_LEFT − 1). */
#define SCR_ZONE_LEFT_ADJ 0x3F
/** Right scroll zone start. */
#define SCR_ZONE_RIGHT_START 0x140
/** Right scroll zone end. */
#define SCR_ZONE_RIGHT_END 0x180
/** Max scroll work value (SCR_ZONE_RIGHT_END − 1). */
#define SCR_ZONE_MAX 0x17F

/* ── Screen geometry ─────────────────────────────────────────────── */

/** Default camera X offset (192 = 0xC0). */
#define BG_POS_OFFSET_DEFAULT 0xC0
/** Visible screen height in pixels. */
#define BG_SCREEN_HEIGHT 224
/** Tile-map wrap width. */
#define BG_WRAP_WIDTH 512
/** Family coordinate height. */
#define BG_FAMILY_HEIGHT 768
/** Y scroll dead-zone offset. */
#define BG_Y_VERTICAL_OFFSET 0x58
/** Fixed-point Y acceleration scale. */
#define BG_Y_SCALE_FACTOR 0x1C000
/** Default opaque scanline count. */
#define BG_OPAQUE_DEFAULT 224

/* ── Stage indices ───────────────────────────────────────────────── */

/** Elena's stage (Kenya) — uses different base_y_pos. */
#define STAGE_ELENA 4
/** Stages > STAGE_BONUS_THRESHOLD are bonus/ending stages. */
#define STAGE_BONUS_THRESHOLD 19
/** Parry bonus game identifier. */
#define BONUS_GAME_PARRY 0x15

/* ── Scroll movement tuning ──────────────────────────────────────── */

/** Numerator for remake_x_mvstep speed re-scaling. */
#define SCR_SPEED_NUMERATOR 0x50
/** Denominator for remake_x_mvstep speed re-scaling. */
#define SCR_SPEED_DENOMINATOR 100

#endif /* BG_CONSTANTS_H */
