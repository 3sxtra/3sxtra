/**
 * @file bracket.c
 * @brief Client-side bracket generation and advancement.
 *
 * Generates single-elimination, double-elimination, and round-robin bracket
 * trees for local display and handles advancement when match results arrive.
 */

#include "bracket.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

// ─── Utilities ───────────────────────────────────────────────────

/// Round up to next power of 2
static int next_pow2(int n) {
    int p = 1;
    while (p < n) p <<= 1;
    return p;
}

/// log2 for powers of 2
static int log2_int(int n) {
    int r = 0;
    while (n > 1) { n >>= 1; r++; }
    return r;
}

// ─── Seeding ─────────────────────────────────────────────────────

static int cmp_rating_desc(const void* a, const void* b) {
    const BracketSeed* sa = (const BracketSeed*)a;
    const BracketSeed* sb = (const BracketSeed*)b;
    if (sb->rating > sa->rating) return 1;
    if (sb->rating < sa->rating) return -1;
    return 0;
}

void Bracket_SortByRating(BracketSeed* seeds, int count) {
    if (!seeds || count < 2) return;
    qsort(seeds, (size_t)count, sizeof(BracketSeed), cmp_rating_desc);
    for (int i = 0; i < count; i++)
        seeds[i].seed = i;
}

// ─── Single Elimination ──────────────────────────────────────────

int Bracket_SingleElimRounds(int num_players) {
    if (num_players < 2) return 0;
    return log2_int(next_pow2(num_players));
}

int Bracket_GetMatchesInRound(int num_players, int round) {
    if (num_players < 2 || round < 0) return 0;
    int bracket_size = next_pow2(num_players);
    int total_rounds = log2_int(bracket_size);
    if (round >= total_rounds) return 0;
    return bracket_size >> (round + 1);
}

int Bracket_GenerateSingleElim(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries) {
    if (!player_ids || !player_names || num_players < 2 || !out_bracket || max_entries < 1)
        return -1;

    int bracket_size = next_pow2(num_players);
    int total_rounds = log2_int(bracket_size);
    int total_entries = bracket_size - 1;

    if (total_entries > max_entries)
        return -1;

    memset(out_bracket, 0, sizeof(BracketEntry) * (size_t)total_entries);

    // Round 0: first round — seed bracket_size/2 matches
    int first_round_matches = bracket_size / 2;
    int entry_idx = 0;

    for (int pos = 0; pos < first_round_matches; pos++) {
        BracketEntry* e = &out_bracket[entry_idx++];
        e->round = 0;
        e->position = pos;

        // Standard bracket seeding: seed i vs seed (N-1-i)
        int seed_a = pos;
        int seed_b = bracket_size - 1 - pos;

        if (seed_a < num_players) {
            strncpy(e->player1_id, player_ids[seed_a], 63);
            strncpy(e->player1_name, player_names[seed_a], 31);
        }
        if (seed_b < num_players) {
            strncpy(e->player2_id, player_ids[seed_b], 63);
            strncpy(e->player2_name, player_names[seed_b], 31);
        }

        // Auto-advance byes
        if (seed_a < num_players && seed_b >= num_players) {
            strncpy(e->winner_id, e->player1_id, 63);
            e->completed = 1;
        } else if (seed_b < num_players && seed_a >= num_players) {
            strncpy(e->winner_id, e->player2_id, 63);
            e->completed = 1;
        }
    }

    // Subsequent rounds: empty placeholders
    for (int r = 1; r < total_rounds; r++) {
        int matches_in_round = bracket_size >> (r + 1);
        for (int pos = 0; pos < matches_in_round; pos++) {
            BracketEntry* e = &out_bracket[entry_idx++];
            e->round = r;
            e->position = pos;
        }
    }

    // Auto-advance byes into round 1
    for (int i = 0; i < first_round_matches; i++) {
        BracketEntry* e = &out_bracket[i];
        if (e->completed && e->winner_id[0]) {
            const char* winner_name = "";
            if (strcmp(e->winner_id, e->player1_id) == 0)
                winner_name = e->player1_name;
            else
                winner_name = e->player2_name;
            Bracket_AdvanceSingleElim(out_bracket, entry_idx, 0, i,
                                       e->winner_id, winner_name);
        }
    }

    return entry_idx;
}

