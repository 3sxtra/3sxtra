/**
 * @file menu_screen.h
 * @brief Data-driven MenuScreen registry — types and API declarations.
 *
 * Replaces the monolithic r_no[] + jump-table menu dispatch with named screens,
 * lifecycle callbacks (on_enter / on_tick / on_exit), and shared helpers for
 * fade, cursor, and navigation.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md).
 * Rendering is untouched — RmlUi Phase 3 handles that.
 * Netplay GameState and MenuBridge contracts remain identical.
 */

#ifndef MENU_SCREEN_H
#define MENU_SCREEN_H

#include "sf33rd/Source/Game/effect/eff57.h" /* MenuHeader enum */
#include "structs.h"                         /* struct _TASK   */
#include "types.h"                           /* u8, s16, etc.  */

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════
 *  Screen IDs
 *  Keep in sync with g_screens[] in menu_screen_registry.c and the
 *  g_legacy_to_screen[] lookup table.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum MenuScreenId {
    MENU_SCREEN_NONE = -1,

    /* --- After_Title screens (r_no[0]=0) --- */
    MENU_SCREEN_MODE_SELECT = 0,
    MENU_SCREEN_OPTION_SELECT,
    MENU_SCREEN_GAME_OPTION,
    MENU_SCREEN_BUTTON_CONFIG,
    MENU_SCREEN_SOUND_TEST,
    MENU_SCREEN_MEMORY_CARD,
    MENU_SCREEN_SYSTEM_DIRECTION,
    MENU_SCREEN_EXTRA_OPTION,
    MENU_SCREEN_DIRECTION_MENU,
    MENU_SCREEN_TRAINING_MODE,
    MENU_SCREEN_LOAD_REPLAY,
    MENU_SCREEN_EXIT_CONFIRM,
    MENU_SCREEN_VS_RESULT,
    MENU_SCREEN_SAVE_REPLAY,
    MENU_SCREEN_NETWORK_LOBBY,
    MENU_SCREEN_CASUAL_LOBBY,
    MENU_SCREEN_NETWORK_LOBBY_LAN,
    MENU_SCREEN_LEADERBOARD,
    MENU_SCREEN_SCREEN_ADJUST,

    /* --- Training sub-screens (r_no[0]=7) — Phase 5a (Task 19) --- */
    MENU_SCREEN_NORMAL_TRAINING,
    MENU_SCREEN_BLOCKING_TRAINING,
    MENU_SCREEN_DUMMY_SETTING,
    MENU_SCREEN_TRAINING_OPTION,
    MENU_SCREEN_BUTTON_CONFIG_TR,
    MENU_SCREEN_CHAR_CHANGE_TR,
    MENU_SCREEN_BLOCKING_TR_OPTION,

    /* --- In_Game screens (r_no[0]=1) — Phase 5b --- */
    MENU_SCREEN_PAUSE_MENU,
    MENU_SCREEN_BUTTON_CONFIG_IG,
    MENU_SCREEN_CHAR_CHANGE_IG,
    /* Pad_Come_Out is a no-op stub — not migrated */

    /* --- Transient Flow Screens --- */
    MENU_SCREEN_CONTINUE,
    MENU_SCREEN_WIN,
    MENU_SCREEN_LOSER,
    MENU_SCREEN_GAMEOVER,
    MENU_SCREEN_SAVER,

    /* --- Pre-Game Flow Screens --- */
    MENU_SCREEN_RANKING,
    MENU_SCREEN_DEMO,

    /* --- Character Select Screen --- */
    MENU_SCREEN_CHAR_SELECT,

    MENU_SCREEN_COUNT
} MenuScreenId;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Phase State Machine
 *  MenuScreen_Tick() advances through these phases automatically, calling
 *  lifecycle callbacks at the appropriate moments.
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef enum MenuScreenPhase {
    MENU_PHASE_ENTER,    /* one-shot: setup effects, show RmlUi doc     */
    MENU_PHASE_WAIT,     /* wait for timer / asset loads                */
    MENU_PHASE_FADE_IN,  /* FadeIn transition                          */
    MENU_PHASE_ACTIVE,   /* per-frame input handling                    */
    MENU_PHASE_FADE_OUT, /* FadeOut before transition                   */
    MENU_PHASE_EXIT,     /* one-shot: cleanup, hide RmlUi doc           */
} MenuScreenPhase;

/* ═══════════════════════════════════════════════════════════════════════════
 *  MenuScreen Struct
 *
 *  Each screen is a static entry in g_screens[MENU_SCREEN_COUNT].
 *  Fields are set at registration time (later tasks) and are immutable
 *  at runtime, except cursor_max which may be overridden in on_enter for
 *  screens whose item count is conditional (e.g. Option_Select: 6 or 7).
 * ═══════════════════════════════════════════════════════════════════════════ */

