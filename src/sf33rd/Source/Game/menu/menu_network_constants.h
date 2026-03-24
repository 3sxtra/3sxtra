/**
 * @file menu_network_constants.h
 * Named constants for network menu UI magic numbers.
 *
 * Extracted from menu_network.c during modernization task #45.
 * Pure definitions — no logic, no dependencies beyond stdint-compatible types.
 */

#ifndef MENU_NETWORK_CONSTANTS_H
#define MENU_NETWORK_CONSTANTS_H

/* ── UI ARGB Colors ─────────────────────────────────────────────── */

/** Semi-transparent black overlay (fullscreen dim). */
#define NET_COLOR_OVERLAY_DIM   0xA0000000
/** Dark popup box background. */
#define NET_COLOR_BOX_BG        0xE0181818
/** Vibrant red border / banner. */
#define NET_COLOR_BORDER_RED    0xFFCC0000
/** Solid white (borders). */
#define NET_COLOR_WHITE         0xFFFFFFFF
/** Accept button text (green). */
#define NET_COLOR_ACCEPT        0xFF00FF00
/** Decline/cancel button text (red). */
#define NET_COLOR_DECLINE       0xFFFF0000
/** Grey description banner background. */
#define NET_COLOR_DESC_BG       0x80202020

/* ── Screen-fill overlay geometry ────────────────────────────────── */

/** Screen overlay left edge (overbleed). */
#define NET_SCREEN_LEFT         (-2)
/** Screen overlay right edge (overbleed). */
#define NET_SCREEN_RIGHT        386
/** Screen overlay top edge (overbleed). */
#define NET_SCREEN_TOP          (-2)
/** Screen overlay bottom edge (overbleed). */
#define NET_SCREEN_BOTTOM       226

/* ── Popup box geometry ──────────────────────────────────────────── */

#define NET_POPUP_LEFT          60
#define NET_POPUP_RIGHT         324
#define NET_POPUP_TOP           56
#define NET_POPUP_BOTTOM        168

/* ── Popup border geometry ───────────────────────────────────────── */

/** Top border Y (1px above popup box). */
#define NET_BORDER_TOP_Y0       55
#define NET_BORDER_TOP_Y1       57
/** Bottom border Y (1px below popup box). */
#define NET_BORDER_BOT_Y0       167
#define NET_BORDER_BOT_Y1       169

/* ── Banner geometry (red header bar borders) ────────────────────── */

/** White top border Y for header bar. */
#define NET_HDR_TOP_BORDER_Y0   14
#define NET_HDR_TOP_BORDER_Y1   15
/** White bottom border Y for header bar. */
#define NET_HDR_BOT_BORDER_Y0   41
#define NET_HDR_BOT_BORDER_Y1   42

/* ── Description banner geometry ─────────────────────────────────── */

#define NET_DESC_TOP            175
#define NET_DESC_BOTTOM         213

/* ── Effect Order slot indices ───────────────────────────────────── */
/* EFF_SLOT_HEADER (0x4E) and EFF_SLOT_CURSOR_BG (0x8A) are in
 * menu_input_constants.h — do NOT redefine here. */

/** Network menu title label (effect_61, "NETWORK LOBBY"). */
#define EFF_SLOT_NET_TITLE      0x5F
/** Second grey overlay box (Internet peer area). */
#define EFF_SLOT_LOBBY_BOX2     0x8B
/** Network/direction header banner (effect_57). */
#define EFF_SLOT_NET_HDR        0x70

/* ── Effect init font/layout codes ───────────────────────────────── */

/** CG large font layout (effect_61_init). */
#define EFF_FONT_CG_LARGE       0x7047
/** Compact 8px font layout (effect_61_init). */
#define EFF_FONT_COMPACT         0x70A7

/* ── Effect Z-order / palette codes ──────────────────────────────── */

/** Z-order for network header banner. */
#define EFF_Z_NETWORK_HDR        0x3F
/** Blue background palette code. */
#define EFF_Z_BLUE_BG            0x45
/** Cursor type for 4-item gateway menu (effect_04_init). */
#define NET_CURSOR_TYPE_GATEWAY  7
/** Cursor slot ID for gateway (effect_04_init). */
#define NET_CURSOR_SLOT_GATEWAY  0x48

/* ── effect_66 Z-order for lobby overlay ─────────────────────────── */

/** Grey peer area box Z-depth (LAN). */
#define EFF66_ZORDER_LOBBY_LAN   (-0x7FF0)
/** Grey peer area box Z-depth (Internet). */
#define EFF66_ZORDER_LOBBY_NET   (-0x7FF1)

/* ── Order_Timer / Order_Dir base values ─────────────────────────── */