int Bracket_GenerateSingleElimSeeded(const BracketSeed* seeds,
                                      int num_players,
                                      BracketEntry* out_bracket,
                                      int max_entries) {
    if (!seeds || num_players < 2)
        return -1;

    // Build parallel arrays from seeds
    char (*ids)[64] = (char (*)[64])calloc((size_t)num_players, 64);
    char (*names)[32] = (char (*)[32])calloc((size_t)num_players, 32);
    if (!ids || !names) {
        free(ids);
        free(names);
        return -1;
    }

    for (int i = 0; i < num_players; i++) {
        strncpy(ids[i], seeds[i].player_id, 63);
        strncpy(names[i], seeds[i].display_name, 31);
    }

    int result = Bracket_GenerateSingleElim(ids, names, num_players,
                                             out_bracket, max_entries);
    free(ids);
    free(names);
    return result;
}

// ─── Common Utilities ────────────────────────────────────────────

BracketEntry* Bracket_FindEntry(BracketEntry* bracket, int bracket_size,
                                 int round, int position) {
    for (int i = 0; i < bracket_size; i++) {
        if (bracket[i].round == round && bracket[i].position == position)
            return &bracket[i];
    }
    return NULL;
}

bool Bracket_AdvanceSingleElim(BracketEntry* bracket, int bracket_size,
                                int round, int position,
                                const char* winner_id, const char* winner_name) {
    if (!bracket || !winner_id || bracket_size < 1)
        return false;

    BracketEntry* current = Bracket_FindEntry(bracket, bracket_size, round, position);
    if (!current)
        return false;

    strncpy(current->winner_id, winner_id, 63);
    current->completed = 1;

    int next_round = round + 1;
    int next_position = position / 2;

    BracketEntry* next = Bracket_FindEntry(bracket, bracket_size, next_round, next_position);
    if (!next)
        return true; // Grand final — no next round

    if (position % 2 == 0) {
        strncpy(next->player1_id, winner_id, 63);
        if (winner_name)
            strncpy(next->player1_name, winner_name, 31);
    } else {
        strncpy(next->player2_id, winner_id, 63);
        if (winner_name)
            strncpy(next->player2_name, winner_name, 31);
    }

    return true;
}

const char* Bracket_GetWinner(const BracketEntry* bracket, int bracket_size,
                               int total_rounds) {
    if (!bracket || bracket_size < 1 || total_rounds < 1)
        return "";

    for (int i = 0; i < bracket_size; i++) {
        if (bracket[i].round == total_rounds - 1 && bracket[i].position == 0) {
            if (bracket[i].completed && bracket[i].winner_id[0])
                return bracket[i].winner_id;
            return "";
        }
    }
    return "";
}

// ─── Double Elimination ──────────────────────────────────────────

int Bracket_DoubleElimTotalEntries(int num_players) {
    if (num_players < 2) return 0;
    int bracket_size = next_pow2(num_players);
    int winners_rounds = log2_int(bracket_size);
    int losers_rounds = 2 * (winners_rounds - 1);
    int first_round_matches = bracket_size / 2;

    // Count winners bracket entries
    int total = first_round_matches; // WR0
    for (int r = 1; r < winners_rounds; r++)
        total += bracket_size >> (r + 1);

    // Count losers bracket entries (same formula as generator)
    for (int lr = 0; lr < losers_rounds; lr++) {
        int matches;
        if (lr == 0)
            matches = first_round_matches / 2;
        else if (lr % 2 == 0)
            matches = first_round_matches >> ((lr / 2) + 1);
        else
            matches = first_round_matches >> ((lr + 1) / 2);
        if (matches < 1) matches = 1;
        total += matches;
    }

    // Grand finals
    total += 1;
    return total;
}

