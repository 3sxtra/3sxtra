#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <cmocka.h>
#include <string.h>

/* We mock Config_GetString using cmocka's mock() system */
const char* Config_GetString(const char* key) {
    check_expected(key);
    return (const char*)mock();
}

/* Stubs for Identity module — lobby_server.c calls these for room management */
static bool mock_identity_initialized = false;
static char mock_identity_player_id[32] = "test_player_1234";
static char mock_identity_display[32] = "TestPlayer";
static char mock_identity_pubkey[65] = "0000000000000000000000000000000000000000000000000000000000000000";

bool Identity_IsInitialized(void) { return mock_identity_initialized; }
const char* Identity_GetPlayerId(void) { return mock_identity_player_id; }
const char* Identity_GetDisplayName(void) { return mock_identity_display; }
const char* Identity_GetPublicKeyHex(void) { return mock_identity_pubkey; }

/* Include the source file to access static variables directly */
#include "../../src/netplay/lobby_server.c"

static void test_init_with_defaults(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, NULL);
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, NULL);

    LobbyServer_Init();

    assert_true(configured);
    
    // Parse the default URL to verify the expected host/port
    const char* default_p = DEFAULT_LOBBY_URL;
    if (strncmp(default_p, "http://", 7) == 0) default_p += 7;
    const char* default_colon = strchr(default_p, ':');
    char expected_host[256] = {0};
    int expected_port = 80;
    if (default_colon) {
        memcpy(expected_host, default_p, default_colon - default_p);
        expected_port = atoi(default_colon + 1);
    } else {
        strcpy(expected_host, default_p);
    }

    assert_string_equal(server_host, expected_host);
    assert_int_equal(server_port, expected_port);
    assert_string_equal(server_key, DEFAULT_LOBBY_KEY);
}

static void test_init_with_custom_url_and_port(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, "http://example.com:8080");
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, "my_custom_key");

    LobbyServer_Init();

    assert_true(configured);
    assert_string_equal(server_host, "example.com");
    assert_int_equal(server_port, 8080);
    assert_string_equal(server_key, "my_custom_key");
}

static void test_init_with_custom_url_no_port(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, "http://mylobby.net");
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, "secret");

    LobbyServer_Init();

    assert_true(configured);
    assert_string_equal(server_host, "mylobby.net");
    assert_int_equal(server_port, 80); // Default port
    assert_string_equal(server_key, "secret");
}

static void test_init_with_custom_url_no_scheme(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, "localhost:9000");
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, "local_key");

    LobbyServer_Init();

    assert_true(configured);
    assert_string_equal(server_host, "localhost");
    assert_int_equal(server_port, 9000);
    assert_string_equal(server_key, "local_key");
}

static void test_init_with_trailing_slash(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, "http://api.domain.com/");
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, "slash_key");

    LobbyServer_Init();

    assert_true(configured);
    assert_string_equal(server_host, "api.domain.com");
    assert_int_equal(server_port, 80);
    assert_string_equal(server_key, "slash_key");
}

static void test_init_missing_key_from_config_uses_default(void **state) {
    (void) state;
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_URL);
    will_return(Config_GetString, "http://example.com:1234");
    
    expect_string(Config_GetString, key, CFG_KEY_LOBBY_SERVER_KEY);
    will_return(Config_GetString, ""); // Empty string should fallback to default key

    LobbyServer_Init();

    assert_true(configured);
    assert_string_equal(server_host, "example.com");
    assert_int_equal(server_port, 1234);
    assert_string_equal(server_key, DEFAULT_LOBBY_KEY);
}

/* Task 5: UpdatePresence when NOT configured (configured == false).
   http_request() returns false immediately without opening any socket. */
static void test_update_presence_not_connected(void **state) {
    (void) state;
    /* Reset the module state: call Init with NULL returns from Config_GetString
       but that would consume mock expectations.  Instead, force configured=false
       directly via the static variable accessible through the #include "*.c" pattern. */
    configured = false;
    bool result = LobbyServer_UpdatePresence("pid", "Player", "US", "CODE", NULL, -1, "wired");
    assert_false(result);
}

/* Various lobby APIs should return failure/0 when not configured,
   without hanging or crashing (timeout handling). */
static void test_lobby_apis_not_configured(void **state) {
    (void) state;
    configured = false;

    /* GetSearching returns 0 players */
    LobbyPlayer players[4];
    int count = LobbyServer_GetSearching(players, 4, NULL);
    assert_int_equal(count, 0);

    /* Leave returns false */
    bool left = LobbyServer_Leave("test_player");
    assert_false(left);

    /* StartSearching returns false */
    bool searching = LobbyServer_StartSearching("test_player");
    assert_false(searching);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_init_with_defaults),
        cmocka_unit_test(test_init_with_custom_url_and_port),
        cmocka_unit_test(test_init_with_custom_url_no_port),
        cmocka_unit_test(test_init_with_custom_url_no_scheme),
        cmocka_unit_test(test_init_with_trailing_slash),
        cmocka_unit_test(test_init_missing_key_from_config_uses_default),
        /* Task 5 addition */
        cmocka_unit_test(test_update_presence_not_connected),
        cmocka_unit_test(test_lobby_apis_not_configured),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
