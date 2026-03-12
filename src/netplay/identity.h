/**
 * @file identity.h
 * @brief Persistent player identity for the 3SX netplay lobby.
 *
 * Auto-generates a stable, unique player identity on first launch by
 * hashing random bytes with SHA-256. The identity key and derived player ID
 * persist in config.ini across sessions.
 *
 * Must be called after Config_Init().
 */
#ifndef NETPLAY_IDENTITY_H
#define NETPLAY_IDENTITY_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Initialize identity: load existing keys from config or generate new ones.
/// Must be called after Config_Init().
void Identity_Init(void);

/// Returns the stable player ID string (16 hex chars derived from identity key).
/// Returns "" if not initialized.
const char* Identity_GetPlayerId(void);

/// Returns the display name (auto-generated "Player-XXXX" if not set by user).
/// Returns "" if not initialized.
const char* Identity_GetDisplayName(void);

/// Update the display name (persists to config).
void Identity_SetDisplayName(const char* name);

/// Returns true if identity has been initialized.
bool Identity_IsInitialized(void);

/// Returns the full public key as a hex string (64 chars).
const char* Identity_GetPublicKeyHex(void);

#ifdef __cplusplus
}
#endif

#endif
