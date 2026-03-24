/**
 * @file bracket.h
 * @brief Client-side bracket generation and advancement for tournaments.
 *
 * Used to generate bracket trees locally for display purposes when
 * the server sends seeded player data. Also provides advancement logic
 * for computing next-round matchups after results come in.
 */
#ifndef BRACKET_H
#define BRACKET_H

#include <stdbool.h>
#include "netplay/lobby_server.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─── Seeding ─────────────────────────────────────────────────────

/// Seed entry for bracket generation.
typedef struct {
    char player_id[64];
    char display_name[32];
    int seed;       // 0 = top seed, ascending
    float rating;   // optional: used for rating-based sort
} BracketSeed;

/// Sort BracketSeed array by rating descending (highest rating = seed 0).
void Bracket_SortByRating(BracketSeed* seeds, int count);

// ─── Single Elimination ──────────────────────────────────────────

/// Number of rounds needed for a single-elimination bracket of N players.
int Bracket_SingleElimRounds(int num_players);

/// Number of matches in a given round (0-indexed) for single elim.
int Bracket_GetMatchesInRound(int num_players, int round);

/// Generate a single-elimination bracket from seeded players.
/// Players should be pre-ordered by seed (index 0 = top seed).
/// Writes bracket entries to out_bracket (max MAX_BRACKET_SIZE).
/// Returns number of entries written, or -1 on error.
int Bracket_GenerateSingleElim(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries);

/// Generate single-elimination bracket from BracketSeed array.
/// Seeds should be pre-sorted (call Bracket_SortByRating first if needed).
int Bracket_GenerateSingleElimSeeded(const BracketSeed* seeds,
                                      int num_players,
                                      BracketEntry* out_bracket,
                                      int max_entries);

/// Advance bracket: given a completed match (round, position, winner_id),
/// place the winner into the correct next-round slot.
bool Bracket_AdvanceSingleElim(BracketEntry* bracket, int bracket_size,
                                int round, int position,
                                const char* winner_id, const char* winner_name);

// ─── Double Elimination ──────────────────────────────────────────

/// Total entries needed for a double-elimination bracket of N players.
/// Winners bracket + losers bracket + grand finals.
int Bracket_DoubleElimTotalEntries(int num_players);

/// Generate double-elimination bracket from seeded players.
/// Uses round encoding: rounds 0..R-1 = winners bracket,
///                       rounds R..2R-1 = losers bracket,
///                       round 2R = grand finals.
/// Returns number of entries written, or -1 on error.
int Bracket_GenerateDoubleElim(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries);

/// Advance double-elimination bracket: routes winner to next winners slot,
/// and loser to the appropriate losers bracket slot.
/// winners_rounds = number of winners bracket rounds (from log2(next_pow2(N))).
bool Bracket_AdvanceDoubleElim(BracketEntry* bracket, int bracket_size,
                                int winners_rounds,
                                int round, int position,
                                const char* winner_id, const char* winner_name,
                                const char* loser_id, const char* loser_name);

// ─── Round Robin ─────────────────────────────────────────────────

/// Total entries needed for round robin of N players.
/// Every player plays every other: N*(N-1)/2 matches.
int Bracket_RoundRobinTotalEntries(int num_players);

/// Generate a round-robin bracket (every player vs every other).
/// Rounds are generated using the circle method for balanced scheduling.
/// Returns number of entries written, or -1 on error.
int Bracket_GenerateRoundRobin(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries);

// ─── Swiss ───────────────────────────────────────────────────────

/// Generate pairings for one Swiss round.
/// Sorts players by record (wins descending) and pairs adjacent players.
/// wins[] is an array of win counts parallel to player_ids[].
/// swiss_round is the 0-indexed round number (used for BracketEntry.round).
/// Returns number of entries written (num_players/2), or -1 on error.
int Bracket_GenerateSwissRound(const char player_ids[][64],
                                const char player_names[][32],
                                const int* wins,
                                int num_players,
                                int swiss_round,
                                BracketEntry* out_bracket,
                                int max_entries);

/// Recommended number of Swiss rounds for N players.
/// Uses ceil(log2(N)) — same as single elim depth.
int Bracket_SwissRounds(int num_players);

// ─── Common Utilities ────────────────────────────────────────────

/// Find the bracket entry for a given round and position.
BracketEntry* Bracket_FindEntry(BracketEntry* bracket, int bracket_size,
                                 int round, int position);

/// Check if the bracket is complete (a grand final winner exists).
/// Returns the winner's player_id, or empty string if not yet decided.
const char* Bracket_GetWinner(const BracketEntry* bracket, int bracket_size,
                               int total_rounds);

#ifdef __cplusplus
}
#endif

#endif // BRACKET_H
