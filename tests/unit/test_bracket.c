/**
 * @file test_bracket.c
 * @brief Unit tests for bracket.c — single-elimination, double-elimination,
 *        round-robin bracket generation and advancement.
 */

#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include <string.h>

#include "netplay/lobby_server.h"
#include "netplay/bracket.h"

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_SingleElimRounds
 * ═══════════════════════════════════════════════════════════════════ */

static void test_rounds_for_2_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(2), 1);
}

static void test_rounds_for_4_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(4), 2);
}

static void test_rounds_for_8_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(8), 3);
}

static void test_rounds_for_6_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(6), 3);
}

static void test_rounds_for_1_player(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(1), 0);
}

static void test_rounds_for_16_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_SingleElimRounds(16), 4);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_GetMatchesInRound
 * ═══════════════════════════════════════════════════════════════════ */

static void test_matches_in_round_8_players(void** state) {
    (void)state;
    // 8 players: round 0 = 4, round 1 = 2, round 2 = 1
    assert_int_equal(Bracket_GetMatchesInRound(8, 0), 4);
    assert_int_equal(Bracket_GetMatchesInRound(8, 1), 2);
    assert_int_equal(Bracket_GetMatchesInRound(8, 2), 1);
    assert_int_equal(Bracket_GetMatchesInRound(8, 3), 0); // out of range
}

static void test_matches_in_round_4_players(void** state) {
    (void)state;
    assert_int_equal(Bracket_GetMatchesInRound(4, 0), 2);
    assert_int_equal(Bracket_GetMatchesInRound(4, 1), 1);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_GenerateSingleElim
 * ═══════════════════════════════════════════════════════════════════ */

static void test_generate_4_players(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 4, bracket, MAX_BRACKET_SIZE);

    assert_int_equal(count, 3);

    assert_int_equal(bracket[0].round, 0);
    assert_int_equal(bracket[0].position, 0);
    assert_string_equal(bracket[0].player1_name, "Alice");
    assert_string_equal(bracket[0].player2_name, "Dave");
    assert_int_equal(bracket[0].completed, 0);

    assert_int_equal(bracket[1].round, 0);
    assert_int_equal(bracket[1].position, 1);
    assert_string_equal(bracket[1].player1_name, "Bob");
    assert_string_equal(bracket[1].player2_name, "Carol");

    assert_int_equal(bracket[2].round, 1);
    assert_int_equal(bracket[2].position, 0);
    assert_string_equal(bracket[2].player1_name, "");
    assert_string_equal(bracket[2].player2_name, "");
}

static void test_generate_3_players_has_bye(void** state) {
    (void)state;
    char ids[3][64] = { "p1", "p2", "p3" };
    char names[3][32] = { "Alice", "Bob", "Carol" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 3, bracket, MAX_BRACKET_SIZE);

    assert_int_equal(count, 3);
    assert_int_equal(bracket[0].completed, 1);
    assert_string_equal(bracket[0].winner_id, "p1");
    assert_int_equal(bracket[1].completed, 0);
    assert_string_equal(bracket[2].player1_name, "Alice");
}

static void test_generate_invalid_input(void** state) {
    (void)state;
    BracketEntry bracket[4];
    assert_int_equal(Bracket_GenerateSingleElim(NULL, NULL, 0, bracket, 4), -1);
    assert_int_equal(Bracket_GenerateSingleElim(NULL, NULL, 1, bracket, 4), -1);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_AdvanceSingleElim
 * ═══════════════════════════════════════════════════════════════════ */

static void test_advance_4_player_bracket(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 4, bracket, MAX_BRACKET_SIZE);
    assert_int_equal(count, 3);

    bool ok = Bracket_AdvanceSingleElim(bracket, count, 0, 0, "p1", "Alice");
    assert_true(ok);
    assert_int_equal(bracket[0].completed, 1);
    assert_string_equal(bracket[2].player1_name, "Alice");

    ok = Bracket_AdvanceSingleElim(bracket, count, 0, 1, "p3", "Carol");
    assert_true(ok);
    assert_int_equal(bracket[1].completed, 1);
    assert_string_equal(bracket[2].player2_name, "Carol");

    ok = Bracket_AdvanceSingleElim(bracket, count, 1, 0, "p1", "Alice");
    assert_true(ok);
    assert_int_equal(bracket[2].completed, 1);

    const char* winner = Bracket_GetWinner(bracket, count, 2);
    assert_string_equal(winner, "p1");
}

