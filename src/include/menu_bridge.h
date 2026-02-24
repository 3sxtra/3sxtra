#ifndef MENU_BRIDGE_H
#define MENU_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MENU_BRIDGE_SHM_NAME "3SX_MENU_BRIDGE_SHM"

/*
 * Input bitmasks matching engine's SWKey (include/sf33rd/AcrSDK/common/pad.h)
 * Duplicated here for external tool reference.
 */
#define MENU_INPUT_UP (1 << 0)
#define MENU_INPUT_DOWN (1 << 1)
#define MENU_INPUT_LEFT (1 << 2)
#define MENU_INPUT_RIGHT (1 << 3)
#define MENU_INPUT_LP (1 << 4)        // SWK_WEST
#define MENU_INPUT_MP (1 << 5)        // SWK_NORTH
#define MENU_INPUT_HP (1 << 6)        // SWK_RIGHT_SHOULDER
#define MENU_INPUT_UNUSED_1 (1 << 7)  // SWK_LEFT_SHOULDER
#define MENU_INPUT_LK (1 << 8)        // SWK_SOUTH (Confirm in menus)
#define MENU_INPUT_MK (1 << 9)        // SWK_EAST
#define MENU_INPUT_HK (1 << 10)       // SWK_RIGHT_TRIGGER
#define MENU_INPUT_UNUSED_2 (1 << 11) // SWK_LEFT_TRIGGER
#define MENU_INPUT_START (1 << 14)    // SWK_START
#define MENU_INPUT_SELECT (1 << 15)   // SWK_BACK

#pragma pack(push, 1)
typedef struct MenuBridgeState {
    /* === NAVIGATION STATE === */
    uint8_t nav_G_No[4];       /* Main game state: [major, sub1, sub2, sub3] */
    uint8_t nav_S_No[4];       /* Selection state machine */
    uint8_t nav_Play_Type;     /* 0=Arcade, 1=Versus, 2=Training */
    uint8_t nav_Play_Game;     /* 0=in menus, 1-2=in gameplay */
    uint8_t nav_My_char[2];    /* COMMITTED selected characters [P1, P2] */
    uint8_t nav_Super_Arts[2]; /* Selected super arts [P1, P2] */

    /* Real-time cursor feedback */
    int8_t nav_Cursor_X[2];    /* Grid X */
    int8_t nav_Cursor_Y[2];    /* Grid Y */
    int8_t nav_Cursor_Char[2]; /* Character ID UNDER CURSOR */

    /* Control flags */
    uint8_t menu_input_active; /* 1=Python controls inputs */

    /* Input buffers (injected when menu_input_active=1) */
    uint16_t p1_input;
    uint16_t p2_input;

    /* Frame counter (for external tools to sync to game frames) */
    uint32_t frame_count;

    /* Combat-active flag: 1 when Allow_a_battle_f is set (round active, players can move) */
    uint8_t allow_battle;

    /* SA cursor position (0=SA1, 1=SA2, 2=SA3) — populated from Arts_Y[] */
    int8_t nav_Cursor_SA[2];

    /* Screen sub-state (for FIGHT banner detection: C_No[0]==1, C_No[1]==4) */
    uint8_t nav_C_No[4];

    /* Reserved for future expansion (alignment padding) */
    uint8_t _reserved[53];

} MenuBridgeState;
#pragma pack(pop)

/* API Declarations */
void MenuBridge_Init(const char* shm_suffix);
void MenuBridge_PreTick(void);
void MenuBridge_PostTick(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_BRIDGE_H */
