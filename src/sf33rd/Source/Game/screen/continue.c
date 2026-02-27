/**
 * @file continue.c
 * Manages the "Continue" screen, countdown, and player input.
 */

#include "common.h"
#include "sf33rd/Source/Game/debug/Debug.h"
#include "sf33rd/Source/Game/effect/eff49.h"
#include "sf33rd/Source/Game/effect/eff58.h"
#include "sf33rd/Source/Game/effect/eff76.h"
#include "sf33rd/Source/Game/effect/eff95.h"
#include "sf33rd/Source/Game/effect/effa9.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/sound/se.h"
#include "sf33rd/Source/Game/stage/bg.h"
#include "sf33rd/Source/Game/stage/bg_data.h"

#define CONTINUE_JMP_COUNT 5

u8 CONTINUE_X;

static void Continue_1st();
static void Continue_2nd();
static void Continue_3rd();
static void Continue_4th();
static void Continue_5th();
static void Setup_Continue_OBJ();
static s16 Check_Exit_Continue();

/** @brief Main continue-screen dispatcher — runs the current sub-state and returns exit flag. */
s32 Continue_Scene() {
    void (*Continue_Jmp_Tbl[CONTINUE_JMP_COUNT])() = {
        Continue_1st, Continue_2nd, Continue_3rd, Continue_4th, Continue_5th
    };

    CONTINUE_X = 0;
    if (Cont_No[0] < CONTINUE_JMP_COUNT) {
        Continue_Jmp_Tbl[Cont_No[0]]();
    }

    if ((Check_Exit_Check() == 0) && (Debug_w[DEBUG_TIME_STOP] == -1)) {
        CONTINUE_X = 0;
    }

    return CONTINUE_X;
}

/** @brief Continue phase 1 — set up BG scroll, spawn countdown/effects, wait for scene readiness. */
static void Continue_1st() {
    switch (Cont_No[1]) {
    case 0:
        Cont_No[1] += 1;
        Target_BG_X[3] = bg_w.bgw[3].wxy[0].disp.pos + 0x1CA;
        Target_BG_X[1] = bg_w.bgw[1].wxy[0].disp.pos + 0x1CA;
        Offset_BG_X[3] = 0;
        Offset_BG_X[1] = 0;
        bg_mvxy.a[0].sp = 0xE0000;
        bg_mvxy.d[0].sp = 0;
        Next_Step = 0;

        Setup_Continue_OBJ();
        effect_A9_init(0x37, 0, 0x13, 0);
        BGM_Request(58);
        spawn_effect_76(0x38, 3, 1);
        effect_58_init(0xC, 1, 3);
        effect_58_init(0xC, 1, 1);
        Suicide[2] = 1;
        effect_58_init(0x10, 5, 2);

        break;

    case 1:
        if (Next_Step) {
            Cont_No[1] += 1;
            Cont_Timer = 0x14;
        }

        break;

    case 2:
        if (Scene_Cut) {
            Cont_Timer = 1;
        }

        if (--Cont_Timer == 0) {
            Cont_No[0] += 1;
            Cont_No[1] = 0;
            Continue_Count_Down[LOSER] = 0;
        }

        break;
    }
}

/** @brief Continue phase 2 — wait until the continue countdown expires. */
static void Continue_2nd() {
    if (Continue_Count[LOSER] < 0) {
        Cont_No[0] += 1;
    }
}

/** @brief Continue phase 3 — wait for exit conditions from Check_Exit_Continue. */
static void Continue_3rd() {
    if ((Cont_Timer = Check_Exit_Continue())) {
        Cont_No[0] += 1;
    }
}

/** @brief Continue phase 4 — countdown delay before signaling exit. */
static void Continue_4th() {
    if (--Cont_Timer == 0) {
        Cont_No[0] += 1;
        CONTINUE_X = 1;
    }
}

/** @brief Continue phase 5 — immediate exit (fallback). */
static void Continue_5th() {
    CONTINUE_X = 1;
}

/** @brief Spawn all visual effects/objects for the continue screen (portraits, labels, panels). */
static void Setup_Continue_OBJ() {
    effect_49_init(4);
    effect_49_init(8);

    effect_95_init(4);
    effect_95_init(8);
    effect_95_init(1);
    effect_95_init(2);

    spawn_effect_76(0x3B, 3, 1);
    spawn_effect_76(0x3C, 3, 1);
    spawn_effect_76(0x3D, 3, 1);
    spawn_effect_76(0x3E, 3, 1);
    spawn_effect_76(0x3F, 3, 1);
}

/** @brief Check whether both fighters have finished their exit animations; returns frame delay or 0. */
static s16 Check_Exit_Continue() {
    if (((E_Number[0][0]) == 2) || ((E_Number[1][0]) == 2)) {
        return 0;
    }

    if (E_Number[LOSER ^ 1][0] == 0) {
        return 0x3C;
    }

    if ((E_Number[LOSER ^ 1][0] != 3) && (E_Number[LOSER ^ 1][0] != 0)) {
        return 0;
    }

    if ((E_Number[LOSER][0] != 3) && (E_Number[LOSER][0] != 0)) {
        return 0;
    }

    return 1;
}
