/**
 * @file identity.c
 * @brief Persistent player identity for the 3SX netplay lobby.
 *
 * On first launch, generates 32 random bytes using SDL_rand_bits() and
 * hashes them with SHA-256 to produce a stable identity key. The key
 * and its hex representation are persisted in config.ini.
 *
 * The player_id is the first 16 hex characters of the SHA-256 hash
 * (8 bytes of entropy — sufficient for <100K concurrent players).
 *
 * If no display name is set, auto-generates "Player-XXXX" using
 * 4 random hex characters.
 */
#include "identity.h"
#include "port/config/config.h"
#include "sha256.h"
#include <SDL3/SDL.h>
#include <stdio.h>
#include <string.h>

#define IDENTITY_KEY_BYTES 32
#define IDENTITY_HEX_LEN (IDENTITY_KEY_BYTES * 2) /* 64 hex chars */
#define PLAYER_ID_LEN 16                          /* first 16 hex chars */

static bool initialized = false;
static char public_key_hex[IDENTITY_HEX_LEN + 1]; /* 64 chars + null */
static char secret_key_hex[IDENTITY_HEX_LEN + 1];
static char player_id[PLAYER_ID_LEN + 1]; /* 16 chars + null */
static char display_name[32];

/* Convert a byte array to a hex string */
static void bytes_to_hex(const uint8_t* bytes, size_t len, char* out) {
    for (size_t i = 0; i < len; i++) {
        snprintf(out + i * 2, 3, "%02x", bytes[i]);
    }
    out[len * 2] = '\0';
}

/* Parse hex string into bytes. Returns number of bytes parsed. */
static size_t hex_to_bytes(const char* hex, uint8_t* out, size_t max_bytes) {
    size_t hex_len = strlen(hex);
    size_t byte_count = hex_len / 2;
    if (byte_count > max_bytes)
        byte_count = max_bytes;
    for (size_t i = 0; i < byte_count; i++) {
        unsigned int val = 0;
        if (sscanf(hex + i * 2, "%2x", &val) != 1)
            return i;
        out[i] = (uint8_t)val;
    }
    return byte_count;
}

#ifdef _WIN32
#include <windows.h>
#include <bcrypt.h>
#elif defined(__APPLE__)
#include <stdlib.h>
#else
#include <sys/random.h>
#endif

/* Generate cryptographically secure random bytes.
 * Uses BCryptGenRandom on Windows and getrandom on Unix/Linux.
 * Falls back to SDL_rand_bits if native APIs fail (though they shouldn't). */
static void generate_random_bytes(uint8_t* buf, size_t len) {
    bool success = false;

#ifdef _WIN32
    if (BCryptGenRandom(NULL, buf, (ULONG)len, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0) {
        success = true;
    }
#elif defined(__APPLE__)
    arc4random_buf(buf, len);
    success = true;
#else
    if (getrandom(buf, len, 0) == (ssize_t)len) {
        success = true;
    }
#endif

    if (!success) {
        SDL_LogWarn(SDL_LOG_CATEGORY_APPLICATION, "[Identity] CSPRNG failed, falling back to PRNG");
        for (size_t i = 0; i < len; i += 4) {
            uint32_t r = SDL_rand_bits();
            size_t remaining = len - i;
            if (remaining > 4)
                remaining = 4;
            memcpy(buf + i, &r, remaining);
        }
    }
}

/* Generate a new identity keypair */
static void generate_identity(void) {
    uint8_t random_seed[IDENTITY_KEY_BYTES];
    uint8_t hash[32];

    /* Generate random seed and hash it for the "public key" */
    generate_random_bytes(random_seed, IDENTITY_KEY_BYTES);
    sha256_hash(random_seed, IDENTITY_KEY_BYTES, hash);
    bytes_to_hex(hash, 32, public_key_hex);

    /* Generate a separate random seed for the "secret key" */
    generate_random_bytes(random_seed, IDENTITY_KEY_BYTES);
    sha256_hash(random_seed, IDENTITY_KEY_BYTES, hash);
    bytes_to_hex(hash, 32, secret_key_hex);

    /* Persist to config */
    Config_SetString(CFG_KEY_IDENTITY_PUBLIC, public_key_hex);
    Config_SetString(CFG_KEY_IDENTITY_SECRET, secret_key_hex);
    Config_Save();

    SDL_Log("[Identity] Generated new identity: %s", public_key_hex);
}

/* Load identity from config. Returns true if both keys exist and are valid. */
static bool load_identity(void) {
    const char* pub = Config_GetString(CFG_KEY_IDENTITY_PUBLIC);
    const char* sec = Config_GetString(CFG_KEY_IDENTITY_SECRET);

    if (!pub || !sec || strlen(pub) != IDENTITY_HEX_LEN || strlen(sec) != IDENTITY_HEX_LEN) {
        return false;
    }

    /* Validate that both are valid hex */
    uint8_t tmp[32];
    if (hex_to_bytes(pub, tmp, 32) != 32)
        return false;
    if (hex_to_bytes(sec, tmp, 32) != 32)
        return false;

    SDL_strlcpy(public_key_hex, pub, sizeof(public_key_hex));
    SDL_strlcpy(secret_key_hex, sec, sizeof(secret_key_hex));
    return true;
}

/* Generate an auto display name like "Player-A1B2" */
static void generate_display_name(void) {
    uint32_t r = SDL_rand_bits();
    snprintf(display_name, sizeof(display_name), "Player-%04X", r & 0xFFFF);
    Config_SetString(CFG_KEY_LOBBY_DISPLAY_NAME, display_name);
    Config_Save();
    SDL_Log("[Identity] Generated display name: %s", display_name);
}

void Identity_Init(void) {
    memset(public_key_hex, 0, sizeof(public_key_hex));
    memset(secret_key_hex, 0, sizeof(secret_key_hex));
    memset(player_id, 0, sizeof(player_id));
    memset(display_name, 0, sizeof(display_name));

    /* Try to load existing identity */
    if (!load_identity()) {
        generate_identity();
    }

    /* Derive player_id from first 16 hex chars of public key */
    memcpy(player_id, public_key_hex, PLAYER_ID_LEN);
    player_id[PLAYER_ID_LEN] = '\0';

    /* Load or generate display name */
    const char* existing_name = Config_GetString(CFG_KEY_LOBBY_DISPLAY_NAME);
    if (existing_name && existing_name[0]) {
        SDL_strlcpy(display_name, existing_name, sizeof(display_name));
    } else {
        generate_display_name();
    }

    initialized = true;
    SDL_Log("[Identity] Initialized — player_id=%s display_name=%s", player_id, display_name);
}

const char* Identity_GetPlayerId(void) {
    return initialized ? player_id : "";
}

const char* Identity_GetDisplayName(void) {
    return initialized ? display_name : "";
}

void Identity_SetDisplayName(const char* name) {
    if (!name || !name[0])
        return;
    SDL_strlcpy(display_name, name, sizeof(display_name));
    Config_SetString(CFG_KEY_LOBBY_DISPLAY_NAME, display_name);
    Config_Save();
}

bool Identity_IsInitialized(void) {
    return initialized;
}

const char* Identity_GetPublicKeyHex(void) {
    return initialized ? public_key_hex : "";
}
