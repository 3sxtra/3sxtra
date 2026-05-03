/**
 * @file native_save.c
 * @brief Native filesystem save system implementation.
 *
 * Replaces the PS2 memory card subsystem (sdk_libmc.c + mcsub.c + savesub.c)
 * with direct file I/O. Options and direction use human-readable INI format.
 * Replay data uses binary format (performance-sensitive, ~30KB per file).
 *
 * Save directory: SDL_GetPrefPath("CrowdedStreet", "3SX") via Paths_GetPrefPath().
 * Files: options.ini, direction.ini, replays/replay_NN.bin, replays/replay_NN.meta
 */

#include "port/save/native_save.h"
#include "game_state.h"
#include "common.h"

#include "sf33rd/Source/Game/engine/state_user.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/system_subroutines.h"
#include "sf33rd/Source/Game/system/system_subroutines_2.h"
#include "sf33rd/Source/Game/system/work_sys.h"

#include <SDL3/SDL.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#ifndef _WIN32
#include <unistd.h>
#endif

/* ── Forward declarations ──────────────────────────────────────────── */

/* Defined in paths.c */
const char* Paths_GetPrefPath(void);

/* Externs from game code — use the headers' types */
extern void Copy_Save_w(void);
extern void Copy_Check_w(void);
extern void Save_Game_Data(void);

/* ── Path helpers ──────────────────────────────────────────────────── */

static char save_dir[512] = { 0 };

static void ensure_save_dir(void) {
    if (save_dir[0] == '\0') {
        const char* pref = Paths_GetPrefPath();
        if (pref) {
            snprintf(save_dir, sizeof(save_dir), "%s", pref);
        } else {
            snprintf(save_dir, sizeof(save_dir), "./");
        }
    }
}

static void make_path(char* dst, size_t dst_size, const char* filename) {
    ensure_save_dir();
    snprintf(dst, dst_size, "%s%s", save_dir, filename);
}

static void ensure_replay_dir(void) {
    static int done = 0;
    if (done)
        return;
    ensure_save_dir();
    char replay_dir[512];
    snprintf(replay_dir, sizeof(replay_dir), "%sreplays", save_dir);
    SDL_CreateDirectory(replay_dir);
    done = 1;
}

static void make_replay_path(char* dst, size_t dst_size, const char* filename, const char* ext) {
    ensure_save_dir();
    ensure_replay_dir();
    snprintf(dst, dst_size, "%sreplays/%s%s", save_dir, filename, ext);
}

/* ── INI parser helpers ────────────────────────────────────────────── */

static void ini_trim(char* s) {
    /* trim trailing whitespace */
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r' || s[len - 1] == ' ' || s[len - 1] == '\t')) {
        s[--len] = '\0';
    }
}

static int ini_read_int(FILE* f, const char* key, int def) {
    char line[256];

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        char* eq = strchr(line, '=');
        if (!eq || eq == line)
            continue;
        size_t key_len = (size_t)(eq - line);
        char found_key[128];
        if (key_len >= sizeof(found_key))
            continue;
        memcpy(found_key, line, key_len);
        found_key[key_len] = '\0';
        ini_trim(found_key);
        if (strcmp(found_key, key) == 0) {
            char* endp;
            long v = strtol(eq + 1, &endp, 10);
            if (endp != eq + 1)
                return (int)v;
        }
    }
    return def;
}

static void ini_read_bytes(FILE* f, const char* key, u8* dst, int count) {
    char line[1024];

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        char* eq = strchr(line, '=');
        if (!eq || eq == line)
            continue;
        size_t key_len = (size_t)(eq - line);
        char found_key[128];
        if (key_len >= sizeof(found_key))
            continue;
        memcpy(found_key, line, key_len);
        found_key[key_len] = '\0';
        ini_trim(found_key);
        if (strcmp(found_key, key) == 0) {
            char* p = eq + 1;
            for (int i = 0; i < count && *p; i++) {
                dst[i] = (u8)strtol(p, &p, 10);
                if (*p == ',')
                    p++;
            }
            return;
        }
    }
}