int Bracket_GenerateDoubleElim(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries) {
    if (!player_ids || !player_names || num_players < 2 || !out_bracket || max_entries < 1)
        return -1;

    int bracket_size = next_pow2(num_players);
    int winners_rounds = log2_int(bracket_size);
    // Losers bracket: each winners round (except final) drops losers down.
    // Losers bracket has 2*(winners_rounds-1) rounds.
    int losers_rounds = 2 * (winners_rounds - 1);
    int total_entries = Bracket_DoubleElimTotalEntries(num_players);

    if (total_entries > max_entries)
        return -1;

    memset(out_bracket, 0, sizeof(BracketEntry) * (size_t)total_entries);

    // ── Winners bracket: same as single elim (rounds 0..winners_rounds-1) ──
    int entry_idx = 0;
    int first_round_matches = bracket_size / 2;

    for (int pos = 0; pos < first_round_matches; pos++) {
        BracketEntry* e = &out_bracket[entry_idx++];
        e->round = 0;
        e->position = pos;

        int seed_a = pos;
        int seed_b = bracket_size - 1 - pos;

        if (seed_a < num_players) {
            strncpy(e->player1_id, player_ids[seed_a], 63);
            strncpy(e->player1_name, player_names[seed_a], 31);
        }
        if (seed_b < num_players) {
            strncpy(e->player2_id, player_ids[seed_b], 63);
            strncpy(e->player2_name, player_names[seed_b], 31);
        }

        if (seed_a < num_players && seed_b >= num_players) {
            strncpy(e->winner_id, e->player1_id, 63);
            e->completed = 1;
        } else if (seed_b < num_players && seed_a >= num_players) {
            strncpy(e->winner_id, e->player2_id, 63);
            e->completed = 1;
        }
    }

    // Subsequent winners rounds (empty slots)
    for (int r = 1; r < winners_rounds; r++) {
        int matches = bracket_size >> (r + 1);
        for (int pos = 0; pos < matches; pos++) {
            BracketEntry* e = &out_bracket[entry_idx++];
            e->round = r;
            e->position = pos;
        }
    }

    // ── Losers bracket (rounds encoded as winners_rounds + i) ──
    // Losers bracket round structure:
    //   LR0: bracket_size/4 matches (losers from WR0 pair up)
    //   LR1: bracket_size/4 matches (LR0 winners vs WR1 losers)
    //   LR2: bracket_size/8 matches (LR1 winners pair up)
    //   LR3: bracket_size/8 matches (LR2 winners vs WR2 losers)
    //   ...pattern continues
    for (int lr = 0; lr < losers_rounds; lr++) {
        int lr_round = winners_rounds + lr;
        // Even losers rounds: surviving losers pair, halve the count
        // Odd losers rounds: losers vs dropped-down winners, same count
        int matches;
        if (lr == 0)
            matches = first_round_matches / 2;
        else if (lr % 2 == 0)
            matches = first_round_matches >> ((lr / 2) + 1);
        else
            matches = first_round_matches >> ((lr + 1) / 2);

        if (matches < 1) matches = 1;

        for (int pos = 0; pos < matches; pos++) {
            BracketEntry* e = &out_bracket[entry_idx++];
            e->round = lr_round;
            e->position = pos;
        }
    }

    // ── Grand finals ──
    {
        int gf_round = winners_rounds + losers_rounds;
        BracketEntry* gf = &out_bracket[entry_idx++];
        gf->round = gf_round;
        gf->position = 0;
    }

    // Auto-advance byes in winners R0
    for (int i = 0; i < first_round_matches; i++) {
        BracketEntry* e = &out_bracket[i];
        if (e->completed && e->winner_id[0]) {
            // Advance within winners bracket
            int next_round = 1;
            int next_pos = i / 2;
            BracketEntry* next = Bracket_FindEntry(out_bracket, entry_idx, next_round, next_pos);
            if (next) {
                if (i % 2 == 0) {
                    strncpy(next->player1_id, e->winner_id, 63);
                    strncpy(next->player1_name,
                            strcmp(e->winner_id, e->player1_id) == 0 ? e->player1_name : e->player2_name, 31);
                } else {
                    strncpy(next->player2_id, e->winner_id, 63);
                    strncpy(next->player2_name,
                            strcmp(e->winner_id, e->player1_id) == 0 ? e->player1_name : e->player2_name, 31);
                }
            }
        }
    }

    return entry_idx;
}