typedef struct MenuScreen {
    const char* name; /* "mode_select", etc.              */
    MenuScreenId id;
    MenuScreenId parent; /* screen to return to on cancel    */

    /* Lifecycle callbacks — all receive the TASK_MENU task pointer      */
    void (*on_enter)(struct _TASK*); /* called once on screen entry     */
    void (*on_tick)(struct _TASK*);  /* called every frame while active */
    void (*on_exit)(struct _TASK*);  /* called once on screen exit      */

    /* Cursor config (cursor_max may be overridden at runtime in on_enter
       for screens whose item count is conditional, e.g. Option_Select
       shows 6 or 7 items depending on Extra_Option unlock state)       */
    int cursor_max;  /* max menu items (for MC_Move_Sub) */
    int cancel_item; /* item index that means "exit"     */

    /* RmlUi integration — nullable function pointers                   */
    void (*rmlui_show)(void); /* show RmlUi document              */
    void (*rmlui_hide)(void); /* hide RmlUi document              */

    /* Effect config                                                    */
    MenuHeader header_type; /* MENU_HEADER_MODE_MENU, etc.     */
    u8 effect_slot;         /* Order[] slot for header bar     */
} MenuScreen;

/* ═══════════════════════════════════════════════════════════════════════════
 *  Core API — Registry & Dispatcher
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Request a transition to the specified screen.
 *
 * The actual transition happens at the top of the next MenuScreen_Tick()
 * call — never mid-frame. This prevents re-entrant lifecycle callbacks.
 */
void MenuScreen_Goto(MenuScreenId id);

/**
 * @brief Navigate to the current screen's parent (equivalent to cancel).
 *
 * Shorthand for MenuScreen_Goto(current_screen->parent).
 */
void MenuScreen_Back(void);

/**
 * @brief Per-frame dispatcher — drives the phase state machine.
 *
 * Called once per frame from the integration hook in After_Title() (and
 * later from Training_Menu() and In_Game()). Advances through phases
 * automatically, calling lifecycle callbacks at the appropriate moments.
 */
void MenuScreen_Tick(struct _TASK* task_ptr);

/**
 * @brief Returns true if the registry is currently driving a screen.
 *
 * When false, legacy dispatch handles the current r_no[1] value.
 */
bool MenuScreen_IsActive(void);

/**
 * @brief Map a legacy AT_Jmp_Tbl index to a MenuScreenId.
 *
 * Returns MENU_SCREEN_NONE for un-migrated or disabled screens.
 * Used by the integration hook in After_Title() to intercept r_no[1].
 *
 * @note Option_Select has FOUR alias indices (2, 3, 7, 15) — all must
 * map to MENU_SCREEN_OPTION_SELECT when migrated.
 */
MenuScreenId MenuScreen_FromLegacyIndex(int legacy_index);

/**
 * @brief Transition from ACTIVE to FADE_OUT phase.
 *
 * Used by screens that need an animated exit before the Goto/Back call.
 * Does NOT automatically navigate — the caller must check GetPhase()
 * for MENU_PHASE_EXIT and then call Goto/Back.
 */
void MenuScreen_RequestFadeOut(void);

/**
 * @brief Query the current phase (for fade-out completion checks in on_tick).
 */
MenuScreenPhase MenuScreen_GetPhase(void);

/**
 * @brief Query the currently active MenuScreen ID.
 *
 * Useful for nested legacy calls to determine if they should bypass their
 * own MenuScreen wrapper because another screen (like DEMO) is driving.
 */
MenuScreenId MenuScreen_GetCurrent(void);

/**
 * @brief Exit to legacy (non-registry) dispatch paths.
 *
 * Clears g_current_screen so MenuScreen_IsActive() returns false.
 * The caller sets r_no directly before calling this. Calls on_exit
 * and rmlui_hide for the current screen.
 */
void MenuScreen_ExitToLegacy(struct _TASK* task_ptr);

/**
 * @brief Hard reset to Mode Select — replaces Back_to_Mode_Select().
 *
 * Performs: full G_No/E_No reset, System_all_clear_Level_B(),
 * Menu_Init() bootstrap, BGM restart, then Goto(MODE_SELECT).
 */
void MenuScreen_HardReset(struct _TASK* task_ptr);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Shared Helpers — replacements for per-screen boilerplate
 *  (see menu_screen_helpers.c for implementations)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief FadeOut wrapper. Returns true when fade completes.
 */
bool MenuScreen_FadeOut(struct _TASK* task_ptr, int speed);

/**
 * @brief FadeIn wrapper. Returns true when fade completes.
 */
bool MenuScreen_FadeIn(struct _TASK* task_ptr, int speed);

/**
 * @brief Wait for the init timer to expire (wraps Menu_Sub_case1).
 *
 * For use in custom wait phases. The dispatcher calls Menu_Sub_case1
 * directly during MENU_PHASE_WAIT, so most screens don't need this.
 */
bool MenuScreen_WaitTimer(struct _TASK* task_ptr);

/**
 * @brief Unified vertical cursor movement — replaces MC_Move_Sub + Check_Menu_Lever.
 *
 * Reads both P1 and P2 input. Wraps to cursor boundaries and plays SE.
 *
 * @param cursor_max  Maximum cursor index (0-based: 6 for 7 items).
 * @param skip_item   Item to skip when Connect_Status==0 (0xFF to disable).
 * @return            IO_Result value.
 */
