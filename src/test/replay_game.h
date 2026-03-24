/**
 * @file replay_game.h
 * @brief Multi-round replay parser for the test runner.
 *
 * Parses CPS3 RAM dumps to extract character/SA selection and per-round
 * input sequences. Uses stb_ds dynamic arrays for variable-length storage.
 */

#ifndef REPLAY_GAME_H
#define REPLAY_GAME_H

#include <SDL3/SDL_stdinc.h>

/** Character IDs matching CPS3 internal numbering. */
typedef enum ReplayCharacter {
    CHAR_GILL     = 0,
    CHAR_ALEX     = 1,
    CHAR_RYU      = 2,
    CHAR_YUN      = 3,
    CHAR_DUDLEY   = 4,
    CHAR_NECRO    = 5,
    CHAR_HUGO     = 6,
    CHAR_IBUKI    = 7,
    CHAR_ELENA    = 8,
    CHAR_ORO      = 9,
    CHAR_YANG     = 10,
    CHAR_KEN      = 11,
    CHAR_SEAN     = 12,
    CHAR_URIEN    = 13,
    CHAR_AKUMA    = 14,
    CHAR_CHUNLI   = 15,
    CHAR_MAKOTO   = 16,
    CHAR_Q        = 17,
    CHAR_TWELVE   = 18,
    CHAR_REMY     = 19,
} ReplayCharacter;

/** A single frame of player inputs. */
typedef struct ReplayInput {
    Uint16 p1;
    Uint16 p2;
} ReplayInput;

/** One round's worth of inputs and metadata. */
typedef struct ReplayRound {
    ReplayInput* inputs;    /**< stb_ds dynamic array — call arrfree() to release */
    int          start_frame; /**< Frame index where this round's inputs begin */
} ReplayRound;

/** A complete multi-round replay game. */
typedef struct ReplayGame {
    ReplayRound* rounds;        /**< stb_ds dynamic array of rounds */
    Uint8        characters[2]; /**< Character IDs (adjusted for PS2 — no Shin Akuma) */
    Uint8        supers[2];     /**< Super Art indices */
} ReplayGame;

/**
 * Parse a replay from RAM dump frames and an inputs binary file.
 *
 * Reads per-frame RAM dumps from configuration.test.states_path to determine
 * character/SA selection and round boundaries. Reads the actual inputs from
 * configuration.test.inputs_path.
 *
 * @param[out] game  Zeroed and populated on return. Call ReplayGame_Destroy() when done.
 */
void ReplayGame_Parse(ReplayGame* game);

/** Free all dynamic arrays owned by a ReplayGame. */
void ReplayGame_Destroy(ReplayGame* game);

#endif /* REPLAY_GAME_H */