bool Bracket_AdvanceDoubleElim(BracketEntry* bracket, int bracket_size,
                                int winners_rounds,
                                int round, int position,
                                const char* winner_id, const char* winner_name,
                                const char* loser_id, const char* loser_name) {
    if (!bracket || !winner_id || bracket_size < 1 || winners_rounds < 1)
        return false;

    BracketEntry* current = Bracket_FindEntry(bracket, bracket_size, round, position);
    if (!current)
        return false;

    strncpy(current->winner_id, winner_id, 63);
    current->completed = 1;

    // Determine if this match is in the winners or losers bracket
    bool is_winners = (round < winners_rounds);
    int losers_rounds = 2 * (winners_rounds - 1);
    int gf_round = winners_rounds + losers_rounds;

    if (is_winners) {
        // ── Winners bracket: winner advances within winners ──
        int next_round = round + 1;
        int next_pos = position / 2;

        if (next_round < winners_rounds) {
            BracketEntry* next = Bracket_FindEntry(bracket, bracket_size, next_round, next_pos);
            if (next) {
                if (position % 2 == 0) {
                    strncpy(next->player1_id, winner_id, 63);
                    if (winner_name) strncpy(next->player1_name, winner_name, 31);
                } else {
                    strncpy(next->player2_id, winner_id, 63);
                    if (winner_name) strncpy(next->player2_name, winner_name, 31);
                }
            }
        } else {
            // Winners bracket final winner goes to grand finals as P1
            BracketEntry* gf = Bracket_FindEntry(bracket, bracket_size, gf_round, 0);
            if (gf) {
                strncpy(gf->player1_id, winner_id, 63);
                if (winner_name) strncpy(gf->player1_name, winner_name, 31);
            }
        }

        // ── Loser drops to losers bracket ──
        if (loser_id && loser_id[0]) {
            // Winners round R drops losers into losers round 2*R - 1 (odd rounds
            // = "drop-down" rounds). For WR0 the losers go into LR0 as a special case.
            int lr;
            if (round == 0)
                lr = 0;  // WR0 losers -> LR0
            else
                lr = 2 * round - 1;

            int lr_round = winners_rounds + lr;
            // Position in the losers round: same as winners position / 2 for
            // drop-down rounds, direct mapping for LR0
            int lr_pos = (round == 0) ? position / 2 : position;

            BracketEntry* losers_entry = Bracket_FindEntry(bracket, bracket_size, lr_round, lr_pos);
            if (losers_entry) {
                // Drop-down rounds (odd LR): losers fill P2 slot
                // Pairing rounds (even LR, including LR0): losers fill by position
                if (round == 0) {
                    // LR0: pair up — even positions fill P1, odd fill P2
                    if (position % 2 == 0) {
                        strncpy(losers_entry->player1_id, loser_id, 63);
                        if (loser_name) strncpy(losers_entry->player1_name, loser_name, 31);
                    } else {
                        strncpy(losers_entry->player2_id, loser_id, 63);
                        if (loser_name) strncpy(losers_entry->player2_name, loser_name, 31);
                    }
                } else {
                    // Drop-down: loser fills P2 (P1 comes from previous LR winner)
                    strncpy(losers_entry->player2_id, loser_id, 63);
                    if (loser_name) strncpy(losers_entry->player2_name, loser_name, 31);
                }
            }
        }
    } else if (round < gf_round) {
        // ── Losers bracket: winner advances within losers ──
        int lr = round - winners_rounds; // losers round index (0-based)
        int next_lr = lr + 1;
        int next_lr_round = winners_rounds + next_lr;

        if (next_lr < losers_rounds) {
            int next_pos;
            if (lr % 2 == 0) {
                // Even LR (pairing round): halve positions
                next_pos = position;
            } else {
                // Odd LR (drop-down round): same count, direct map
                next_pos = position / 2;
                // If next round is also even (pairing), positions halve
                if (next_lr % 2 == 0)
                    next_pos = position / 2;
                else
                    next_pos = position;
            }

            BracketEntry* next = Bracket_FindEntry(bracket, bracket_size, next_lr_round, next_pos);
            if (next) {
                // Even LR winners go to P1 of odd LR (drop-down round expects P1 from below)
                if (lr % 2 == 0) {
                    strncpy(next->player1_id, winner_id, 63);
                    if (winner_name) strncpy(next->player1_name, winner_name, 31);
                } else {
                    if (position % 2 == 0) {
                        strncpy(next->player1_id, winner_id, 63);
                        if (winner_name) strncpy(next->player1_name, winner_name, 31);
                    } else {
                        strncpy(next->player2_id, winner_id, 63);
                        if (winner_name) strncpy(next->player2_name, winner_name, 31);
                    }
                }
            }
        } else {
            // Losers bracket final winner goes to grand finals as P2
            BracketEntry* gf = Bracket_FindEntry(bracket, bracket_size, gf_round, 0);
            if (gf) {
                strncpy(gf->player2_id, winner_id, 63);
                if (winner_name) strncpy(gf->player2_name, winner_name, 31);
            }
        }
    } else {
        // Grand finals — winner_id is the tournament champion, no advancement
    }

    return true;
}

// ─── Round Robin ─────────────────────────────────────────────────

int Bracket_RoundRobinTotalEntries(int num_players) {
    if (num_players < 2) return 0;
    return num_players * (num_players - 1) / 2;
}

