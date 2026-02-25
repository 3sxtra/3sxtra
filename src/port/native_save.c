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

#include "port/native_save.h"
#include "common.h"

#include "sf33rd/Source/Game/engine/workuser.h"
#include "sf33rd/Source/Game/menu/dir_data.h"
#include "sf33rd/Source/Game/sound/sound3rd.h"
#include "sf33rd/Source/Game/system/sys_sub.h"
#include "sf33rd/Source/Game/system/sys_sub2.h"
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

static void make_replay_path(char* dst, size_t dst_size, int slot, const char* ext) {
    ensure_save_dir();
    ensure_replay_dir();
    snprintf(dst, dst_size, "%sreplays/replay_%02d%s", save_dir, slot, ext);
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
    char found_key[128];
    int val;

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        if (sscanf(line, "%127[^=]=%d", found_key, &val) == 2) {
            ini_trim(found_key);
            if (strcmp(found_key, key) == 0)
                return val;
        }
    }
    return def;
}

static void ini_read_bytes(FILE* f, const char* key, u8* dst, int count) {
    char line[1024];
    char found_key[128];
    char values[512];

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        if (sscanf(line, "%127[^=]=%511[^\n]", found_key, values) == 2) {
            ini_trim(found_key);
            if (strcmp(found_key, key) == 0) {
                char* p = values;
                for (int i = 0; i < count && *p; i++) {
                    dst[i] = (u8)strtol(p, &p, 10);
                    if (*p == ',')
                        p++;
                }
                return;
            }
        }
    }
}

