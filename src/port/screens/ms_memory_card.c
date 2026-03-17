/**
 * @file ms_memory_card.c
 * @brief Migrated Memory Card screen — Task 13.
 *
 * Replaces the Memory_Card() function in menu.c with MenuScreen registry
 * callbacks.  This is the save/load management screen with 4 items:
 *   Item 0: Save
 *   Item 1: Load
 *   Item 2: Auto Save (L/R toggle)
 *   Item 3: Exit
 *
 * The screen has a main phase (cases 0–3) with cursor navigation and
 * sub-phases (cases 4–6) for the actual Save/Load/Delete file browser
 * driven by Save_Load_Menu().
 *
 * Legacy location: menu.c lines 3045–3125 (AT_Jmp_Tbl index 13).
 * Exit handling: menu_input.c Button_Exit_Check() case 13 (lines 860–899).
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md §8 Phase 3).
 */

#include "port/menu_screen.h"

#include "sf33rd/Source/Game/effect/eff04.h"       /* effect_04_init */
#include "sf33rd/Source/Game/effect/eff57.h"       /* effect_57_init, MenuHeader */
#include "sf33rd/Source/Game/effect/eff61.h"       /* effect_61_init */
#include "sf33rd/Source/Game/effect/eff64.h"       /* effect_64_init */
#include "sf33rd/Source/Game/effect/eff66.h"       /* effect_66_init */
#include "sf33rd/Source/Game/engine/workuser.h"    /* Menu_Cursor_Y, save_w, etc. */
#include "sf33rd/Source/Game/io/vm_sub.h"          /* Setup_File_Property */
#include "sf33rd/Source/Game/menu/menu.h"          /* Menu_Common_Init */
#include "sf33rd/Source/Game/menu/menu_internal.h" /* Memory_Card_Sub, Button_Exit_Check, etc. */
#include "sf33rd/Source/Game/sound/sound3rd.h"     /* SE_selected */
#include "sf33rd/Source/Game/system/reset.h"       /* Suicide */
#include "sf33rd/Source/Game/system/sys_sub.h"     /* Save_Game_Data */
#include "sf33rd/Source/Game/system/work_sys.h"    /* save_w */
#include "sf33rd/Source/Game/ui/sc_sub.h"          /* FadeOut, FadeIn, FadeInit */
#include "structs.h"                               /* struct _TASK */

/* RmlUi Phase 3 */
#include "port/sdl/rmlui/rmlui_memory_card.h"    /* rmlui_memory_card_show/hide */
#include "port/sdl/rmlui/rmlui_phase3_toggles.h" /* use_rmlui, rmlui_menu_memory_card */

/* ═══════════════════════════════════════════════════════════════════════════
 *  Internal state
 * ═══════════════════════════════════════════════════════════════════════════ */

