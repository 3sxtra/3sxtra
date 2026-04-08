/**
 * @file test_runner.c
 * @brief Headless replay regression test runner.
 *
 * Drives the game engine through character select and into gameplay,
 * feeding pre-recorded inputs from a binary file. Supports multi-round
 * replays via the ReplayGame parser.
 *
 * Invoked via CLI: --test --states-path <dir> --inputs-path <file>
 */

#include "test/test_runner.h"
#include "arcade/arcade_constants.h"
#include "main.h"
#include "port/menu_task.h"
#include "sf33rd/AcrSDK/common/pad.h"
#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/system/work_sys.h"
#include "test/replay_game.h"
#include "test/test_runner_utils.h"
#include "test/test_runner_compare.h"

#include "stb/stb_ds.h"
#include <SDL3/SDL.h>

typedef enum Phase {
    PHASE_TITLE,
    PHASE_MENU,
    PHASE_CHARACTER_SELECT_TRANSITION,
    PHASE_CHARACTER_SELECT,
    PHASE_GAME_TRANSITION,
    PHASE_ROUND_TRANSITION,
    PHASE_GAME,
} Phase;

static const Uint8 character_to_cursor[20][2] = { { 7, 1 }, { 1, 0 }, { 5, 2 }, { 6, 1 }, { 3, 2 }, { 4, 0 }, { 1, 2 },
                                                  { 3, 0 }, { 2, 2 }, { 4, 2 }, { 0, 1 }, { 0, 2 }, { 2, 0 }, { 5, 0 },
                                                  { 6, 0 }, { 3, 1 }, { 2, 1 }, { 4, 1 }, { 1, 1 }, { 5, 1 } };

static const SWKey color_to_keys[13] = {
    SWK_WEST,
    SWK_NORTH,
    SWK_RIGHT_SHOULDER,
    SWK_SOUTH,
    SWK_EAST,
    SWK_RIGHT_TRIGGER,
    SWK_WEST | SWK_RIGHT_SHOULDER | SWK_EAST,
    SWK_START | SWK_WEST,
    SWK_START | SWK_NORTH,
    SWK_START | SWK_RIGHT_SHOULDER,
    SWK_START | SWK_SOUTH,
    SWK_START | SWK_EAST,
    SWK_START | SWK_RIGHT_TRIGGER,
};

static Uint64 frame = 0;
static Phase phase = PHASE_TITLE;
static int char_select_phase = 0;
static int wait_timer = 0;
static int inputs_index = 0;
static int comparison_index = 0;
static bool in_battle = false;
static bool in_battle_prev = false;

static bool initialized = false;
static ReplayGame game;
static int round_index = 0;

static SDL_IOStream* io_at_index(int index) {
    const char* path = ram_path(index);
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    SDL_free((void*)path);
    return io;
}

static ReplayRound* current_round(void) {
    return &game.rounds[round_index];
}

static void set_cursor(ReplayCharacter character, int player) {
    Cursor_X[player] = character_to_cursor[character][0];
    Cursor_Y[player] = character_to_cursor[character][1];
}

/// Repeatedly press and release a button
static void mash_button(SWKey button, int player) {
    u16* dst = player ? &p2sw_buff : &p1sw_buff;
    *dst |= (frame & 1) ? button : 0;
}

static void tap_button(SWKey button, int player) {
    u16* dst = player ? &p2sw_buff : &p1sw_buff;
    *dst |= button;
}

static void reset_comparison_index(void) {
    comparison_index = current_round()->start_frame;
}

static void finish_round(void) {
    inputs_index = 0;

    if (round_index < arrlen(game.rounds) - 1) {
        round_index += 1;
        phase = PHASE_ROUND_TRANSITION;
    } else {
        ReplayGame_Destroy(&game);
        exit(0);
    }

    reset_comparison_index();
}