static void ini_read_s8_array(FILE* f, const char* key, s8* dst, int count) {
    char line[1024];

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        char* eq = strchr(line, '=');
        if (!eq || eq == line)
            continue;
        size_t key_len = (size_t)(eq - line);
        char found_key[128];
        if (key_len >= sizeof(found_key))
            continue;
        memcpy(found_key, line, key_len);
        found_key[key_len] = '\0';
        ini_trim(found_key);
        if (strcmp(found_key, key) == 0) {
            char* p = eq + 1;
            for (int i = 0; i < count && *p; i++) {
                dst[i] = (s8)strtol(p, &p, 10);
                if (*p == ',')
                    p++;
            }
            return;
        }
    }
}

static void write_bytes(FILE* f, const char* key, const u8* data, int count) {
    fprintf(f, "%s=", key);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%d%s", data[i], (i < count - 1) ? "," : "");
    }
    fprintf(f, "\n");
}

static void write_s8_array(FILE* f, const char* key, const s8* data, int count) {
    fprintf(f, "%s=", key);
    for (int i = 0; i < count; i++) {
        fprintf(f, "%d%s", data[i], (i < count - 1) ? "," : "");
    }
    fprintf(f, "\n");
}

/* ── Atomic write helper ───────────────────────────────────────────── */

/* Write to .tmp then rename — prevents corruption on crash/power loss */
static FILE* atomic_open(const char* path, char* tmp_path, size_t tmp_size) {
    snprintf(tmp_path, tmp_size, "%s.tmp", path);
    return fopen(tmp_path, "w");
}

static int atomic_commit(const char* path, const char* tmp_path) {
    /* On Windows, remove target first since rename() fails if target exists */
#ifdef _WIN32
    _unlink(path);
#else
    unlink(path);
#endif
    if (rename(tmp_path, path) != 0) {
        SDL_Log("[NativeSave] ERROR: rename %s -> %s failed: %s", tmp_path, path, strerror(errno));
        return -1;
    }
    return 0;
}

/* ── Binary atomic write helper ────────────────────────────────────── */

static FILE* atomic_open_bin(const char* path, char* tmp_path, size_t tmp_size) {
    snprintf(tmp_path, tmp_size, "%s.tmp", path);
    return fopen(tmp_path, "wb");
}

/* ── Lifecycle ─────────────────────────────────────────────────────── */

/** @brief Initialize the native save system. Call once at startup. */
void NativeSave_Init(void) {
    ensure_save_dir();
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] Save directory: %s", save_dir);
}

/** @brief Get the save directory path (for debug display). */
const char* NativeSave_GetSavePath(void) {
    ensure_save_dir();
    return save_dir;
}

