#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cmocka.h>

#include "netplay/stun.h"

/* ------------------------------------------------------------------ */
/* Encode / Decode — new string-based "ip|port" format (March 2026)    */
/* ------------------------------------------------------------------ */

/* IPv4 round-trip: encode → decode → same values */
static void test_stun_encode_decode_ipv4(void **state) {
    (void) state;

    char code[64];
    Stun_EncodeEndpoint("192.168.1.100", 7000, code);

    /* Format: "192.168.1.100|7000" */
    assert_string_equal(code, "192.168.1.100|7000");

    char ip[64];
    uint16_t port = 0;
    bool ok = Stun_DecodeEndpoint(code, ip, &port);

    assert_true(ok);
    assert_string_equal(ip, "192.168.1.100");
    assert_int_equal(port, 7000);
}

/* IPv6 round-trip */
static void test_stun_encode_decode_ipv6(void **state) {
    (void) state;

    char code[64];
    Stun_EncodeEndpoint("2001:8a0:587b:a01::1", 55688, code);

    assert_string_equal(code, "2001:8a0:587b:a01::1|55688");

    char ip[64];
    uint16_t port = 0;
    bool ok = Stun_DecodeEndpoint(code, ip, &port);

    assert_true(ok);
    assert_string_equal(ip, "2001:8a0:587b:a01::1");
    assert_int_equal(port, 55688);
}

/* IPv6 full form (worst-case length) */
static void test_stun_encode_decode_ipv6_full(void **state) {
    (void) state;

    /* Full 39-char IPv6 + |port = ~45 chars total — fits in char[64] */
    const char* full_ipv6 = "2001:0db8:85a3:0000:0000:8a2e:0370:7334";
    char code[64];
    Stun_EncodeEndpoint(full_ipv6, 65535, code);

    char ip[64];
    uint16_t port = 0;
    bool ok = Stun_DecodeEndpoint(code, ip, &port);

    assert_true(ok);
    assert_string_equal(ip, full_ipv6);
    assert_int_equal(port, 65535);
}

/* Deterministic: encoding the same input twice gives the same output */
static void test_stun_encode_deterministic(void **state) {
    (void) state;

    char code1[64];
    char code2[64];

    Stun_EncodeEndpoint("10.0.0.1", 12345, code1);
    Stun_EncodeEndpoint("10.0.0.1", 12345, code2);

    assert_string_equal(code1, code2);
}

/* Decode rejects missing '|' separator */
static void test_stun_decode_no_separator(void **state) {
    (void) state;

    char ip[64];
    uint16_t port = 0;

    /* Just an IP, no |port */
    assert_false(Stun_DecodeEndpoint("192.168.1.1", ip, &port));
    /* IPv6 with colons but no pipe */
    assert_false(Stun_DecodeEndpoint("2001:8a0:587b:a", ip, &port));
}

/* Decode rejects empty string */
static void test_stun_decode_empty(void **state) {
    (void) state;

    char ip[64];
    uint16_t port = 0;
    assert_false(Stun_DecodeEndpoint("", ip, &port));
}

/* Decode rejects NULL inputs */
static void test_stun_decode_null(void **state) {
    (void) state;

    char ip[64];
    uint16_t port = 0;
    assert_false(Stun_DecodeEndpoint(NULL, ip, &port));
    assert_false(Stun_DecodeEndpoint("1.2.3.4|5", NULL, &port));
    assert_false(Stun_DecodeEndpoint("1.2.3.4|5", ip, NULL));
}

/* Port 0 edge case */
static void test_stun_decode_port_zero(void **state) {
    (void) state;

    char ip[64];
    uint16_t port = 99;
    bool ok = Stun_DecodeEndpoint("127.0.0.1|0", ip, &port);
    assert_true(ok);
    assert_string_equal(ip, "127.0.0.1");
    assert_int_equal(port, 0);
}

/* Stun_Discover with NULL result returns false without crashing */
static void test_stun_discover_null(void **state) {
    (void) state;
    bool ok = Stun_Discover(NULL, 0);
    assert_false(ok);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_stun_encode_decode_ipv4),
        cmocka_unit_test(test_stun_encode_decode_ipv6),
        cmocka_unit_test(test_stun_encode_decode_ipv6_full),
        cmocka_unit_test(test_stun_encode_deterministic),
        cmocka_unit_test(test_stun_decode_no_separator),
        cmocka_unit_test(test_stun_decode_empty),
        cmocka_unit_test(test_stun_decode_null),
        cmocka_unit_test(test_stun_decode_port_zero),
        cmocka_unit_test(test_stun_discover_null),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
