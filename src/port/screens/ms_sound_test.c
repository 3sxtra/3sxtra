/**
 * @file ms_sound_test.c
 * @brief Migrated Sound Test screen — Task 13.
 *
 * Replaces the Sound_Test() function in menu.c with MenuScreen registry
 * callbacks.  This is the sound settings screen: Sound Mode (mono/stereo),
 * BGM Level, SE Level, BGM Type, Default, BGM Play/Stop, Exit.
 *
 * The screen has 7 items (cursor_max=6):
 *   Items 0–3: L/R toggle items (sound mode, BGM level, SE level, BGM type)
 *   Item 4:   "Default" — resets sound settings
 *   Item 5:   "BGM Play" — plays selected BGM track
 *   Item 6:   "Exit" — returns to Option_Select
 *
 * L/R value changes are live — they modify Convert_Buff[3][1] directly via
 * Sound_Cursor_Sub() / SD_Move_Sub_LR().  Sound mode changes are applied
 * via Setup_Sound_Mode().  BGM/SE levels are committed immediately.
 *
 * Legacy location: menu.c lines 2879–3043 (AT_Jmp_Tbl index 12).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff04.h"         /* effect_04_init */
#include "sf33rd/Source/Game/effect/eff57.h"         /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/eff61.h"         /* effect_61_init */
#include "sf33rd/Source/Game/effect/eff64.h"         /* effect_64_init */
#include "sf33rd/Source/Game/effect/effa8.h"         /* effect_A8_init */
#include "sf33rd/Source/Game/engine/workuser.h"      /* Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/menu/menu.h"            /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h"   /* Sound_Cursor_Sub, Return_Option_Mode_Sub, etc. */
#include "sf33rd/Source/Game/sound/se.h"             /* SE_selected, BGM_Request_Code_Check, BGM_Stop */
#include "sf33rd/Source/Game/sound/sound3rd.h"       /* bgm_level, se_level, SsBgmHalfVolume, SsRequest, etc. */
#include "sf33rd/Source/Game/system/reset.h"         /* Suicide */
#include "sf33rd/Source/Game/system/sys_sub.h"       /* Save_Game_Data, Clear_Flash_Sub, Clear_Flash_Init */
#include "sf33rd/Source/Game/system/work_sys.h"      /* save_w, sys_w */
#include "sf33rd/Source/Game/ui/sc_sub.h"            /* FadeOut, FadeIn, FadeInit */
#include "port/sdl/input/controller_image_overlay.h" /* ControllerImageOverlay_Init/Shutdown */
#include "structs.h"                                 /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_sound_menu.h"     /* rmlui_sound_menu_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_sound */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Sound_Test case 0
 *
 *  Sets up fade, timer, common init, sound mode buffers, header bar (0x72,
 *  MENU_HEADER_SOUND), CPS3 effect items, and RmlUi sound menu.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sound_test_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;
    s16 unused_s3;
    s16 unused_s2;

    s_wait_done = false;

    /* ── Replicate Sound_Test case 0 init pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    setupAlwaysSeamlessFlag(((plsw_00[0] | plsw_00[1]) & 0x4000) != 0);
    Clear_Flash_Init(4);
    Menu_Common_Init();
    ControllerImageOverlay_Init();
    Menu_Cursor_Y[0] = 0;
    Menu_Suicide[1] = 1;
    Menu_Suicide[2] = 0;
    Convert_Buff[3][1][5] = 0;

    /* Init sound mode convert buffers */
    if (sys_w.sound_mode == 0) {
        Convert_Buff[3][1][0] = 0;
    } else {
        Convert_Buff[3][1][0] = 1;
    }

    if (sys_w.bgm_type == BGM_ARRANGED) {
        Convert_Buff[3][1][3] = 0;
    } else {
        Convert_Buff[3][1][3] = 1;
    }

    Convert_Buff[3][1][7] = 1;

    /* Kill/setup parent effect slots */
    Order[0x4F] = 4;
    Order_Timer[0x4F] = 1;
    Order[0x4E] = 2;
    Order_Dir[0x4E] = 2;
    Order_Timer[0x4E] = 1;

    /* Header bar + item labels + value indicators — CPS3 only */
    if (use_rmlui && rmlui_menu_sound) {
        rmlui_sound_menu_show();
    } else {
        effect_57_init(0x72, MENU_HEADER_SOUND, 0, 0x3F, 2);
        Order[0x72] = 1;
        Order_Dir[0x72] = 8;
        Order_Timer[0x72] = 1;
        effect_04_init(2, 6, 2, 0x48);

        {
            s32 ixSoundMenuItem[4] = { 10, 11, 11, 12 };

            for (ix = 0; ix < 4; ix++) {
                Order[ix + 0x57] = 1;
                Order_Dir[ix + 0x57] = 4;
                Order_Timer[ix + 0x57] = ix + 0x14;
                effect_64_init(ix + 0x57, 0, 2, ixSoundMenuItem[ix] + 1, ix, 0x7047, ix + 0xC, 3, 1);
            }
        }

        Order_Dir[0x78] = 0;
        effect_A8_init(0, 0x78, 0, 2, 5, 0x70A7, 0);
        Order_Dir[0x79] = 1;
        effect_A8_init(0, 0x79, 0, 2, 5, 0x70A7, 1);
        effect_A8_init(3, 0x7A, 0, 2, 5, 0x70A7, 3);
        Convert_Buff[3][1][5] = 0;
        Order_Dir[0x7B] = 0;
        effect_A8_init(2, 0x7B, 0, 2, 5, 0x70A7, 2);

        for (ix = 0, unused_s3 = char_index = 0x3B; ix < 7; ix++, unused_s2 = char_index++) {
            effect_61_init(0, ix + 0x50, 0, 2, char_index, ix, 0x7047);
            Order[ix + 0x50] = 1;
            Order_Dir[ix + 0x50] = 4;
            Order_Timer[ix + 0x50] = ix + 0x14;
        }

        Menu_Cursor_Move = 5;
    }

    /* ── Set r_no[1] to 12 for legacy compatibility ── */
    task_ptr->r_no[1] = 12;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Sound_Test case 3
 *
 *  Input handling: Sound_Cursor_Sub(0/1) for both players handles up/down
 *  cursor movement and L/R value toggles.  Live updates to BGM/SE levels,
 *  BGM type, and sound mode.  BGM play/stop on item 5.  Exit on item 6
 *  or cancel button.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sound_test_tick(struct _TASK* task_ptr) {
    u8 last_mode;

    /* ── One-time post-wait-phase setup ── */
    if (!s_wait_done) {
        s_wait_done = true;
        Suicide[3] = 0;
    }

    /* Clear_Flash_Sub is called every frame in Sound_Test (before the switch) */
    Clear_Flash_Sub();

    /* ── Input handling — same as legacy Sound_Test case 3 ── */
    last_mode = Convert_Buff[3][1][0];
    Sound_Cursor_Sub(0);

    if (IO_Result == 0) {
        Sound_Cursor_Sub(1);
    }

    /* Item 4: Default — reset sound settings */
    if ((Menu_Cursor_Y[0] == 4) && (IO_Result == 0x100)) {
        SE_selected();
        Convert_Buff[3][1][0] = 0;
        Convert_Buff[3][1][1] = 0xF;
        Convert_Buff[3][1][2] = 0xF;
        Convert_Buff[3][1][3] = 0;
    }

    /* Live-apply BGM level */
    if (bgm_level != (s16)Convert_Buff[3][1][1]) {
        bgm_level = Convert_Buff[3][1][1];
        save_w[Present_Mode].BGM_Level = Convert_Buff[3][1][1];
        SsBgmHalfVolume(0);
    }

    /* Live-apply SE level */
    if (se_level != (s16)Convert_Buff[3][1][2]) {
        se_level = Convert_Buff[3][1][2];
        setSeVolume(save_w[Present_Mode].SE_Level = Convert_Buff[3][1][2]);
    }

    /* Persist BGM type */
    save_w[Present_Mode].BgmType = Convert_Buff[3][1][3];

    /* Handle BGM type change — switch tracks */
    if (sys_w.bgm_type != Convert_Buff[3][1][3]) {
        sys_w.bgm_type = Convert_Buff[3][1][3];
        Convert_Buff[3][1][5] = 0;
        BGM_Request_Code_Check(0x41);
    }

    Order_Dir[0x7B] = Convert_Buff[3][1][5];
    Setup_Sound_Mode(last_mode);
    Save_Game_Data();

    /* Item 5: BGM Play/Stop */
    if (Menu_Cursor_Y[0] == 5) {
        if (IO_Result == 0x100) {
            SsRequest((u16)Order_Dir[0x7B] + 1);
            Convert_Buff[3][1][7] = 1;
            return;
        }

        if ((IO_Result == 0x200) && Convert_Buff[3][1][7]) {
            Convert_Buff[3][1][7] = 0;
            BGM_Stop();
            return;
        }
    }

    /* Item 6 (Exit) or Cancel — return to Option_Select */
    if (IO_Result == 0x200 || ((Menu_Cursor_Y[0] == 6) && (IO_Result == 0x100 || IO_Result == 0x4000))) {
        SE_selected();
        if (use_rmlui && rmlui_menu_sound)
            rmlui_sound_menu_hide();
        ControllerImageOverlay_Shutdown();
        Return_Option_Mode_Sub(task_ptr);
        setupAlwaysSeamlessFlag(0);
        Order[0x72] = 4;
        Order_Timer[0x72] = 4;
        BGM_Request_Code_Check(0x41);

        /* Return_Option_Mode_Sub sets r_no[1]=7 → hand off to legacy */
        MenuScreen_ExitToLegacy(task_ptr);
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sound_test_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void sound_test_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_sound)
        rmlui_sound_menu_show();
}

static void sound_test_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_sound)
        rmlui_sound_menu_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_SOUND_TEST]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_sound_test_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_sound_test_reg_ptr)(void) = ms_sound_test_register;
static void ms_sound_test_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_sound_test_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_sound_test_register(void) {
#endif
    g_screens[MENU_SCREEN_SOUND_TEST] = (MenuScreen) {
        .name = "sound_test",
        .id = MENU_SCREEN_SOUND_TEST,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = sound_test_enter,
        .on_tick = sound_test_tick,
        .on_exit = sound_test_exit,
        .cursor_max = 6,  /* 7 items (0–6) */
        .cancel_item = 6, /* last item = "Exit" */
        .rmlui_show = sound_test_rmlui_show,
        .rmlui_hide = sound_test_rmlui_hide,
        .header_type = MENU_HEADER_SOUND,
        .effect_slot = 0x72,
    };
}