/* ═══════════════════════════════════════════════════════════════════
 *  OPTIONS  —  INI format
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief Load options from options.ini into CurrentSave(). */
int NativeSave_LoadOptions(void) {
    char path[512];
    make_path(path, sizeof(path), "options.ini");

    FILE* f = fopen(path, "r");
    struct _SAVE_W* sw = CurrentSave();

    if (!f) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] No options.ini found — using defaults");
    } else {

        /* Controller */
        ini_read_bytes(f, "pad_1p_buttons", sw->Pad_Infor[0].Shot, 8);
        sw->Pad_Infor[0].Vibration = (u8)ini_read_int(f, "pad_1p_vibration", 0);
        ini_read_bytes(f, "pad_2p_buttons", sw->Pad_Infor[1].Shot, 8);
        sw->Pad_Infor[1].Vibration = (u8)ini_read_int(f, "pad_2p_vibration", 0);

        /* Game settings */
        sw->Difficulty = (u8)ini_read_int(f, "difficulty", sw->Difficulty);
        sw->Time_Limit = (s8)ini_read_int(f, "time_limit", sw->Time_Limit);
        sw->Battle_Number[0] = (u8)ini_read_int(f, "battle_number_1", sw->Battle_Number[0]);
        sw->Battle_Number[1] = (u8)ini_read_int(f, "battle_number_2", sw->Battle_Number[1]);
        sw->Damage_Level = (u8)ini_read_int(f, "damage_level", sw->Damage_Level);
        sw->Handicap = (u8)ini_read_int(f, "handicap", sw->Handicap);
        sw->Partner_Type[0] = (u8)ini_read_int(f, "partner_type_1p", sw->Partner_Type[0]);
        sw->Partner_Type[1] = (u8)ini_read_int(f, "partner_type_2p", sw->Partner_Type[1]);

        /* Display */
        sw->Adjust_X = (s8)ini_read_int(f, "adjust_x", sw->Adjust_X);
        sw->Adjust_Y = (s8)ini_read_int(f, "adjust_y", sw->Adjust_Y);
        sw->Screen_Size = (u8)ini_read_int(f, "screen_size", sw->Screen_Size);
        sw->Screen_Mode = (u8)ini_read_int(f, "screen_mode", sw->Screen_Mode);

        /* Gameplay */
        sw->GuardCheck = (u8)ini_read_int(f, "guard_check", sw->GuardCheck);
        sw->Auto_Save = (u8)ini_read_int(f, "auto_save", sw->Auto_Save);
        sw->AnalogStick = (u8)ini_read_int(f, "analog_stick", sw->AnalogStick);
        sw->Unlock_All = (u8)ini_read_int(f, "unlock_all", 1);

        /* Sound */
        sw->BgmType = (u8)ini_read_int(f, "bgm_type", sw->BgmType);
        sw->SoundMode = (u8)ini_read_int(f, "sound_mode", sw->SoundMode);
        sw->BGM_Level = (u8)ini_read_int(f, "bgm_level", sw->BGM_Level);
        sw->SE_Level = (u8)ini_read_int(f, "se_level", sw->SE_Level);

        /* Extra */
        sw->Extra_Option = (u8)ini_read_int(f, "extra_option", sw->Extra_Option);

        /* Player colors — 2 players × 20 characters */
        ini_read_bytes(f, "pl_color_1p", sw->PL_Color[0], 20);
        ini_read_bytes(f, "pl_color_2p", sw->PL_Color[1], 20);

        /* Extra option contents — 4 pages × 8 entries */
        for (int page = 0; page < 4; page++) {
            char key[64];
            snprintf(key, sizeof(key), "extra_option_page_%d", page);
            ini_read_s8_array(f, key, sw->extra_option.contents[page], 8);
        }

        /* Broadcast config */
        sw->broadcast_config.enabled = (bool)ini_read_int(f, "broadcast_enabled", 0);
        sw->broadcast_config.source = (BroadcastSource)ini_read_int(f, "broadcast_source", 0);
        sw->broadcast_config.show_ui = (bool)ini_read_int(f, "broadcast_show_ui", 0);

        /* Rankings — 20 entries, stored as binary blob (too complex for INI) */
        /* We read them as comma-separated bytes if they exist */
        for (int i = 0; i < 20; i++) {
            char key[64];
            snprintf(key, sizeof(key), "ranking_%02d", i);
            ini_read_bytes(f, key, (u8*)&sw->Ranking[i], sizeof(RANK_DATA));
        }

        fclose(f);
    }

    /* Apply loaded settings to game state (mirrors load_data_set_system) */
    memcpy(&save_w[4], sw, sizeof(struct _SAVE_W));
    memcpy(&save_w[5], sw, sizeof(struct _SAVE_W));

    sys_w.bgm_type = sw->BgmType;
    sys_w.sound_mode = sw->SoundMode;
    bgm_level = sw->BGM_Level;
    se_level = sw->SE_Level;

    setupSoundMode();
    SsBgmHalfVolume(0);
    setSeVolume(se_level);

    Copy_Save_w();
    Copy_Check_w();

    g_state.X_Adjust = sw->Adjust_X;
    g_state.Y_Adjust = sw->Adjust_Y;

    {
        u8 dw, dh;
        dspwhUnpack(sw->Screen_Size, &dw, &dh);
        Disp_Size_H = dw;
        Disp_Size_V = dh;
    }
    sys_w.screen_mode = sw->Screen_Mode;

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] Options loaded from %s", path);
    return 0;
}