static void ini_read_s8_array(FILE* f, const char* key, s8* dst, int count) {
    char line[1024];
    char found_key[128];
    char values[512];

    rewind(f);
    while (fgets(line, sizeof(line), f)) {
        ini_trim(line);
        if (line[0] == '#' || line[0] == '[' || line[0] == '\0')
            continue;
        if (sscanf(line, "%127[^=]=%511[^\n]", found_key, values) == 2) {
            ini_trim(found_key);
            if (strcmp(found_key, key) == 0) {
                char* p = values;
                for (int i = 0; i < count && *p; i++) {
                    dst[i] = (s8)strtol(p, &p, 10);
                    if (*p == ',')
                        p++;
                }
                return;
            }
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
        printf("[NativeSave] ERROR: rename %s -> %s failed: %s\n", tmp_path, path, strerror(errno));
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

void NativeSave_Init(void) {
    ensure_save_dir();
    printf("[NativeSave] Save directory: %s\n", save_dir);
}

const char* NativeSave_GetSavePath(void) {
    ensure_save_dir();
    return save_dir;
}

/* ═══════════════════════════════════════════════════════════════════
 *  OPTIONS  —  INI format
 * ═══════════════════════════════════════════════════════════════════ */

int NativeSave_LoadOptions(void) {
    char path[512];
    make_path(path, sizeof(path), "options.ini");

    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[NativeSave] No options.ini found — using defaults\n");
        return -1;
    }

    struct _SAVE_W* sw = &save_w[Present_Mode];

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

    /* Apply loaded settings to game state (mirrors load_data_set_system) */
    memcpy(&save_w[4], sw, sizeof(struct _SAVE_W));
    memcpy(&save_w[5], sw, sizeof(struct _SAVE_W));

    sys_w.bgm_type = sw->BgmType;
    sys_w.sound_mode = sw->SoundMode;
    bgm_level = sw->BGM_Level;
    se_level = sw->SE_Level;

    setupSoundMode();
    SsBgmHalfVolume(0);
    setSeVolume();

    Copy_Save_w();
    Copy_Check_w();

    X_Adjust = sw->Adjust_X;
    Y_Adjust = sw->Adjust_Y;

    {
        u8 dw, dh;
        dspwhUnpack(sw->Screen_Size, &dw, &dh);
        Disp_Size_H = dw;
        Disp_Size_V = dh;
    }
    sys_w.screen_mode = sw->Screen_Mode;

    printf("[NativeSave] Options loaded from %s\n", path);
    return 0;
}

void NativeSave_SaveOptions(void) {
    char path[512], tmp[520];
    make_path(path, sizeof(path), "options.ini");

    Save_Game_Data(); /* Sync game state → save_w */

    FILE* f = atomic_open(path, tmp, sizeof(tmp));
    if (!f) {
        printf("[NativeSave] ERROR: Cannot create %s: %s\n", tmp, strerror(errno));
        return;
    }

    struct _SAVE_W* sw = &save_w[Present_Mode];

    fprintf(f, "# 3SX Options — auto-generated, hand-editable\n");
    fprintf(f, "# DO NOT change key names. Values are integers.\n\n");

    fprintf(f, "[Controller]\n");
    write_bytes(f, "pad_1p_buttons", sw->Pad_Infor[0].Shot, 8);
    fprintf(f, "pad_1p_vibration=%d\n", sw->Pad_Infor[0].Vibration);
    write_bytes(f, "pad_2p_buttons", sw->Pad_Infor[1].Shot, 8);
    fprintf(f, "pad_2p_vibration=%d\n\n", sw->Pad_Infor[1].Vibration);

    fprintf(f, "[Game]\n");
    fprintf(f, "difficulty=%d\n", sw->Difficulty);
    fprintf(f, "time_limit=%d\n", sw->Time_Limit);
    fprintf(f, "battle_number_1=%d\n", sw->Battle_Number[0]);
    fprintf(f, "battle_number_2=%d\n", sw->Battle_Number[1]);
    fprintf(f, "damage_level=%d\n", sw->Damage_Level);
    fprintf(f, "handicap=%d\n", sw->Handicap);
    fprintf(f, "partner_type_1p=%d\n", sw->Partner_Type[0]);
    fprintf(f, "partner_type_2p=%d\n\n", sw->Partner_Type[1]);

    fprintf(f, "[Display]\n");
    fprintf(f, "adjust_x=%d\n", sw->Adjust_X);
    fprintf(f, "adjust_y=%d\n", sw->Adjust_Y);
    fprintf(f, "screen_size=%d\n", sw->Screen_Size);
    fprintf(f, "screen_mode=%d\n\n", sw->Screen_Mode);

    fprintf(f, "[Gameplay]\n");
    fprintf(f, "guard_check=%d\n", sw->GuardCheck);
    fprintf(f, "auto_save=%d\n", sw->Auto_Save);
    fprintf(f, "analog_stick=%d\n", sw->AnalogStick);
    fprintf(f, "unlock_all=%d\n\n", sw->Unlock_All);

    fprintf(f, "[Sound]\n");
    fprintf(f, "bgm_type=%d\n", sw->BgmType);
    fprintf(f, "sound_mode=%d\n", sw->SoundMode);
    fprintf(f, "bgm_level=%d\n", sw->BGM_Level);
    fprintf(f, "se_level=%d\n\n", sw->SE_Level);

    fprintf(f, "[Extra]\n");
    fprintf(f, "extra_option=%d\n", sw->Extra_Option);
    write_bytes(f, "pl_color_1p", sw->PL_Color[0], 20);
    write_bytes(f, "pl_color_2p", sw->PL_Color[1], 20);
    for (int page = 0; page < 4; page++) {
        char key[64];
        snprintf(key, sizeof(key), "extra_option_page_%d", page);
        write_s8_array(f, key, sw->extra_option.contents[page], 8);
    }
    fprintf(f, "\n");

    fprintf(f, "[Broadcast]\n");
    fprintf(f, "broadcast_enabled=%d\n", sw->broadcast_config.enabled);
    fprintf(f, "broadcast_source=%d\n", sw->broadcast_config.source);
    fprintf(f, "broadcast_show_ui=%d\n\n", sw->broadcast_config.show_ui);

    fprintf(f, "[Rankings]\n");
    for (int i = 0; i < 20; i++) {
        char key[64];
        snprintf(key, sizeof(key), "ranking_%02d", i);
        write_bytes(f, key, (const u8*)&sw->Ranking[i], sizeof(RANK_DATA));
    }

    fclose(f);
    atomic_commit(path, tmp);
    printf("[NativeSave] Options saved to %s\n", path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  DIRECTION  —  INI format
 * ═══════════════════════════════════════════════════════════════════ */

int NativeSave_LoadDirection(void) {
    char path[512];
    make_path(path, sizeof(path), "direction.ini");

    FILE* f = fopen(path, "r");
    if (!f) {
        printf("[NativeSave] No direction.ini found — using defaults\n");
        return -1;
    }

    SystemDir* sd = &system_dir[Present_Mode];
    s32 page;

    for (int p = 0; p < 10; p++) {
        char key[64];
        snprintf(key, sizeof(key), "page_%d", p);
        ini_read_s8_array(f, key, sd->contents[p], 7);
    }

    fclose(f);

    /* Clamp pages beyond what the current unlock level supports */
    page = Check_SysDir_Page();
    if (page < 9) {
        page += 1;
        for (; page < 10; page++) {
            for (int i = 0; i < 7; i++) {
                system_dir[Present_Mode].contents[page][i] = system_dir->contents[page][i];
            }
        }
    }

    printf("[NativeSave] Direction loaded from %s\n", path);
    return 0;
}

void NativeSave_SaveDirection(void) {
    char path[512], tmp[520];
    make_path(path, sizeof(path), "direction.ini");

    FILE* f = atomic_open(path, tmp, sizeof(tmp));
    if (!f) {
        printf("[NativeSave] ERROR: Cannot create %s: %s\n", tmp, strerror(errno));
        return;
    }

    SystemDir* sd = &system_dir[Present_Mode];

    fprintf(f, "# 3SX Direction Config — auto-generated\n");
    fprintf(f, "# Each page has 7 values (dipswitch settings per character page)\n\n");

    for (int p = 0; p < 10; p++) {
        char key[64];
        snprintf(key, sizeof(key), "page_%d", p);
        write_s8_array(f, key, sd->contents[p], 7);
    }

    fclose(f);
    atomic_commit(path, tmp);
    printf("[NativeSave] Direction saved to %s\n", path);
}

/* ═══════════════════════════════════════════════════════════════════
 *  REPLAY  —  binary format
 * ═══════════════════════════════════════════════════════════════════ */

/** Internal header for native replay files */
typedef struct {
    u32 magic;     /* 0x33535852 = "3SXR" */
    u32 version;   /* 1 */
    u32 data_size; /* sizeof(_REPLAY_W) */
    u32 reserved;
} NativeReplayHeader;

#define NATIVE_REPLAY_MAGIC 0x33535852 /* "3SXR" */
#define NATIVE_REPLAY_VERSION 1

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

int NativeSave_ReplayExists(int slot) {
    if (slot < 0 || slot >= NATIVE_SAVE_REPLAY_SLOTS)
        return 0;

    char path[512];
    make_replay_path(path, sizeof(path), slot, ".bin");

    FILE* f = fopen(path, "rb");
    if (!f)
        return 0;
    fclose(f);
    return 1;
}

int NativeSave_GetReplayInfo(int slot, _sub_info* out) {
    if (slot < 0 || slot >= NATIVE_SAVE_REPLAY_SLOTS || !out)
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), slot, ".meta");

    FILE* f = fopen(path, "rb");
    if (!f)
        return -1;

    size_t read = fread(out, 1, sizeof(_sub_info), f);
    fclose(f);

    return (read == sizeof(_sub_info)) ? 0 : -1;
}

int NativeSave_LoadReplay(int slot) {
    if (slot < 0 || slot >= NATIVE_SAVE_REPLAY_SLOTS)
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), slot, ".bin");

    FILE* f = fopen(path, "rb");
    if (!f) {
        printf("[NativeSave] Replay %d not found\n", slot);
        return -1;
    }

    /* Read and validate header */
    NativeReplayHeader hdr;
    if (fread(&hdr, 1, sizeof(hdr), f) != sizeof(hdr)) {
        printf("[NativeSave] Replay %d: header read error\n", slot);
        fclose(f);
        return -2;
    }

    if (hdr.magic != NATIVE_REPLAY_MAGIC) {
        printf("[NativeSave] Replay %d: bad magic 0x%08X\n", slot, hdr.magic);
        fclose(f);
        return -2;
    }

    if (hdr.data_size != sizeof(_REPLAY_W)) {
        printf(
            "[NativeSave] Replay %d: size mismatch (file=%u, expected=%zu)\n", slot, hdr.data_size, sizeof(_REPLAY_W));
        /* Still try to load — forward compat */
    }

    /* Read replay data */
    size_t to_read = (hdr.data_size < sizeof(_REPLAY_W)) ? hdr.data_size : sizeof(_REPLAY_W);
    memset(&Replay_w, 0, sizeof(_REPLAY_W));
    size_t got = fread(&Replay_w, 1, to_read, f);
    fclose(f);

    if (got < to_read) {
        printf("[NativeSave] Replay %d: short read (%zu/%zu)\n", slot, got, to_read);
        return -2;
    }

    printf("[NativeSave] Replay %d loaded from %s\n", slot, path);
    return 0;
}

