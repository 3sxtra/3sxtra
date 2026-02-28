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

    /* Step-mode control (for frame-precise replay injection) */
    uint8_t step_mode_active; /* 1=game waits for step_requested each frame */
    uint8_t step_requested;   /* Python sets 1, C clears after frame runs */
    uint8_t selfplay_active;  /* 1=inject P2 inputs during gameplay too */

    /* === FORCE MATCH INJECTION === */
    uint8_t force_match_active; /* 1=Python requests immediate Jump to Game2_0 */
    uint8_t fm_p1_char;
    uint8_t fm_p2_char;
    uint8_t fm_p1_sa;
    uint8_t fm_p2_sa;
    uint8_t fm_stage;
    int16_t fm_rng_16;
    int16_t fm_rng_32;
    int16_t fm_rng_16_ex;
    int16_t fm_rng_32_ex;

    /* === GAME STATE (for parity testing) === */
    uint16_t game_timer; /* Round timer (Game_timer) */
    int16_t p1_health;   /* P1 vitality (plw[0].wu.vitality) */
    int16_t p2_health;   /* P2 vitality (plw[1].wu.vitality) */
    int16_t p1_pos_x;    /* P1 X position */
    int16_t p1_pos_y;    /* P1 Y position */
    int16_t p2_pos_x;    /* P2 X position */
    int16_t p2_pos_y;    /* P2 Y position */
    int16_t p1_facing;   /* P1 direction */
    int16_t p2_facing;   /* P2 direction */
    int16_t p1_meter;    /* P1 super meter (spg_dat[0].current_spg) */
    int16_t p2_meter;    /* P2 super meter (spg_dat[1].current_spg) */
    int16_t p1_stun;     /* P1 stun value (sdat[0].cstn) */
    int16_t p2_stun;     /* P2 stun value (sdat[1].cstn) */
    uint8_t p1_busy;     /* P1 can't-act flag (plw[0].wu.do_not_move) */
    uint8_t p2_busy;     /* P2 can't-act flag (plw[1].wu.do_not_move) */

    /* === RNG STATE (for parity testing) === */
    int16_t rng_16;    /* Random_ix16 */
    int16_t rng_32;    /* Random_ix32 */
    int16_t rng_16_ex; /* Random_ix16_ex */
    int16_t rng_32_ex; /* Random_ix32_ex */

    /* === EXTENDED GAME STATE (for parity testing) === */
    uint32_t p1_action;    /* plw[0].wu.routine_no[0..1] packed as u32 */
    uint32_t p2_action;    /* plw[1].wu.routine_no[0..1] packed as u32 */
    uint16_t p1_animation; /* plw[0].wu.cg_number */
    uint16_t p2_animation; /* plw[1].wu.cg_number */
    uint8_t p1_posture;    /* plw[0].guard_flag */
    uint8_t p2_posture;    /* plw[1].guard_flag */
    uint8_t p1_freeze;     /* plw[0].wu.hit_stop (clamped to u8) */
    uint8_t p2_freeze;     /* plw[1].wu.hit_stop (clamped to u8) */
    uint8_t is_in_match;   /* Allow_a_battle_f (explicit for parity CSV) */

} MenuBridgeState;
#pragma pack(pop)

/* API Declarations */
void MenuBridge_Init(const char* shm_suffix);
void MenuBridge_StepGate(void);
void MenuBridge_PreTick(void);
void MenuBridge_PostTick(void);

#ifdef __cplusplus
}
#endif

#endif /* MENU_BRIDGE_H */