static void test_advance_invalid_round(void** state) {
    (void)state;
    char ids[2][64] = { "p1", "p2" };
    char names[2][32] = { "Alice", "Bob" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 2, bracket, MAX_BRACKET_SIZE);
    assert_int_equal(count, 1);

    bool ok = Bracket_AdvanceSingleElim(bracket, count, 5, 0, "p1", "Alice");
    assert_false(ok);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_GetWinner
 * ═══════════════════════════════════════════════════════════════════ */

static void test_winner_before_completion(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 4, bracket, MAX_BRACKET_SIZE);
    const char* winner = Bracket_GetWinner(bracket, count, 2);
    assert_string_equal(winner, "");
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_FindEntry
 * ═══════════════════════════════════════════════════════════════════ */

static void test_find_entry(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElim(ids, names, 4, bracket, MAX_BRACKET_SIZE);

    BracketEntry* e = Bracket_FindEntry(bracket, count, 1, 0);
    assert_non_null(e);
    assert_int_equal(e->round, 1);
    assert_int_equal(e->position, 0);

    BracketEntry* nf = Bracket_FindEntry(bracket, count, 99, 0);
    assert_null(nf);
}

/* ═══════════════════════════════════════════════════════════════════
 *  BracketSeed / SortByRating / GenerateSingleElimSeeded
 * ═══════════════════════════════════════════════════════════════════ */

static void test_sort_by_rating(void** state) {
    (void)state;
    BracketSeed seeds[4] = {
        { "p1", "Low",    0, 1000.0f },
        { "p2", "High",   0, 2500.0f },
        { "p3", "Mid",    0, 1500.0f },
        { "p4", "VHigh",  0, 3000.0f },
    };

    Bracket_SortByRating(seeds, 4);

    // Should be sorted descending by rating
    assert_string_equal(seeds[0].display_name, "VHigh");
    assert_string_equal(seeds[1].display_name, "High");
    assert_string_equal(seeds[2].display_name, "Mid");
    assert_string_equal(seeds[3].display_name, "Low");
    assert_int_equal(seeds[0].seed, 0);
    assert_int_equal(seeds[3].seed, 3);
}

static void test_generate_seeded(void** state) {
    (void)state;
    BracketSeed seeds[4] = {
        { "p1", "Alice", 0, 0.0f },
        { "p2", "Bob",   1, 0.0f },
        { "p3", "Carol", 2, 0.0f },
        { "p4", "Dave",  3, 0.0f },
    };

    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int count = Bracket_GenerateSingleElimSeeded(seeds, 4, bracket, MAX_BRACKET_SIZE);
    assert_int_equal(count, 3);

    // Same as GenerateSingleElim — seeds map to player_ids
    assert_string_equal(bracket[0].player1_name, "Alice");
    assert_string_equal(bracket[0].player2_name, "Dave");
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_GenerateDoubleElim
 * ═══════════════════════════════════════════════════════════════════ */

static void test_double_elim_4_players(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int total_expected = Bracket_DoubleElimTotalEntries(4);
    // 4 players → bracket_size=4, 2*(4-1)+1 = 7
    assert_int_equal(total_expected, 7);

    int count = Bracket_GenerateDoubleElim(ids, names, 4, bracket, MAX_BRACKET_SIZE);
    assert_true(count > 0);
    assert_int_equal(count, 7);

    // Winners round 0: 2 matches
    assert_int_equal(bracket[0].round, 0);
    assert_string_equal(bracket[0].player1_name, "Alice");
    assert_string_equal(bracket[0].player2_name, "Dave");

    assert_int_equal(bracket[1].round, 0);
    assert_string_equal(bracket[1].player1_name, "Bob");
    assert_string_equal(bracket[1].player2_name, "Carol");

    // Winners round 1 (finals): 1 match
    assert_int_equal(bracket[2].round, 1);

    // Last entry should be grand finals
    assert_int_equal(bracket[count - 1].round > 1, 1); // GF round > winners rounds
}

static void test_double_elim_invalid(void** state) {
    (void)state;
    BracketEntry bracket[4];
    assert_int_equal(Bracket_GenerateDoubleElim(NULL, NULL, 0, bracket, 4), -1);
    assert_int_equal(Bracket_DoubleElimTotalEntries(1), 0);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Bracket_GenerateRoundRobin
 * ═══════════════════════════════════════════════════════════════════ */

static void test_round_robin_4_players(void** state) {
    (void)state;
    char ids[4][64] = { "p1", "p2", "p3", "p4" };
    char names[4][32] = { "Alice", "Bob", "Carol", "Dave" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int total_expected = Bracket_RoundRobinTotalEntries(4);
    // 4 choose 2 = 6 matches
    assert_int_equal(total_expected, 6);

    int count = Bracket_GenerateRoundRobin(ids, names, 4, bracket, MAX_BRACKET_SIZE);
    assert_int_equal(count, 6);

    // Verify every player pair appears exactly once
    int pair_count[4][4] = { 0 };
    for (int i = 0; i < count; i++) {
        int p1 = -1, p2 = -1;
        for (int j = 0; j < 4; j++) {
            if (strcmp(bracket[i].player1_id, ids[j]) == 0) p1 = j;
            if (strcmp(bracket[i].player2_id, ids[j]) == 0) p2 = j;
        }
        assert_true(p1 >= 0 && p2 >= 0);
        pair_count[p1][p2]++;
        pair_count[p2][p1]++;
    }
    // Each pair should appear exactly once
    for (int a = 0; a < 4; a++)
        for (int b = a + 1; b < 4; b++)
            assert_int_equal(pair_count[a][b], 1);
}

static void test_round_robin_3_players(void** state) {
    (void)state;
    char ids[3][64] = { "p1", "p2", "p3" };
    char names[3][32] = { "Alice", "Bob", "Carol" };
    BracketEntry bracket[MAX_BRACKET_SIZE];
    memset(bracket, 0, sizeof(bracket));

    int total_expected = Bracket_RoundRobinTotalEntries(3);
    assert_int_equal(total_expected, 3);

    int count = Bracket_GenerateRoundRobin(ids, names, 3, bracket, MAX_BRACKET_SIZE);
    assert_int_equal(count, 3);
}

static void test_round_robin_invalid(void** state) {
    (void)state;
    BracketEntry bracket[4];
    assert_int_equal(Bracket_GenerateRoundRobin(NULL, NULL, 0, bracket, 4), -1);
    assert_int_equal(Bracket_RoundRobinTotalEntries(1), 0);
}

/* ═══════════════════════════════════════════════════════════════════
 *  Main
 * ═══════════════════════════════════════════════════════════════════ */

int main(void) {
    const struct CMUnitTest tests[] = {
        /* Single elim rounds */
        cmocka_unit_test(test_rounds_for_2_players),
        cmocka_unit_test(test_rounds_for_4_players),
        cmocka_unit_test(test_rounds_for_8_players),
        cmocka_unit_test(test_rounds_for_6_players),
        cmocka_unit_test(test_rounds_for_1_player),
        cmocka_unit_test(test_rounds_for_16_players),
        /* GetMatchesInRound */
        cmocka_unit_test(test_matches_in_round_8_players),
        cmocka_unit_test(test_matches_in_round_4_players),
        /* Single elim generation */
        cmocka_unit_test(test_generate_4_players),
        cmocka_unit_test(test_generate_3_players_has_bye),
        cmocka_unit_test(test_generate_invalid_input),
        /* Single elim advancement */
        cmocka_unit_test(test_advance_4_player_bracket),
        cmocka_unit_test(test_advance_invalid_round),
        /* Winner detection */
        cmocka_unit_test(test_winner_before_completion),
        /* FindEntry */
        cmocka_unit_test(test_find_entry),
        /* BracketSeed / rating sort */
        cmocka_unit_test(test_sort_by_rating),
        cmocka_unit_test(test_generate_seeded),
        /* Double elimination */
        cmocka_unit_test(test_double_elim_4_players),
        cmocka_unit_test(test_double_elim_invalid),
        /* Round robin */
        cmocka_unit_test(test_round_robin_4_players),
        cmocka_unit_test(test_round_robin_3_players),
        cmocka_unit_test(test_round_robin_invalid),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