/** @brief Save current options from CurrentSave() to options.ini. */
void NativeSave_SaveOptions(void) {
    char path[512], tmp[520];
    make_path(path, sizeof(path), "options.ini");

    Save_Game_Data(); /* Sync game state → save_w */

    FILE* f = atomic_open(path, tmp, sizeof(tmp));
    if (!f) {
        SDL_Log("[NativeSave] ERROR: Cannot create %s: %s", tmp, strerror(errno));
        return;
    }

    struct _SAVE_W* sw = CurrentSave();

    fprintf(f, "# 3SX Options — auto-generated, hand-editable\n");
    fprintf(f, "# DO NOT change key names. Values are integers unless noted.\n");
    fprintf(f, "# See docs/CONFIG_REFERENCE.md for full documentation.\n\n");

    fprintf(f, "[Controller]\n");
    fprintf(f, "# Button mapping: 8 values = LP,MP,HP,unused,LK,MK,HK,unused  (0-11 = button index)\n");
    write_bytes(f, "pad_1p_buttons", sw->Pad_Infor[0].Shot, 8);
    fprintf(f, "# Vibration: 0=off, 1=on\n");
    fprintf(f, "pad_1p_vibration=%d\n", sw->Pad_Infor[0].Vibration);
    write_bytes(f, "pad_2p_buttons", sw->Pad_Infor[1].Shot, 8);
    fprintf(f, "pad_2p_vibration=%d\n\n", sw->Pad_Infor[1].Vibration);

    fprintf(f, "[Game]\n");
    fprintf(f, "# difficulty: 1-8 (1=easiest, 8=hardest)\n");
    fprintf(f, "difficulty=%d\n", sw->Difficulty);
    fprintf(f, "# time_limit: -1=infinite, 0-99 seconds\n");
    fprintf(f, "time_limit=%d\n", sw->Time_Limit);
    fprintf(f, "# battle_number: 1-9 rounds per match\n");
    fprintf(f, "battle_number_1=%d\n", sw->Battle_Number[0]);
    fprintf(f, "battle_number_2=%d\n", sw->Battle_Number[1]);
    fprintf(f, "# damage_level: 0-3 (0=lowest, 3=highest)\n");
    fprintf(f, "damage_level=%d\n", sw->Damage_Level);
    fprintf(f, "# handicap: 0=off, 1=on\n");
    fprintf(f, "handicap=%d\n", sw->Handicap);
    fprintf(f, "# partner_type: 0=Normal, 1=CPU\n");
    fprintf(f, "partner_type_1p=%d\n", sw->Partner_Type[0]);
    fprintf(f, "partner_type_2p=%d\n\n", sw->Partner_Type[1]);

    fprintf(f, "[Display]\n");
    fprintf(f, "# adjust_x/y: screen position offset in pixels\n");
    fprintf(f, "adjust_x=%d\n", sw->Adjust_X);
    fprintf(f, "adjust_y=%d\n", sw->Adjust_Y);
    fprintf(f, "# screen_size: packed display size (internal)\n");
    fprintf(f, "screen_size=%d\n", sw->Screen_Size);
    fprintf(f, "# screen_mode: 0=Normal, 1=Flipped\n");
    fprintf(f, "screen_mode=%d\n\n", sw->Screen_Mode);

    fprintf(f, "[Gameplay]\n");
    fprintf(f, "# guard_check: 0=off, 1=on (guard indicator)\n");
    fprintf(f, "guard_check=%d\n", sw->GuardCheck);
    fprintf(f, "# auto_save: 0=off, 1=on\n");
    fprintf(f, "auto_save=%d\n", sw->Auto_Save);
    fprintf(f, "# analog_stick: 0=off, 1=on\n");
    fprintf(f, "analog_stick=%d\n", sw->AnalogStick);
    fprintf(f, "# unlock_all: 0=locked, 1=all characters unlocked\n");
    fprintf(f, "unlock_all=%d\n\n", sw->Unlock_All);

    fprintf(f, "[Sound]\n");
    fprintf(f, "# bgm_type: 0=Original, 1=Arranged\n");
    fprintf(f, "bgm_type=%d\n", sw->BgmType);
    fprintf(f, "# sound_mode: 0=Stereo, 1=Mono\n");
    fprintf(f, "sound_mode=%d\n", sw->SoundMode);
    fprintf(f, "# bgm_level/se_level: 0-15 volume level\n");
    fprintf(f, "bgm_level=%d\n", sw->BGM_Level);
    fprintf(f, "se_level=%d\n\n", sw->SE_Level);

    fprintf(f, "[Extra]\n");
    fprintf(f, "# extra_option: 0=off, 1=on (enables extra option pages)\n");
    fprintf(f, "extra_option=%d\n", sw->Extra_Option);
    fprintf(f, "# pl_color: per-character color index (20 characters, 0=default)\n");
    write_bytes(f, "pl_color_1p", sw->PL_Color[0], 20);
    write_bytes(f, "pl_color_2p", sw->PL_Color[1], 20);
    for (int page = 0; page < 4; page++) {
        char key[64];
        snprintf(key, sizeof(key), "extra_option_page_%d", page);
        write_s8_array(f, key, sw->extra_option.contents[page], 8);
    }
    fprintf(f, "\n");

    fprintf(f, "\n[Broadcast]\n");
    fprintf(f, "# broadcast_enabled: 0=off, 1=on\n");
    fprintf(f, "broadcast_enabled=%d\n", sw->broadcast_config.enabled);
    fprintf(f, "# broadcast_source: 0=Game, 1=Window\n");
    fprintf(f, "broadcast_source=%d\n", sw->broadcast_config.source);
    fprintf(f, "# broadcast_show_ui: 0=hide, 1=show overlay\n");
    fprintf(f, "broadcast_show_ui=%d\n\n", sw->broadcast_config.show_ui);

    fprintf(f, "[Rankings]\n");
    fprintf(f, "# Binary ranking data — do not edit manually\n");
    for (int i = 0; i < 20; i++) {
        char key[64];
        snprintf(key, sizeof(key), "ranking_%02d", i);
        write_bytes(f, key, (const u8*)&sw->Ranking[i], sizeof(RANK_DATA));
    }

    fclose(f);
    atomic_commit(path, tmp);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] Options saved to %s", path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  DIRECTION  —  INI format
 * ═══════════════════════════════════════════════════════════════════ */

