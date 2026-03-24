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

/** Load options from options.ini into CurrentSave(). Returns 0 on success. */
int NativeSave_LoadOptions(void);

/** Save current options from CurrentSave() to options.ini. */
void NativeSave_SaveOptions(void);

/* ── Direction (key config) ────────────────────────────────────────── */

/** Load direction config from direction.ini. Returns 0 on success. */
int NativeSave_LoadDirection(void);

/** Save current direction config to direction.ini. */
void NativeSave_SaveDirection(void);

/* ── Replay ────────────────────────────────────────────────────────── */

/* Used by legacy UI screens still allocating static buffers, max number to display */
#define NATIVE_SAVE_REPLAY_SLOTS 999999
#define NATIVE_SAVE_NETPLAY_BASE 10000

/** Find all replays for a given mode. Returns the number of replays found (sorted descending by filename). */
int NativeSave_FindAllReplays(char out_filenames[][128], int max_count, int is_netplay);

/** Check if a replay file exists. Returns 1=exists, 0=empty. */
int NativeSave_ReplayExists(const char* filename);

/** Get metadata for a replay file without loading full data. Returns 0 on success. */
int NativeSave_GetReplayInfo(const char* filename, _sub_info* out);

/** Load replay data from filename. Returns 0 on success. */
int NativeSave_LoadReplay(const char* filename);

/** Save current replay data to the given filename. Returns 0 on success. */
int NativeSave_SaveReplay(const char* filename);

/** Delete a replay file. Returns 0 on success. */
int NativeSave_DeleteReplay(const char* filename);

/** Auto-save replay using descriptive filename. Returns 0 on success. */
int NativeSave_AutoSaveReplay(int is_netplay);

/* ── Utility ───────────────────────────────────────────────────────── */

/** Get the save directory path (for debug display). */
const char* NativeSave_GetSavePath(void);

#ifdef __cplusplus
}
#endif

#endif /* NATIVE_SAVE_H */
