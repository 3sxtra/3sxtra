#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <cmocka.h>

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include "netplay/stun.h"

static void test_stun_encode_decode_roundtrip(void **state) {
    (void) state;

    uint32_t ip = htonl(0x7F000001); 
    uint16_t port = htons(12345);
    char code[9];

    Stun_EncodeEndpoint(ip, port, code);
    assert_int_equal(strlen(code), 8);

    uint32_t decoded_ip;
    uint16_t decoded_port;
    bool success = Stun_DecodeEndpoint(code, &decoded_ip, &decoded_port);

    assert_true(success);
    assert_int_equal(decoded_ip, ip);
    assert_int_equal(decoded_port, port);
}

static void test_stun_encode_deterministic(void **state) {
    (void) state;

    uint32_t ip = htonl(0xC0A80164);
    uint16_t port = htons(7000);
    char code1[9];
    char code2[9];

    Stun_EncodeEndpoint(ip, port, code1);
    Stun_EncodeEndpoint(ip, port, code2);
    
    assert_string_equal(code1, code2);
    
    const char *alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
    for(int i=0; i<8; ++i) {
        assert_non_null(strchr(alphabet, code1[i]));
    }
}

static void test_stun_decode_invalid(void **state) {
    (void) state;
    
    uint32_t ip;
    uint16_t port;

    assert_false(Stun_DecodeEndpoint("Short", &ip, &port));
    assert_false(Stun_DecodeEndpoint("TooLongCode", &ip, &port));
    assert_false(Stun_DecodeEndpoint("Bad!Char", &ip, &port));
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_stun_encode_decode_roundtrip),
        cmocka_unit_test(test_stun_encode_deterministic),
        cmocka_unit_test(test_stun_decode_invalid),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