/** @brief Load direction config from direction.ini. */
int NativeSave_LoadDirection(void) {
    char path[512];
    make_path(path, sizeof(path), "direction.ini");

    FILE* f = fopen(path, "r");
    SystemDir* sd = &system_dir[g_state.Present_Mode];
    s32 page;

    if (!f) {
        SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] No direction.ini found — using defaults");
    } else {
        for (int p = 0; p < 10; p++) {
            char key[64];
            snprintf(key, sizeof(key), "page_%d", p);
            ini_read_s8_array(f, key, sd->contents[p], 7);
        }

        fclose(f);
    }

    /* Clamp pages beyond what the current unlock level supports */
    page = Check_SysDir_Page();
    if (page < 9) {
        page += 1;
        for (; page < 10; page++) {
            for (int i = 0; i < 7; i++) {
                system_dir[g_state.Present_Mode].contents[page][i] = system_dir->contents[page][i];
            }
        }
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] Direction loaded from %s", path);
    return 0;
}

/** @brief Save current direction config to direction.ini. */
void NativeSave_SaveDirection(void) {
    char path[512], tmp[520];
    make_path(path, sizeof(path), "direction.ini");

    FILE* f = atomic_open(path, tmp, sizeof(tmp));
    if (!f) {
        SDL_Log("[NativeSave] ERROR: Cannot create %s: %s", tmp, strerror(errno));
        return;
    }

    SystemDir* sd = &system_dir[g_state.Present_Mode];

    fprintf(f, "# 3SX Direction Config — auto-generated\n");
    fprintf(f, "# Each page has 7 values: per-character direction dipswitch settings\n");
    fprintf(f, "# Values: 0=default, 1=right, 2=left\n\n");

    for (int p = 0; p < 10; p++) {
        char key[64];
        snprintf(key, sizeof(key), "page_%d", p);
        write_s8_array(f, key, sd->contents[p], 7);
    }

    fclose(f);
    atomic_commit(path, tmp);
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[NativeSave] Direction saved to %s", path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  REPLAY  —  binary format
 * ═══════════════════════════════════════════════════════════════════ */

/** Internal header for native replay files */
typedef struct {
    u32 magic;     /* 0x33535852 = "3SXR" */
    u32 version;   /* 2 */
    u32 data_size; /* sizeof(_REPLAY_W) */
    u32 reserved;
    /* Added in version 2 */
    u8 player1_char;
    u8 player2_char;
    u8 winner; /* 0=P1, 1=P2, 2=Draw */
    u8 stage;
    u32 date_timestamp; /* time_t truncated to u32 */
} NativeReplayHeader;

#define NATIVE_REPLAY_MAGIC 0x33535852 /* "3SXR" */
#define NATIVE_REPLAY_VERSION 2

static void get_current_date(memcard_date* md) {
    time_t rawtime;
    struct tm* t;

    time(&rawtime);
    t = localtime(&rawtime);
    if (t) {
        md->sec = (u8)t->tm_sec;
        md->min = (u8)t->tm_min;
        md->hour = (u8)t->tm_hour;
        md->day = (u8)t->tm_mday;
        md->month = (u8)(t->tm_mon + 1);
        md->year = (u16)(t->tm_year + 1900);
        md->dayofweek = (u8)t->tm_wday;
    } else {
        memset(md, 0, sizeof(*md));
        md->day = 1;
        md->month = 1;
        md->year = 2000;
    }
}

/** @brief Check if a replay file exists. Returns 1 if exists, 0 if empty. */
int NativeSave_ReplayExists(const char* filename) {
    if (!filename || !filename[0])
        return 0;

    char path[512];
    make_replay_path(path, sizeof(path), filename, ".bin");

    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

/** @brief Get metadata for a replay file without loading full data. */
int NativeSave_GetReplayInfo(const char* filename, _sub_info* out) {
    if (!filename || !filename[0] || !out)
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), filename, ".meta");

    FILE* f = fopen(path, "rb");
    if (!f)
        return -1;

    size_t read = fread(out, 1, sizeof(_sub_info), f);
    fclose(f);

    return (read == sizeof(_sub_info)) ? 0 : -1;
}

