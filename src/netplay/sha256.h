/**
 * @file sha256.h
 * @brief Portable SHA-256 hash and HMAC-SHA256 functions.
 *
 * Extracted from lobby_server.c for reuse across the netplay subsystem.
 * Public domain — based on the reference by Brad Conte (B-Con).
 */
#ifndef NETPLAY_SHA256_H
#define NETPLAY_SHA256_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint8_t data[64];
    uint32_t datalen;
    uint64_t bitlen;
    uint32_t state[8];
} SHA256_CTX;

/// Initialize a SHA-256 context.
void sha256_init(SHA256_CTX* ctx);

/// Feed data into the SHA-256 context.
void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len);

/// Finalize and produce the 32-byte hash.
void sha256_final(SHA256_CTX* ctx, uint8_t hash[32]);

/// Convenience: hash a buffer in one call.
void sha256_hash(const uint8_t* data, size_t len, uint8_t hash[32]);

/// HMAC-SHA256: produce a 32-byte MAC.
void hmac_sha256(const uint8_t* key, size_t key_len, const uint8_t* msg, size_t msg_len, uint8_t out[32]);

#ifdef __cplusplus
}
#endif

#endif
