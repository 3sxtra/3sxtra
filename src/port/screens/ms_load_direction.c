/**
 * @file ms_load_direction.c
 * @brief Extracted Load Direction screen — Task 9.
 *
 * This file documents the Load_Direction screen (AT_Jmp_Tbl index 20).
 * Save_Direction and Load_Direction don't have dedicated MenuScreenId
 * values in the enum. They remain as legacy-dispatched functions in
 * menu_input.c because they are reached via Exit_Sub from
 * System_Direction and use complex I/O paths (Load_Replay_MC_Sub)
 * that manipulate r_no[] directly.
 *
 * Legacy location: menu_input.c lines 427–469.
 * Parent: MENU_SCREEN_SYSTEM_DIRECTION (conceptual).
 *
 * The screen loads System Direction settings via NativeSave_LoadDirection(),
 * then exits back to SysDir via Load_Replay_MC_Sub cancel path.
 *
 * This file is created as part of Task 9 per the PRD for code organization,
 * but the actual function body remains in menu_input.c until Phase 6 cleanup.
 *
 * Part of the Menu Backend Migration (see MENU_BACKEND_MIGRATION.md).
 */

/*
 * Load_Direction flow (AT index 20, r_no[2] cases 0-3):
 *
 * case 0: FadeOut, setup BG, replay header, file property, flash init.
 *         g_state.Menu_Suicide[1]=1, g_state.Menu_Suicide[2]=0, g_state.Menu_Cursor_X[0]=0.
 *
 * case 1: Menu_Sub_case1 wait. On completion, NativeSave_LoadDirection().
 *
 * case 2: FadeIn(1, 0x19, 8). On completion: r_no[2]++, free[3]=0,
 *         g_state.Menu_Cursor_X[0] = Setup_Final_Cursor_Pos(0, 8).
 *
 * case 3: g_state.IO_Result = 0x200 (force cancel), Load_Replay_MC_Sub routes
 *         back to SysDir (r_no[1]=5) via the cancel branch.
 */
