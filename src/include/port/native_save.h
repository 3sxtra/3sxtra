/**
 * @file native_save.h
 * @brief Native filesystem save system — replaces PS2 memory card subsystem.
 *
 * Provides direct file I/O for options (INI), direction config (INI), and
 * replay data (binary). All operations are synchronous — no async state
 * machine needed on modern platforms.
 */

#ifndef NATIVE_SAVE_H
#define NATIVE_SAVE_H

#include "structs.h"

/* Replay metadata — forward-declare memcard_date to avoid mcsub.h in C++ */
#ifndef MEMCARD_DATE_DEFINED
#define MEMCARD_DATE_DEFINED
typedef struct memcard_date {
    u8 dayofweek;
    u8 sec;
    u8 min;
    u8 hour;
    u8 day;
    u8 month;
    u16 year;
} memcard_date;
#endif

#ifndef _SUB_INFO_DEFINED
#define _SUB_INFO_DEFINED
typedef struct _sub_info {
    struct memcard_date date;
    s32 player[2];
} _sub_info;
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ── Lifecycle ─────────────────────────────────────────────────────── */

/** Initialize the native save system. Call once at startup. */
void NativeSave_Init(void);

/* ── Options (settings) ────────────────────────────────────────────── */

/** Load options from options.ini into save_w[Present_Mode]. Returns 0 on success. */
int NativeSave_LoadOptions(void);

/** Save current options from save_w[Present_Mode] to options.ini. */
void NativeSave_SaveOptions(void);

/* ── Direction (key config) ────────────────────────────────────────── */

/** Load direction config from direction.ini. Returns 0 on success. */
int NativeSave_LoadDirection(void);

/** Save current direction config to direction.ini. */
void NativeSave_SaveDirection(void);

/* ── Replay ────────────────────────────────────────────────────────── */

#define NATIVE_SAVE_REPLAY_SLOTS 20

/** Check if a replay slot has a saved file. Returns 1=exists, 0=empty. */
int NativeSave_ReplayExists(int slot);

/** Get metadata for a replay slot without loading full data. Returns 0 on success. */
int NativeSave_GetReplayInfo(int slot, _sub_info* out);

/** Load replay data from slot. Returns 0 on success. */
int NativeSave_LoadReplay(int slot);

/** Save current replay data to slot. Returns 0 on success. */
int NativeSave_SaveReplay(int slot);

/** Delete a replay slot. Returns 0 on success. */
int NativeSave_DeleteReplay(int slot);

/* ── Utility ───────────────────────────────────────────────────────── */

/** Get the save directory path (for debug display). */
const char* NativeSave_GetSavePath(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SAVE_H */