u16 MenuScreen_HandleCursor(int cursor_max, int skip_item);

/**
 * @brief Left/right value toggle input — returns debounced switch value.
 *
 * Reads both P1 and P2 input. The caller dispatches on the return value
 * to handle left/right value changes. Does NOT move the cursor.
 */
u16 MenuScreen_HandleCursorLR(void);

/**
 * @brief 2D grid cursor for Memory Card / Replay file browsers.
 *
 * Wraps Dir_Move_Sub2 + Check_Menu_Lever for screens with X/Y navigation.
 * The caller must set the global Menu_Max before calling.
 *
 * @param max_x  Maximum X position (columns - 1).
 * @param max_y  Maximum Y position (rows - 1).
 * @return       IO_Result value.
 */
u16 MenuScreen_HandleGridCursor(int max_x, int max_y);

/**
 * @brief Standard screen entry sequence — replaces Menu_in_Sub().
 *
 * Sets up fade, timer, common init, cursor restore, and effect slots.
 *
 * @param task_ptr    The menu task pointer.
 * @param hdr         MenuHeader enum value for the header bar effect.
 * @param slot        Order[] slot for the header bar.
 */
void MenuScreen_EnterSub(struct _TASK* task_ptr, MenuHeader hdr, u8 slot);

/**
 * @brief Multi-frame fade-out exit — replaces Exit_Sub's free[0] pattern.
 *
 * Call from on_tick every frame once exit is decided. Returns true when
 * the fade-out completes. Saves cursor to Cursor_Y_Pos[cursor_ix].
 * The caller should then call MenuScreen_Goto() or MenuScreen_Back().
 *
 * @param task_ptr    The menu task pointer.
 * @param cursor_ix   Index into Cursor_Y_Pos to save cursor.
 * @return            true when fade-out is complete.
 */
bool MenuScreen_ExitFade(struct _TASK* task_ptr, s16 cursor_ix);

/**
 * @brief Standard exit-to-parent using fade-out and cursor save.
 *
 * Convenience wrapper: calls ExitFade(tp, 0), then Back() when done.
 * Call from on_tick every frame once cancel is decided.
 */
void MenuScreen_ExitToParent(struct _TASK* task_ptr);

/* ═══════════════════════════════════════════════════════════════════════════
 *  Training / In-Game dispatch hooks (Phase 5)
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if a training sub-menu is being driven by the registry.
 *
 * Checked by the integration hook in Training_Menu() before attempting
 * to map r_no[1] to a migrated training sub-screen.
 */
bool MenuScreen_IsTrainingActive(void);

/**
 * @brief Per-frame dispatcher for training sub-menus.
 *
 * Wraps MenuScreen_Tick() and manages the g_training_active flag.
 * Called from Training_Menu() both when IsTrainingActive and on fresh
 * entry via FromTrainingIndex.
 */
void MenuScreen_TrainingTick(struct _TASK* task_ptr);

/**
 * @brief Map a Training_Jmp_Tbl index to a MenuScreenId.
 *
 * Returns MENU_SCREEN_NONE for un-migrated or disabled training screens.
 * Used by the integration hook in Training_Menu() to intercept r_no[1].
 *
 * @param training_index  Index into Training_Jmp_Tbl (0–7).
 */
MenuScreenId MenuScreen_FromTrainingIndex(int training_index);

/* ═══════════════════════════════════════════════════════════════════════════
 *  In-Game dispatch hooks (Phase 5b — Task 20)
 *
 *  In_Game() dispatches via In_Game_Jmp_Tbl[r_no[1]] with 5 entries.
 *  These functions let the registry intercept that dispatch for migrated
 *  In-Game screens (Menu_Select, Button_Config_in_Game, Character_Change).
 *  Pad_Come_Out is a no-op stub and is NOT migrated.
 * ═══════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Returns true if an In-Game sub-menu is being driven by the registry.
 *
 * Checked by the integration hook in In_Game() before attempting
 * to map r_no[1] to a migrated In-Game sub-screen.
 */
bool MenuScreen_IsInGameActive(void);

/**
 * @brief Per-frame dispatcher for In-Game sub-menus.
 *
 * Wraps MenuScreen_Tick() and manages the g_ingame_active flag.
 * Called from In_Game() both when IsInGameActive and on fresh
 * entry via FromInGameIndex.
 */
void MenuScreen_InGameTick(struct _TASK* task_ptr);

/**
 * @brief Map an In_Game_Jmp_Tbl index to a MenuScreenId.
 *
 * Returns MENU_SCREEN_NONE for un-migrated or disabled In-Game screens.
 * Used by the integration hook in In_Game() to intercept r_no[1].
 *
 * @param ingame_index  Index into In_Game_Jmp_Tbl (0–4).
 */
MenuScreenId MenuScreen_FromInGameIndex(int ingame_index);

#ifdef __cplusplus
}
#endif

#endif /* MENU_SCREEN_H */