/** Base timer offset for menu item fade-in. */
#define NET_ORDER_TIMER_BASE     0x14
/** Timer for title label fade-in. */
#define NET_ORDER_TIMER_TITLE    0x12

/* ── Menu item string indices ────────────────────────────────────── */

/** First string index for gateway items (74=LOBBY, 75=LOCAL, 76=LB, 77=REPLAYS, 78=EXIT). */
#define NET_STR_GATEWAY_BASE     74
/** String index for "NETWORK LOBBY" title label. */
#define NET_STR_LOBBY_TITLE      67

/* ── Message system positioning ──────────────────────────────────── */

/** Message Y position for description text. */
#define NET_MSG_POS_Y            0x3E
/** Message Z-order for description text. */
#define NET_MSG_POS_Z            0x44
/** Base message request for lobby description. */
#define NET_MSG_REQ_BASE         35

/* ── Lobby mode kinds (task_ptr->free[2]) ────────────────────────── */

/** Native text-mode lobby. */
#define NET_MODE_NATIVE          0
/** RmlUI lobby. */
#define NET_MODE_RMLUI           1
/** LAN-only lobby. */
#define NET_MODE_LAN             2

/* ── Background mode request ─────────────────────────────────────── */

/** Message_Data->kind_req value for blue background. */
#define NET_BG_MODE_BLUE         4

/* ── BG setup constants ──────────────────────────────────────────── */

/** BG scroll distance for Virtual/Setup_BG calls. */
#define NET_BG_SCROLL            0x200

/* ── Fade parameters ─────────────────────────────────────────────── */
/* FADE_OPAQUE (0xFF) and FADE_SPEED_SLOW (0x19) are in
 * menu_input_constants.h — reuse, do not redefine. */

/* ── dispButtonImage2 parameters ─────────────────────────────────── */

/** Accept button icon X. */
#define NET_BTN_A_X              0x6C
/** Accept/Cancel button icon Y. */
#define NET_BTN_Y                0x8E
/** Cancel button icon X (incoming popup). */
#define NET_BTN_B_X              0xC0
/** Cancel-only button icon X (outgoing popup). */
#define NET_BTN_CANCEL_X         0x98
/** Button sprite width arg. */
#define NET_BTN_SPRITE_W         0x13
/** Button sprite height arg. */
#define NET_BTN_SPRITE_H         0x0F

/* ── SSPutStr / SSPutStrPro text positions ───────────────────────── */

/** Accept text X position. */
#define NET_TEXT_ACCEPT_X        128
/** Decline text X position. */
#define NET_TEXT_DECLINE_X       216
/** Cancel text X position. */
#define NET_TEXT_CANCEL_X        176
/** Accept/Decline/Cancel text Y position. */
#define NET_TEXT_BTN_Y           144

/* ── Slide-in animation ─────────────────────────────────────────── */

/** Initial slide-in offset for SSPutStr elements. */
#define NET_SLIDE_OFFSET_INIT    384

/* ── Native lobby text layout ────────────────────────────────────── */

/** X position for peer info column. */
#define NET_PEER_INFO_X          200
/** Y position for LAN peer row (full lobby). */
#define NET_LAN_PEER_Y_FULL      63
/** Y position for NET peer row (full lobby). */
#define NET_NET_PEER_Y           115
/** Y position for LAN peer row (LAN-only lobby). */
#define NET_LAN_PEER_Y_LANONLY   77
/** Y position for LAN header text. */
#define NET_LAN_HDR_Y            50
/** Y position for NET header text. */
#define NET_NET_HDR_Y            102
/** Y position for LAN toggle value. */
#define NET_LAN_TOGGLE_Y         63
/** Y position for NET auto-connect toggle. */
#define NET_NET_AUTOCONN_Y       115
/** Y position for NET auto-search toggle. */
#define NET_NET_AUTOSRCH_Y       129
/** X position for toggle value text. */
#define NET_TOGGLE_X             136
/** X position for status line text. */
#define NET_STATUS_X             40
/** Y position for status line text. */
#define NET_STATUS_Y             215
/** Peer name sub-row offset (below FOUND/ONLINE count). */
#define NET_PEER_SUBROW          15
/** Horizontal screen width for centering. */
#define NET_SCREEN_WIDTH_PX      384

/* ── Insert_Y for network mode ───────────────────────────────────── */

#define NET_INSERT_Y             23

/* ── Lobby overlay sprite ID ─────────────────────────────────────── */

/** effect_66 sprite ID for grey overlay box. */
#define EFF_SPRITE_LOBBY_BOX     42

#endif /* MENU_NETWORK_CONSTANTS_H */