/** @brief Get winner from replay header. Returns 0 (P1), 1 (P2), 2 (Draw), or -1 (Unknown/Error). */
int NativeSave_GetReplayWinner(const char* filename) {
    if (!filename || !filename[0])
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), filename, ".bin");

    FILE* f = fopen(path, "rb");
    if (!f)
        return -1;

    NativeReplayHeader hdr;
    if (fread(&hdr, 1, 16, f) != 16) {
        fclose(f);
        return -1;
    }
    if (hdr.magic != NATIVE_REPLAY_MAGIC || hdr.version < 2) {
        fclose(f);
        return -1;
    }

    size_t v2_remainder = sizeof(NativeReplayHeader) - 16;
    if (fread((uint8_t*)&hdr + 16, 1, v2_remainder, f) != v2_remainder) {
        fclose(f);
        return -1;
    }

    fclose(f);
    return hdr.winner;
}

/** @brief Load replay data from the given file into Replay_w. */
int NativeSave_LoadReplay(const char* filename) {
    if (!filename || !filename[0])
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), filename, ".bin");

    FILE* f = fopen(path, "rb");
    if (!f) {
        SDL_Log("[NativeSave] Replay %s not found", filename);
        return -1;
    }

    /* Read and validate header — v1 backward compatible */
    NativeReplayHeader hdr;
    memset(&hdr, 0, sizeof(hdr));

    /* First read the common 16 bytes (v1 header) */
    size_t v1_size = 16; /* magic (4) + version (4) + data_size (4) + reserved (4) */
    if (fread(&hdr, 1, v1_size, f) != v1_size) {
        SDL_Log("[NativeSave] Replay %s: header read error (v1)", filename);
        fclose(f);
        return -2;
    }

    /* If version >= 2, read the rest of the header */
    if (hdr.version >= 2) {
        size_t v2_remainder = sizeof(NativeReplayHeader) - v1_size;
        if (fread((uint8_t*)&hdr + v1_size, 1, v2_remainder, f) != v2_remainder) {
            SDL_Log("[NativeSave] Replay %s: header read error (v2 payload)", filename);
            fclose(f);
            return -2;
        }
    }

    if (hdr.magic != NATIVE_REPLAY_MAGIC) {
        SDL_Log("[NativeSave] Replay %s: bad magic 0x%08X", filename, hdr.magic);
        fclose(f);
        return -2;
    }

    if (hdr.data_size != sizeof(_REPLAY_W)) {
        SDL_Log("[NativeSave] Replay %s: size mismatch (file=%u, expected=%zu)",
                filename,
                hdr.data_size,
                sizeof(_REPLAY_W));
        /* Still try to load — forward compat */
    }

    /* Read replay data */
    size_t to_read = (hdr.data_size < sizeof(_REPLAY_W)) ? hdr.data_size : sizeof(_REPLAY_W);
    memset(&Replay_w, 0, sizeof(_REPLAY_W));
    size_t got = fread(&Replay_w, 1, to_read, f);
    fclose(f);

    if (got < to_read) {
        SDL_Log("[NativeSave] Replay %s: short read (%zu/%zu)", filename, got, to_read);
        return -2;
    }

    SDL_Log("[NativeSave] Replay %s loaded from %s", filename, path);
    return 0;
}