int Bracket_GenerateRoundRobin(const char player_ids[][64],
                                const char player_names[][32],
                                int num_players,
                                BracketEntry* out_bracket,
                                int max_entries) {
    if (!player_ids || !player_names || num_players < 2 || !out_bracket || max_entries < 1)
        return -1;

    int total = Bracket_RoundRobinTotalEntries(num_players);
    if (total > max_entries)
        return -1;

    memset(out_bracket, 0, sizeof(BracketEntry) * (size_t)total);

    // Circle method scheduling for round robin.
    // With N players (pad to even if odd), N-1 rounds, N/2 matches per round.
    int n = num_players;
    int padded = (n % 2 == 0) ? n : n + 1; // add bye player if odd
    int rounds = padded - 1;

    // Build rotation array
    int* rotation = (int*)calloc((size_t)padded, sizeof(int));
    if (!rotation) return -1;
    for (int i = 0; i < padded; i++)
        rotation[i] = i;

    int entry_idx = 0;

    for (int round = 0; round < rounds; round++) {
        int matches_in_round = padded / 2;
        for (int m = 0; m < matches_in_round; m++) {
            int p1_idx = rotation[m];
            int p2_idx = rotation[padded - 1 - m];

            // Skip matches involving the bye player
            if (p1_idx >= num_players || p2_idx >= num_players)
                continue;

            if (entry_idx >= max_entries) {
                free(rotation);
                return -1;
            }

            BracketEntry* e = &out_bracket[entry_idx++];
            e->round = round;
            e->position = m;
            strncpy(e->player1_id, player_ids[p1_idx], 63);
            strncpy(e->player1_name, player_names[p1_idx], 31);
            strncpy(e->player2_id, player_ids[p2_idx], 63);
            strncpy(e->player2_name, player_names[p2_idx], 31);
        }

        // Rotate: fix position 0, rotate rest clockwise
        int last = rotation[padded - 1];
        for (int i = padded - 1; i > 1; i--)
            rotation[i] = rotation[i - 1];
        rotation[1] = last;
    }

    free(rotation);
    return entry_idx;
}

// ─── Swiss ───────────────────────────────────────────────────────

int Bracket_SwissRounds(int num_players) {
    // Same as single elim depth: ceil(log2(N))
    return Bracket_SingleElimRounds(num_players);
}

/// Swiss pairing helper: sort indices by wins descending (stable via index tiebreak)
typedef struct {
    int index;
    int wins;
} SwissEntry;

static int cmp_swiss_desc(const void* a, const void* b) {
    const SwissEntry* sa = (const SwissEntry*)a;
    const SwissEntry* sb = (const SwissEntry*)b;
    if (sb->wins != sa->wins)
        return sb->wins - sa->wins;
    return sa->index - sb->index; // stable: lower index first
}

int Bracket_GenerateSwissRound(const char player_ids[][64],
                                const char player_names[][32],
                                const int* wins,
                                int num_players,
                                int swiss_round,
                                BracketEntry* out_bracket,
                                int max_entries) {
    if (!player_ids || !player_names || !wins || num_players < 2 ||
        !out_bracket || max_entries < 1)
        return -1;

    int matches_needed = num_players / 2;
    if (matches_needed > max_entries)
        return -1;

    // Build sorted index array
    SwissEntry* sorted = (SwissEntry*)calloc((size_t)num_players, sizeof(SwissEntry));
    if (!sorted) return -1;

    for (int i = 0; i < num_players; i++) {
        sorted[i].index = i;
        sorted[i].wins = wins[i];
    }
    qsort(sorted, (size_t)num_players, sizeof(SwissEntry), cmp_swiss_desc);

    // Pair adjacent players after sorting by record
    memset(out_bracket, 0, sizeof(BracketEntry) * (size_t)matches_needed);
    int entry_idx = 0;

    for (int i = 0; i + 1 < num_players && entry_idx < matches_needed; i += 2) {
        int p1 = sorted[i].index;
        int p2 = sorted[i + 1].index;

        BracketEntry* e = &out_bracket[entry_idx];
        e->round = swiss_round;
        e->position = entry_idx;
        strncpy(e->player1_id, player_ids[p1], 63);
        strncpy(e->player1_name, player_names[p1], 31);
        strncpy(e->player2_id, player_ids[p2], 63);
        strncpy(e->player2_name, player_names[p2], 31);
        entry_idx++;
    }

    free(sorted);
    return entry_idx;
}