int NativeSave_SaveReplay(int slot) {
    if (slot < 0 || slot >= NATIVE_SAVE_REPLAY_SLOTS)
        return -1;

    char bin_path[512], bin_tmp[520];
    char meta_path[512], meta_tmp[520];
    make_replay_path(bin_path, sizeof(bin_path), slot, ".bin");
    make_replay_path(meta_path, sizeof(meta_path), slot, ".meta");

    /* Prepare replay data — copy from game state (mirrors save_data_store_replay) */
    _REPLAY_W* rw = &Replay_w;
    struct _SAVE_W* sw = &save_w[Present_Mode];
    struct _REP_GAME_INFOR* rp = &Rep_Game_Infor[10];

    memcpy(&rw->game_infor, rp, sizeof(*rp));
    rw->mini_save_w.Pad_Infor[0] = sw->Pad_Infor[0];
    rw->mini_save_w.Pad_Infor[1] = sw->Pad_Infor[1];
    rw->mini_save_w.Time_Limit = sw->Time_Limit;
    rw->mini_save_w.Battle_Number[0] = sw->Battle_Number[0];
    rw->mini_save_w.Battle_Number[1] = sw->Battle_Number[1];
    rw->mini_save_w.Damage_Level = sw->Damage_Level;
    memcpy(&rw->mini_save_w.extra_option, &sw->extra_option, sizeof(sw->extra_option));
    memcpy(&rw->system_dir, &system_dir[Present_Mode], sizeof(rw->system_dir));

    /* Write binary data */
    FILE* f = atomic_open_bin(bin_path, bin_tmp, sizeof(bin_tmp));
    if (!f) {
        printf("[NativeSave] ERROR: Cannot create %s: %s\n", bin_tmp, strerror(errno));
        return -1;
    }

    NativeReplayHeader hdr = {
        .magic = NATIVE_REPLAY_MAGIC, .version = NATIVE_REPLAY_VERSION, .data_size = sizeof(_REPLAY_W), .reserved = 0
    };
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

    printf("[NativeSave] Replay %d saved to %s\n", slot, bin_path);
    return 0;
}

int NativeSave_DeleteReplay(int slot) {
    if (slot < 0 || slot >= NATIVE_SAVE_REPLAY_SLOTS)
        return -1;

    char path[512];
    make_replay_path(path, sizeof(path), slot, ".bin");
    remove(path);
    make_replay_path(path, sizeof(path), slot, ".meta");
    remove(path);

    printf("[NativeSave] Replay %d deleted\n", slot);
    return 0;
}