/** @brief Save current replay data to the given filename. */
int NativeSave_SaveReplay(const char* filename) {
    if (!filename || !filename[0])
        return -1;

    char bin_path[512], bin_tmp[520];
    char meta_path[512], meta_tmp[520];
    make_replay_path(bin_path, sizeof(bin_path), filename, ".bin");
    make_replay_path(meta_path, sizeof(meta_path), filename, ".meta");

    /* Prepare replay data — copy from game state (mirrors save_data_store_replay) */
    _REPLAY_W* rw = &Replay_w;
    struct _SAVE_W* sw = CurrentSave();
    struct _REP_GAME_INFOR* rp = &Rep_Game_Infor[10];

    memcpy(&rw->game_infor, rp, sizeof(*rp));
    rw->mini_save_w.Pad_Infor[0] = sw->Pad_Infor[0];
    rw->mini_save_w.Pad_Infor[1] = sw->Pad_Infor[1];
    rw->mini_save_w.Time_Limit = sw->Time_Limit;
    rw->mini_save_w.Battle_Number[0] = sw->Battle_Number[0];
    rw->mini_save_w.Battle_Number[1] = sw->Battle_Number[1];
    rw->mini_save_w.Damage_Level = sw->Damage_Level;
    memcpy(&rw->mini_save_w.extra_option, &sw->extra_option, sizeof(sw->extra_option));
    memcpy(&rw->system_dir, &system_dir[g_state.Present_Mode], sizeof(rw->system_dir));

    /* Write binary data */
    FILE* f = atomic_open_bin(bin_path, bin_tmp, sizeof(bin_tmp));
    if (!f) {
        SDL_Log("[NativeSave] ERROR: Cannot create %s: %s", bin_tmp, strerror(errno));
        return -1;
    }

    NativeReplayHeader hdr;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = NATIVE_REPLAY_MAGIC;
    hdr.version = NATIVE_REPLAY_VERSION;
    hdr.data_size = sizeof(_REPLAY_W);
    hdr.player1_char = rp->player_infor[0].my_char;
    hdr.player2_char = rp->player_infor[1].my_char;
    hdr.winner = (rw->game_infor.winner == 0) ? 0 : ((rw->game_infor.winner == 1) ? 1 : 2);
    hdr.stage = rw->game_infor.stage;
    hdr.date_timestamp = (u32)time(NULL);

    fwrite(&hdr, 1, sizeof(hdr), f);
    fwrite(rw, 1, sizeof(_REPLAY_W), f);
    fclose(f);
    atomic_commit(bin_path, bin_tmp);

    /* Write metadata sidecar */
    _sub_info meta;
    get_current_date(&meta.date);
    meta.player[0] = rp->player_infor[0].my_char;
    meta.player[1] = rp->player_infor[1].my_char;

    f = atomic_open_bin(meta_path, meta_tmp, sizeof(meta_tmp));
    if (f) {
        fwrite(&meta, 1, sizeof(meta), f);
        fclose(f);
        atomic_commit(meta_path, meta_tmp);
    }

    SDL_Log("[NativeSave] Replay %s saved to %s", filename, bin_path);
    return 0;
}

