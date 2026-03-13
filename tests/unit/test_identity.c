/**
 * @file test_identity.c
 * @brief Unit tests for the player identity module (identity.c).
 *
 * Uses the #include "*.c" pattern (same as test_lobby_server.c) to access
 * static internals. Mocks Config_GetString/Config_SetString/Config_Save
 * via cmocka.
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <cmocka.h>
#include <string.h>

/* === Mocks for config.c === */

static char mock_config_store[16][2][256]; /* [slot][0=key,1=value] */
static int mock_config_count = 0;
static int mock_config_save_calls = 0;

static const char* mock_config_find(const char* key) {
    for (int i = 0; i < mock_config_count; i++) {
        if (strcmp(mock_config_store[i][0], key) == 0)
            return mock_config_store[i][1];
    }
    return NULL;
}

const char* Config_GetString(const char* key) {
    return mock_config_find(key);
}

void Config_SetString(const char* key, const char* value) {
    /* Update existing or add new */
    for (int i = 0; i < mock_config_count; i++) {
        if (strcmp(mock_config_store[i][0], key) == 0) {
            snprintf(mock_config_store[i][1], 256, "%s", value);
            return;
        }
    }
    if (mock_config_count < 16) {
        snprintf(mock_config_store[mock_config_count][0], 256, "%s", key);
        snprintf(mock_config_store[mock_config_count][1], 256, "%s", value);
        mock_config_count++;
    }
}

void Config_Save(void) {
    mock_config_save_calls++;
}

static void reset_mock_config(void) {
    mock_config_count = 0;
    mock_config_save_calls = 0;
    memset(mock_config_store, 0, sizeof(mock_config_store));
}

/* Include the source file to access static variables */
#include "../../src/netplay/identity.c"

/* === Tests === */

static void test_generate_new_identity(void** state) {
    (void)state;
    reset_mock_config();

    Identity_Init();

    /* Should have generated and stored keys */
    assert_true(initialized);
    assert_true(strlen(player_id) == 16);
    assert_true(strlen(public_key_hex) == 64);
    assert_true(strlen(secret_key_hex) == 64);

    /* Keys should be different from each other */
    assert_string_not_equal(public_key_hex, secret_key_hex);

    /* Config should have been saved */
    assert_true(mock_config_save_calls > 0);

    /* Config store should contain the keys */
    const char* stored_pub = mock_config_find("identity-public-key");
    const char* stored_sec = mock_config_find("identity-secret-key");
    assert_non_null(stored_pub);
    assert_non_null(stored_sec);
    assert_string_equal(stored_pub, public_key_hex);
    assert_string_equal(stored_sec, secret_key_hex);

    /* Display name should be auto-generated */
    assert_true(strlen(display_name) > 0);
    assert_true(strncmp(display_name, "Player-", 7) == 0);
}

static void test_load_existing_identity(void** state) {
    (void)state;
    reset_mock_config();

    /* Pre-set keys in mock config */
    const char* test_pub = "a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6a7b8c9d0e1f2a3b4c5d6a7b8c9d0e1f2";
    const char* test_sec = "f1e2d3c4b5a6f7e8d9c0b1a2f3e4d5c6b7a8f9e0d1c2b3a4f5e6d7c8b9a0f1e2";
    Config_SetString("identity-public-key", test_pub);
    Config_SetString("identity-secret-key", test_sec);
    Config_SetString("lobby-display-name", "TestPlayer");
    mock_config_save_calls = 0; /* Reset after our setup */

    Identity_Init();

    assert_true(initialized);
    /* player_id should be first 16 chars of the public key */
    assert_string_equal(player_id, "a1b2c3d4e5f6a7b8");
    assert_string_equal(public_key_hex, test_pub);
    assert_string_equal(secret_key_hex, test_sec);
    /* Display name should be preserved */
    assert_string_equal(display_name, "TestPlayer");
    /* Config_Save should NOT have been called (no new generation needed) */
    assert_int_equal(mock_config_save_calls, 0);
}

static void test_player_id_stability(void** state) {
    (void)state;
    reset_mock_config();

    /* Generate identity */
    Identity_Init();
    char first_id[32];
    snprintf(first_id, sizeof(first_id), "%s", Identity_GetPlayerId());
    char first_pub[128];
    snprintf(first_pub, sizeof(first_pub), "%s", Identity_GetPublicKeyHex());

    /* Re-init should load the same identity */
    Identity_Init();
    assert_string_equal(Identity_GetPlayerId(), first_id);
    assert_string_equal(Identity_GetPublicKeyHex(), first_pub);
}

static void test_display_name_preserved(void** state) {
    (void)state;
    reset_mock_config();

    Config_SetString("lobby-display-name", "MyCustomName");

    /* Generate identity (no keys yet) */
    Identity_Init();

    /* Display name should be the custom one, not auto-generated */
    assert_string_equal(Identity_GetDisplayName(), "MyCustomName");
}

static void test_set_display_name(void** state) {
    (void)state;
    reset_mock_config();

    Identity_Init();
    Identity_SetDisplayName("NewName");

    assert_string_equal(Identity_GetDisplayName(), "NewName");

    /* Verify persisted to config */
    const char* stored = mock_config_find("lobby-display-name");
    assert_non_null(stored);
    assert_string_equal(stored, "NewName");
}

static void test_invalid_keys_regenerate(void** state) {
    (void)state;
    reset_mock_config();

    /* Set invalid (too short) keys */
    Config_SetString("identity-public-key", "tooshort");
    Config_SetString("identity-secret-key", "alsonotvalid");

    Identity_Init();

    /* Should have regenerated */
    assert_true(initialized);
    assert_true(strlen(public_key_hex) == 64);
    assert_true(strlen(secret_key_hex) == 64);
    /* Old invalid values should be overwritten */
    assert_string_not_equal(public_key_hex, "tooshort");
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_generate_new_identity),
        cmocka_unit_test(test_load_existing_identity),
        cmocka_unit_test(test_player_id_stability),
        cmocka_unit_test(test_display_name_preserved),
        cmocka_unit_test(test_set_display_name),
        cmocka_unit_test(test_invalid_keys_regenerate),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