void TestRunner_Prologue() {
    p1sw_buff = 0;
    p2sw_buff = 0;
    in_battle = (C_No[0] == 2);

    if (!initialized) {
        ReplayGame_Parse(&game);
        reset_comparison_index();
        initialized = true;
    }

    switch (phase) {
    case PHASE_TITLE: {
        if (MenuTask_GetPhase() == MTP_AFTER_TITLE && MenuTask_GetSubPhase() == MTSP_MODE_SELECT &&
            MenuTask_GetRNo(2) == 3) {
            phase = PHASE_MENU;
            break;
        }

        mash_button(SWK_START, 0);
        break;
    }

    case PHASE_MENU:
        if (G_No[1] == 1 && G_No[2] == 2) {
            /* Even though we move cursor manually later, setting Last_My_char2
             * is required for Last_Super_Arts to take effect. */
            Last_My_char2[0] = game.characters[0];
            Last_My_char2[1] = game.characters[1];
            Last_Super_Arts[0] = game.supers[0];
            Last_Super_Arts[1] = game.supers[1];
            phase = PHASE_CHARACTER_SELECT_TRANSITION;
            wait_timer = 60;
            break;
        }

        mash_button(SWK_SOUTH, 0);
        break;

    case PHASE_CHARACTER_SELECT_TRANSITION:
        wait_timer -= 1;

        if (wait_timer <= 0) {
            phase = PHASE_CHARACTER_SELECT;
        }

        break;

    case PHASE_CHARACTER_SELECT:
        switch (char_select_phase) {
        case 0:
            set_cursor(game.characters[0], 0);
            set_cursor(game.characters[1], 1);
            tap_button(SWK_START, 1);
            wait_timer = 20;
            char_select_phase = 1;
            break;

        case 1:
            wait_timer -= 1;

            if (wait_timer <= 0) {
                // We must set New_Challenger manually so that the game selects the correct stage.
                // If we set this var earlier it would be overwritten
                New_Challenger = game.new_challenger;
                char_select_phase = 2;
            }

            break;

        case 2:
            tap_button(color_to_keys[game.colors[0]], 0);
            tap_button(color_to_keys[game.colors[1]], 1);
            wait_timer = 45;
            char_select_phase = 3;
            break;

        case 3:
            wait_timer -= 1;

            if (wait_timer <= 0) {
                tap_button(SWK_SOUTH, 0);
                tap_button(SWK_SOUTH, 1);
                phase = PHASE_GAME_TRANSITION;
            }

            break;
        }

        break;

    case PHASE_GAME_TRANSITION:
        if (G_No[1] == 2) {
            phase = PHASE_GAME;
        } else {
            // Mash buttons to skip the VS animation
            mash_button(SWK_ATTACKS, 0);
            break;
        }
        // fallthrough into PHASE_GAME once G_No transitions
        goto play_frame;

    case PHASE_ROUND_TRANSITION:
        if (G_No[1] != 2) {
            // Wait for the next round to start
            break;
        }

        SDL_IOStream* io = io_at_index(comparison_index - 1);
        if (io) {
            Game_timer = read_s16(io, GAME_TIMER_OFFSET);
            sync_values(io);
            SDL_CloseIO(io);
        }

        phase = PHASE_GAME;
        // fallthrough

    play_frame:
    case PHASE_GAME: {
        ReplayInput* inputs = current_round()->inputs;
        const ReplayInput input = inputs[inputs_index];
        p1sw_buff = input.p1;
        p2sw_buff = input.p2;
        inputs_index += 1;

        if (inputs_index >= arrlen(inputs)) {
            finish_round();
        }

        break;
    }
    }
}

void TestRunner_Epilogue() {
    if (phase == PHASE_GAME && in_battle) {
        SDL_IOStream* io = io_at_index(comparison_index);
        if (io) {
            compare_values(io, frame);
            SDL_CloseIO(io);
        }
        comparison_index += 1;
    }

    in_battle_prev = in_battle;
    frame += 1;
    p1sw_buff = 0;
    p2sw_buff = 0;
}