typedef struct {
    char (*out_filenames)[128];
    int max_count;
    int current_count;
    int is_netplay;
} EnumReplayCtx;

static SDL_EnumerationResult enum_replay_cb(void* userdata, const char* dirname, const char* fname) {
    EnumReplayCtx* ctx = (EnumReplayCtx*)userdata;
    const char* ext = strrchr(fname, '.');
    if (ext && strcmp(ext, ".meta") == 0) {
        /* Determine if it's netplay based on a '_net_' substring.
         * Legacy slots >= 10000 were netplay, so we check for 'replay_XXXXX.meta'. */
        int is_netplay = 0;
        int legacy_slot = -1;
        if (sscanf(fname, "replay_%d.meta", &legacy_slot) == 1 && legacy_slot >= NATIVE_SAVE_NETPLAY_BASE) {
            is_netplay = 1;
        } else if (strstr(fname, "_net_") != NULL) {
            is_netplay = 1;
        }

        if (is_netplay == ctx->is_netplay || ctx->is_netplay == -1) {
            if (ctx->current_count < ctx->max_count) {
                size_t len = ext - fname;
                if (len >= 128)
                    len = 127;
                memcpy(ctx->out_filenames[ctx->current_count], fname, len);
                ctx->out_filenames[ctx->current_count][len] = '\0';
                ctx->current_count++;
            }
        }
    }
    return SDL_ENUM_CONTINUE;
}

static int replay_filename_cmp_desc(const void* a, const void* b) {
    return strcmp((const char*)b, (const char*)a);
}

int NativeSave_FindAllReplays(char out_filenames[][128], int max_count, int is_netplay) {
    EnumReplayCtx ctx = { out_filenames, max_count, 0, is_netplay };
    char path[512];
    ensure_save_dir();
    snprintf(path, sizeof(path), "%sreplays", save_dir);

    SDL_EnumerateDirectory(path, enum_replay_cb, &ctx);

    /* Sort descending (newest first based on string comparison) */
    if (ctx.current_count > 1) {
        qsort(out_filenames, ctx.current_count, 128, replay_filename_cmp_desc);
    }
    return ctx.current_count;
}

/* Character name table for auto-save filenames */
#include "character_names.h"

static const char* get_char_name_for_filename(int my_char_id) {
    return character_get_name(my_char_id);
}

/** @brief Auto-save replay to a descriptive filename. */
int NativeSave_AutoSaveReplay(int is_netplay) {
    struct _REP_GAME_INFOR* rp = &Rep_Game_Infor[10];

    int p1_char = rp->player_infor[0].my_char;
    int p2_char = rp->player_infor[1].my_char;

    const char* p1_str = get_char_name_for_filename(p1_char);
    const char* p2_str = get_char_name_for_filename(p2_char);

    int winner = rp->winner;
    const char* result_str = (winner == 0) ? "P1" : ((winner == 1) ? "P2" : "D");

    time_t rawtime;
    struct tm* t;
    time(&rawtime);
    t = localtime(&rawtime);

    char filename[128];
    snprintf(filename,
             sizeof(filename),
             "%04d-%02d-%02d_%02d-%02d-%02d_%s_%s-vs-%s_W-%s",
             t->tm_year + 1900,
             t->tm_mon + 1,
             t->tm_mday,
             t->tm_hour,
             t->tm_min,
             t->tm_sec,
             is_netplay ? "net" : "loc",
             p1_str,
             p2_str,
             result_str);

    SDL_Log("[NativeSave] Auto-saving replay to %s", filename);

    return NativeSave_SaveReplay(filename);
}

/** @brief Delete a replay file (removes both .bin and .meta files). */
int NativeSave_DeleteReplay(const char* filename) {
    if (!filename || !filename[0])
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), filename, ".bin");
    remove(path);
    make_replay_path(path, sizeof(path), filename, ".meta");
    remove(path);

    SDL_Log("[NativeSave] Replay %s deleted", filename);
    return 0;
}
