/**
 * @file replay_game.c
 * @brief Multi-round replay parser implementation.
 *
 * Parses CPS3 RAM dumps to detect character selection, round boundaries,
 * and reads per-round inputs from a binary inputs file.
 */

#include "stb/stb_ds.h"

#include "test/replay_game.h"
#include "arcade/arcade_constants.h"
#include "main.h"
#include "test/test_runner_utils.h"

#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_stdinc.h>

/**
 * Read a single frame of input from the binary inputs file.
 *
 * The inputs file stores 5-byte records per player per frame:
 *   frame N, P1: offset = N * 10
 *   frame N, P2: offset = N * 10 + 5
 *
 * Each record is a raw 16-bit button bitfield that needs to be remapped
 * from CPS3 button layout to the game engine's SWKey layout.
 */
static Uint16 read_input_buff(SDL_IOStream* io, Sint64 offset) {
    Uint16 raw_buff = 0;
    Uint16 buff = 0;

    SDL_SeekIO(io, offset, SDL_IO_SEEK_SET);
    SDL_ReadIO(io, &raw_buff, 2);

    buff |= (raw_buff & (1 << 1)) << 13; /* start */
    buff |= (raw_buff & (1 << 2)) >> 2;  /* up    */
    buff |= (raw_buff & (1 << 3)) >> 2;  /* down  */
    buff |= (raw_buff & (1 << 4)) >> 2;  /* left  */
    buff |= (raw_buff & (1 << 5)) >> 2;  /* right */
    buff |= (raw_buff & (1 << 6)) >> 2;  /* LP    */
    buff |= (raw_buff & (1 << 7)) >> 2;  /* MP    */
    buff |= (raw_buff & (1 << 8)) >> 2;  /* HP    */
    buff |= (raw_buff & (1 << 9)) >> 1;  /* LK    */
    buff |= (raw_buff & (1 << 10)) >> 1; /* MK    */
    buff |= (raw_buff & (1 << 11)) >> 1; /* HK    */

    return buff;
}

/**
 * Adjust character numbers for the PS2 version.
 *
 * There's no Shin Akuma in the PS2 version, so character IDs above
 * CHAR_AKUMA must be decremented by one to match the expected enum.
 */
static void adjust_character_numbers(ReplayGame* game) {
    for (int i = 0; i < 2; i++) {
        if (game->characters[i] > CHAR_AKUMA) {
            game->characters[i] -= 1;
        }
    }
}

/**
 * Parse inputs for a single round from the binary inputs file.
 *
 * @param round      Round to populate with inputs
 * @param start_frame  Frame number where this round begins (used as file offset base)
 * @param frame_count  Number of frames to read
 */
static void parse_round_inputs(ReplayRound* round, int start_frame, int frame_count) {
    SDL_IOStream* io = SDL_IOFromFile(configuration.test.inputs_path, "rb");
    if (io == NULL) {
        return;
    }

    for (int i = 0; i < frame_count; i++) {
        const int frame = start_frame + i;
        const Sint64 p1_offset = frame * 10;
        const Sint64 p2_offset = p1_offset + 5;
        const ReplayInput input = {
            .p1 = read_input_buff(io, p1_offset),
            .p2 = read_input_buff(io, p2_offset),
        };
        arrput(round->inputs, input);
    }

    SDL_CloseIO(io);
}

void ReplayGame_Parse(ReplayGame* game) {
    SDL_zerop(game);

    bool in_game_prev = false;
    bool did_set_char_data = false;
    int round_start_frame = 0;
    int round_frame_count = 0;

    for (int frame_num = 0;; frame_num++) {
        char* path = ram_path(frame_num);
        SDL_IOStream* io = SDL_IOFromFile(path, "rb");
        SDL_free(path);

        if (io == NULL) {
            break;
        }

        const Uint16 routine = read_u16(io, GAME_ROUTINE_OFFSET);
        const bool in_game = (routine == 2);

        /* Detect round start */
        if (in_game && !in_game_prev) {
            round_start_frame = frame_num;
            round_frame_count = 0;
        }

        /* Read character and SA indices until we get into the game.
         * This ensures we read the latest data. */
        if (in_game && !did_set_char_data) {
            SDL_SeekIO(io, MY_CHAR_OFFSET, SDL_IO_SEEK_SET);
            SDL_ReadIO(io, game->characters, 2);

            SDL_SeekIO(io, SUPER_ARTS_OFFSET, SDL_IO_SEEK_SET);
            SDL_ReadIO(io, game->supers, 2);

            SDL_SeekIO(io, NEW_CHALLENGER_OFFSET, SDL_IO_SEEK_SET);
            SDL_ReadU8(io, &game->new_challenger);

            SDL_SeekIO(io, PLAYER_COLOR_OFFSET, SDL_IO_SEEK_SET);
            SDL_ReadIO(io, game->colors, 2);

            did_set_char_data = true;
        }

        /* Count frames in this round */
        if (in_game) {
            round_frame_count++;
        }

        /* Detect round end: was in-game, now leaving */
        if (in_game_prev && !in_game) {
            ReplayRound round;
            SDL_zero(round);
            round.start_frame = round_start_frame;

            parse_round_inputs(&round, round_start_frame, round_frame_count);
            arrput(game->rounds, round);

            /* Reset for potential next round */
            round_frame_count = 0;
        }

        in_game_prev = in_game;
        SDL_CloseIO(io);
    }

    /* If we ended while still in-game (file sequence ended mid-round),
     * flush the partial round */
    if (in_game_prev && round_frame_count > 0) {
        ReplayRound round;
        SDL_zero(round);
        round.start_frame = round_start_frame;

        parse_round_inputs(&round, round_start_frame, round_frame_count);
        arrput(game->rounds, round);
    }

    adjust_character_numbers(game);
}

void ReplayGame_Destroy(ReplayGame* game) {
    for (int i = 0; i < arrlen(game->rounds); i++) {
        arrfree(game->rounds[i].inputs);
    }
    arrfree(game->rounds);
}