static bool s_wait_done = false;

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_enter — extracted from Memory_Card case 0
 *
 *  Sets up fade, timer, common init, cursor, header bar (0x69,
 *  MENU_HEADER_SAVE_LOAD), CPS3 effect items, file property setup,
 *  and RmlUi memory card menu.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void memory_card_enter(struct _TASK* task_ptr) {
    s16 ix;
    s16 char_index;
    s16 unused_s3;
    s16 unused_s2;

    s_wait_done = false;

    /* ── Replicate Memory_Card case 0 init pattern ── */
    FadeOut(1, 0xFF, 8);
    task_ptr->r_no[2] = 1; /* advance so Menu_Sub_case1 works in WAIT phase */
    task_ptr->timer = 5;
    Menu_Common_Init();
    Menu_Cursor_Y[0] = 0;
    Menu_Suicide[1] = 1;
    Menu_Suicide[2] = 0;

    /* Kill/setup parent effect slots */
    Order[0x4F] = 4;
    Order_Timer[0x4F] = 1;
    Order[0x4E] = 2;
    Order_Dir[0x4E] = 4;
    Order_Timer[0x4E] = 1;

    /* Header bar + item labels — CPS3 only */
    if (use_rmlui && rmlui_menu_memory_card) {
        rmlui_memory_card_show();
    } else {
        effect_57_init(0x69, MENU_HEADER_SAVE_LOAD, 0, 0x3F, 2);
        Order[0x69] = 1;
        Order_Dir[0x69] = 8;
        Order_Timer[0x69] = 1;

        for (ix = 0, unused_s3 = char_index = 0x15; ix < 4; ix++, unused_s2 = char_index++) {
            effect_61_init(0, ix + 0x50, 1, 2, char_index, ix, 0x7047);
            Order[ix + 0x50] = 1;
            Order_Dir[ix + 0x50] = 4;
            Order_Timer[ix + 0x50] = ix + 0x14;
        }

        Menu_Cursor_Move = 4;
        effect_64_init(0x61, 1, 2, 0, 2, 0x7047, 0, 3, 0);
        Order[0x61] = 1;
        Order_Dir[0x61] = 4;
        Order_Timer[0x61] = 0x18;
        effect_66_init(0x8A, 8, 2, 1, -1, -1, -0x7FF5);
        Order[0x8A] = 3;
        Order_Timer[0x8A] = 1;
        effect_04_init(2, 2, 2, 0x48);
    }

    Setup_File_Property(0, 0xFF);

    /* ── Set r_no[1] to 13 for Button_Exit_Check compatibility ── */
    task_ptr->r_no[1] = 13;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_tick — extracted from Memory_Card cases 3–6
 *
 *  Case 3 (main): Memory_Card_Sub(0/1) + Button_Exit_Check(0) for cursor
 *  movement and exit/sub-menu dispatching.
 *
 *  Cases 4–6 (sub-menus): Save_Load_Menu(task_ptr) handles the actual
 *  save/load/delete file browser flow.
 *
 *  Button_Exit_Check case 13:
 *  - Cancel or Item 3 → Return_Option_Mode_Sub → exit to Option_Select
 *  - Item 0 → r_no[2]=4 (Save sub-menu)
 *  - Item 1 → r_no[2]=5 (Load sub-menu)
 *  - Item 2 → r_no[2]=6 (Delete sub-menu — though currently "Auto Save" toggle)
 *
 *  We detect the exit by checking if r_no[1] changed from 13.
 *  We also check r_no[2] to see if Button_Exit_Check sent us to a sub-menu.
 * ═══════════════════════════════════════════════════════════════════════════ */

static void memory_card_tick(struct _TASK* task_ptr) {
    /* ── One-time post-wait-phase setup ── */
    if (!s_wait_done) {
        s_wait_done = true;
        Menu_Suicide[3] = 0;
    }

    /* ── Determine which phase we're in ── */
    switch (task_ptr->r_no[2]) {
    case 2:
        task_ptr->r_no[2] = 3;
        /* fallthrough */

    case 3:
        /* Main cursor input */
        task_ptr->r_no[1] = 13; /* Preserve for Button_Exit_Check routing */

        Memory_Card_Sub(0);
        Button_Exit_Check(task_ptr, 0);

        if (IO_Result == 0) {
            Memory_Card_Sub(1);
            Button_Exit_Check(task_ptr, 0);
        }

        /* Check if Button_Exit_Check triggered an exit to Option_Select */
        if (task_ptr->r_no[1] != 13) {
            /* Return_Option_Mode_Sub was called (r_no[1]=7) */
            MenuScreen_ExitToLegacy(task_ptr);
            return;
        }

        /* Check if Button_Exit_Check entered a sub-menu (r_no[2] changed) */
        /* Cases 4, 5, 6 are handled in the default branch below */
        break;

    case 4:
    case 5:
    case 6:
        /* Sub-menu: Save/Load/Delete file browser */
        Save_Load_Menu(task_ptr);

        /* Save_Load_Menu may set r_no[2] back to 3 when done,
         * or it may use Exit_Sub to transition elsewhere.
         * If r_no[1] changed, we exit to legacy. */
        if (task_ptr->r_no[1] != 13) {
            MenuScreen_ExitToLegacy(task_ptr);
        }
        break;

    default:
        /* Shouldn't get here — but handle gracefully */
        break;
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  on_exit — cleanup
 * ═══════════════════════════════════════════════════════════════════════════ */

static void memory_card_exit(struct _TASK* task_ptr) {
    (void)task_ptr;
    s_wait_done = false;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  RmlUi callbacks (nullable in MenuScreen struct)
 * ═══════════════════════════════════════════════════════════════════════════ */

static void memory_card_rmlui_show(void) {
    if (use_rmlui && rmlui_menu_memory_card)
        rmlui_memory_card_show();
}

static void memory_card_rmlui_hide(void) {
    if (use_rmlui && rmlui_menu_memory_card)
        rmlui_memory_card_hide();
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Registration — populate g_screens[MENU_SCREEN_MEMORY_CARD]
 *
 *  Uses GCC/MSVC constructor attribute to register at startup.
 * ═══════════════════════════════════════════════════════════════════════════ */

extern MenuScreen g_screens[MENU_SCREEN_COUNT];

#if defined(_MSC_VER)
/* MSVC: use CRT initializer section */
#pragma section(".CRT$XCU", read)
static void ms_memory_card_register(void);
__declspec(allocate(".CRT$XCU")) static void (*ms_memory_card_reg_ptr)(void) = ms_memory_card_register;
static void ms_memory_card_register(void) {
#elif defined(__GNUC__) || defined(__clang__)
__attribute__((constructor)) static void ms_memory_card_register(void) {
#else
/* Fallback: must be called manually from init code */
void ms_memory_card_register(void) {
#endif
    g_screens[MENU_SCREEN_MEMORY_CARD] = (MenuScreen) {
        .name = "memory_card",
        .id = MENU_SCREEN_MEMORY_CARD,
        .parent = MENU_SCREEN_OPTION_SELECT,
        .on_enter = memory_card_enter,
        .on_tick = memory_card_tick,
        .on_exit = memory_card_exit,
        .cursor_max = 3,  /* 4 items (0–3) */
        .cancel_item = 3, /* last item = "Exit" */
        .rmlui_show = memory_card_rmlui_show,
        .rmlui_hide = memory_card_rmlui_hide,
        .header_type = MENU_HEADER_SAVE_LOAD,
        .effect_slot = 0x69,
    };
}
