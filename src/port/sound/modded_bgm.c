/**
 * @file modded_bgm.c
 * @brief Modded background music and voice line playback via SDL_mixer.
 *
 * Supports:
 * - Drop-in BGM replacement from assets/bgm_mod/{id}.{ogg,flac,opus,mp3,wav}
 * - Loop points via OGG vorbis comment tags (native) or sidecar .loop files
 * - Fade-out transitions
 * - Voice line replacement from assets/voice_mod/{name}.{ext}
 * - Track counting for UI display
 */
#include "port/sound/modded_bgm.h"
#include "port/config/config.h"
#include "port/config/paths.h"
#include "sf33rd/AcrSDK/MiddleWare/PS2/CapSndEng/cse.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "main.h" /* For TASK_INIT, TASK_MENU, task[] */
#include "port/menu_screen.h"
#include "sf33rd/Source/Game/init_task_phases.h"
#include "sf33rd/Source/Game/menu/menu_task_phases.h"

extern MenuScreen g_screens[];
extern u8 Exit_No;
extern s8 VS_Stage;
extern struct _TASK task[];

const char* ModdedBGM_GetGameStateString(void) {
    if (task[TASK_INIT].r_no[0] == ITP_BOOT) {
        return "Boot";
    }

    MenuScreenId screen_id = MenuScreen_GetCurrent();
    if (screen_id != MENU_SCREEN_NONE) {
        if (screen_id == MENU_SCREEN_CHAR_SELECT) {
            if (Exit_No >= 4) {
                return "Versus Screen";
            } else {
                return "Character Select";
            }
        }
        if (g_screens[screen_id].name) {
            return g_screens[screen_id].name;
        }
    }

    if (task[TASK_INIT].r_no[0] == ITP_RUNNING && task[TASK_MENU].r_no[0] == MTP_IN_GAME) {
        static char buf[64];
        SDL_snprintf(buf, sizeof(buf), "In-Game (Stage %d)", VS_Stage + 1);
        return buf;
    }

    return "Transition";
}

static bool is_initialized = false;
static MIX_Mixer* mixer = NULL;
static MIX_Track* music_track = NULL;
static MIX_Track* voice_track = NULL;
static MIX_Audio* current_audio = NULL;
static MIX_Audio* current_voice_audio = NULL;

#define NUM_SFX_TRACKS 16
static MIX_Track* sfx_tracks[NUM_SFX_TRACKS] = { NULL };
static MIX_Audio* sfx_cache[65536] = { NULL };
static bool sfx_attempted[65536] = { false };
static int next_sfx_track = 0;


/* Fade-out state — not needed, MIX_StopTrack has native fade.
 * We track whether a fade is in progress to avoid restarting. */
static bool fade_active = false;

/* Cached track count to avoid per-frame filesystem scans */
static int cached_bgm_count = -1; /* -1 = needs refresh */

void ModdedBGM_Init(void) {
    if (is_initialized)
        return;

    if (!MIX_Init()) {
        SDL_Log("ModdedBGM: MIX_Init failed: %s", SDL_GetError());
        return;
    }

    SDL_AudioSpec spec;
    spec.freq = 48000;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer) {
        SDL_Log("ModdedBGM: MIX_CreateMixerDevice failed: %s", SDL_GetError());
        MIX_Quit();
        return;
    }

    music_track = MIX_CreateTrack(mixer);
    if (!music_track) {
        SDL_Log("ModdedBGM: MIX_CreateTrack (music) failed: %s", SDL_GetError());
        MIX_DestroyMixer(mixer);
        mixer = NULL;
        MIX_Quit();
        return;
    }

    voice_track = MIX_CreateTrack(mixer);
    if (!voice_track) {
        SDL_Log("ModdedBGM: MIX_CreateTrack (voice) failed: %s", SDL_GetError());
        MIX_DestroyTrack(music_track);
        music_track = NULL;
        MIX_DestroyMixer(mixer);
        mixer = NULL;
        MIX_Quit();
        return;
    }

    is_initialized = true;
    current_audio = NULL;
    current_voice_audio = NULL;
    
    for (int i = 0; i < NUM_SFX_TRACKS; i++) {
        sfx_tracks[i] = MIX_CreateTrack(mixer);
        if (!sfx_tracks[i]) {
            SDL_Log("ModdedBGM: MIX_CreateTrack (sfx) failed for track %d: %s", i, SDL_GetError());
        }
    }
    memset(sfx_attempted, 0, sizeof(sfx_attempted));
    memset(sfx_cache, 0, sizeof(sfx_cache));
    next_sfx_track = 0;
    fade_active = false;
    cached_bgm_count = -1;
}

void ModdedBGM_Exit(void) {
    if (!is_initialized)
        return;

    ModdedBGM_Stop();

    if (current_voice_audio) {
        MIX_StopTrack(voice_track, 0);
        MIX_SetTrackAudio(voice_track, NULL);
        MIX_DestroyAudio(current_voice_audio);
        current_voice_audio = NULL;
    }

    for (int i = 0; i < NUM_SFX_TRACKS; i++) {
        if (sfx_tracks[i]) {
            MIX_StopTrack(sfx_tracks[i], 0);
            MIX_SetTrackAudio(sfx_tracks[i], NULL);
            MIX_DestroyTrack(sfx_tracks[i]);
            sfx_tracks[i] = NULL;
        }
    }

    for (int i = 0; i < 65536; i++) {
        if (sfx_cache[i]) {
            MIX_DestroyAudio(sfx_cache[i]);
            sfx_cache[i] = NULL;
        }
        sfx_attempted[i] = false;
    }

    if (voice_track) {
        MIX_DestroyTrack(voice_track);
        voice_track = NULL;
    }
    if (music_track) {
        MIX_DestroyTrack(music_track);
        music_track = NULL;
    }
    if (mixer) {
        MIX_DestroyMixer(mixer);
        mixer = NULL;
    }
    MIX_Quit();
    is_initialized = false;
}

/**
 * @brief Parse a sidecar .loop file for LOOPSTART / LOOPLENGTH values.
 * @return true if both values were successfully parsed.
 */
static bool parse_loop_file(const char* loop_path, Sint64* loop_start, Sint64* loop_length) {
    SDL_IOStream* io = SDL_IOFromFile(loop_path, "r");
    if (!io)
        return false;

    char buf[256];
    *loop_start = -1;
    *loop_length = -1;

    /* Read the entire small sidecar file at once */
    size_t capacity = sizeof(buf) - 1;
    size_t read = SDL_ReadIO(io, buf, capacity);
    buf[read] = '\0';
    SDL_CloseIO(io);

    /* Parse LOOPSTART=N and LOOPLENGTH=N */
    const char* p = buf;
    while (*p) {
        /* Skip whitespace and comments */
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (*p == '#' || *p == ';') {
            while (*p && *p != '\n')
                p++;
            continue;
        }

        if (SDL_strncasecmp(p, "LOOPSTART=", 10) == 0) {
            *loop_start = SDL_strtoll(p + 10, NULL, 10);
        } else if (SDL_strncasecmp(p, "LOOPLENGTH=", 11) == 0) {
            *loop_length = SDL_strtoll(p + 11, NULL, 10);
        }

        /* Advance to next line */
        while (*p && *p != '\n')
            p++;
    }

    if (*loop_start >= 0 && *loop_length > 0) {
        SDL_Log("ModdedBGM: Parsed loop file: start=%lld length=%lld", (long long)*loop_start, (long long)*loop_length);
        return true;
    }

    return false;
}

/**
 * @brief Build a path in assets/{subdir}/{name}.{ext}.
 */
static void build_asset_path(char* out, size_t out_size, const char* subdir, const char* name, const char* ext) {
    char relative[512];
    snprintf(relative, sizeof(relative), "assets/%s/%s.%s", subdir, name, ext);
    const char* resolved = Paths_ResolveAsset(relative);
    snprintf(out, out_size, "%s", resolved);
}

static bool try_load_and_play(const char* ext, int file_id) {
    char path[1024];
    char id_str[32];
    snprintf(id_str, sizeof(id_str), "%d", file_id);
    build_asset_path(path, sizeof(path), "bgm_mod", id_str, ext);

    // Check if file exists by opening via SDL IO
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) {
        return false;
    }
    SDL_CloseIO(io);

    current_audio = MIX_LoadAudio(mixer, path, false);
    if (!current_audio) {
        SDL_Log("ModdedBGM: Found file %s but failed to load: %s", path, SDL_GetError());
        return false;
    }

    MIX_SetTrackAudio(music_track, current_audio);

    // Play with infinite looping
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);

    /* Check for sidecar .loop file (for non-OGG formats without embedded tags) */
    {
        char loop_path[1024];
        snprintf(loop_path,
                 sizeof(loop_path),
                 "%sassets/bgm_mod/%d.loop",
                 Paths_GetBasePath() ? Paths_GetBasePath() : "",
                 file_id);
        Sint64 loop_start, loop_length;
        if (parse_loop_file(loop_path, &loop_start, &loop_length)) {
            /* SDL3_mixer play API only supports loop_start_frame.
             * LOOPLENGTH from sidecar files isn't supported at the play API level —
             * the OGG vorbis comment parser handles it natively for OGG files.
             * For non-OGG, we can at least set the loop start point. */
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOP_START_FRAME_NUMBER, loop_start);
        }
    }

    bool ok = MIX_PlayTrack(music_track, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        SDL_Log("ModdedBGM: Failed to play %s: %s", path, SDL_GetError());
        MIX_DestroyAudio(current_audio);
        current_audio = NULL;
        return false;
    }

    fade_active = false;
    cached_bgm_count = -1; /* New track added/removed — invalidate cache */
    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[%s] ModdedBGM: Playing %s", ModdedBGM_GetGameStateString(), path);
    return true;
}

bool ModdedBGM_Play(int file_id) {
    if (!is_initialized)
        return false;

    // Must be enabled in config
    if (!Config_GetBool(CFG_KEY_MODDED_BGM_ENABLED)) {
        return false;
    }

    ModdedBGM_Stop();

    // Supported formats (SDL3_mixer decoders):
    //   ogg  — Vorbis (stb_vorbis / libvorbisfile) — supports LOOPSTART/LOOPLENGTH tags
    //   flac — FLAC (drflac / libflac)
    //   opus — Opus (libopusfile)
    //   mp3  — MP3 (drmp3 / mpg123)
    //   wav  — WAV (built-in)
    static const char* extensions[] = { "ogg", "flac", "opus", "mp3", "wav" };
    for (int i = 0; i < (int)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (try_load_and_play(extensions[i], file_id))
            return true;
    }

    return false;
}

void ModdedBGM_Stop(void) {
    if (!is_initialized)
        return;

    fade_active = false;
    MIX_StopTrack(music_track, 0);
    if (current_audio) {
        MIX_SetTrackAudio(music_track, NULL);
        MIX_DestroyAudio(current_audio);
        current_audio = NULL;
    }
    cached_bgm_count = -1; /* Track state changed — invalidate */
}

void ModdedBGM_Pause(bool pause) {
    if (!is_initialized)
        return;

    if (pause) {
        MIX_PauseTrack(music_track);
    } else {
        MIX_ResumeTrack(music_track);
    }
}

void ModdedBGM_SetVolume(int volume_db10) {
    if (!is_initialized)
        return;

    // volume_db10 comes in roughly between -999 and 0
    // Convert to linear gain (0.0 to 1.0) for MIX_SetTrackGain
    float gain = powf(10.0f, (float)volume_db10 / 200.0f);

    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 1.0f)
        gain = 1.0f;

    MIX_SetTrackGain(music_track, gain);
}

void ModdedBGM_FadeOut(int fade_ms) {
    if (!is_initialized || !current_audio)
        return;

    if (fade_ms <= 0) {
        ModdedBGM_Stop();
        return;
    }

    /* Use SDL3_mixer's native fade-out: MIX_StopTrack takes sample frames.
     * Convert ms to frames using the track's current format. */
    Sint64 fade_frames = MIX_TrackMSToFrames(music_track, (Sint64)fade_ms);
    if (fade_frames <= 0)
        fade_frames = 1; /* Fallback: at least 1 frame */

    fade_active = true;
    MIX_StopTrack(music_track, fade_frames);
    /* Note: after fade completes, track stops automatically.
     * We still need to clean up current_audio — this happens
     * on the next call to ModdedBGM_Stop() or ModdedBGM_Play(). */
}

/* ── Voice Line Support ── */

static bool try_load_voice(const char* voice_name, const char* ext) {
    char path[1024];
    build_asset_path(path, sizeof(path), "voice_mod", voice_name, ext);

    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io)
        return false;
    SDL_CloseIO(io);

    /* Stop any previously playing voice */
    if (current_voice_audio) {
        MIX_StopTrack(voice_track, 0);
        MIX_SetTrackAudio(voice_track, NULL);
        MIX_DestroyAudio(current_voice_audio);
        current_voice_audio = NULL;
    }

    current_voice_audio = MIX_LoadAudio(mixer, path, false);
    if (!current_voice_audio) {
        SDL_Log("ModdedBGM: Found voice %s but failed to load: %s", path, SDL_GetError());
        return false;
    }

    MIX_SetTrackAudio(voice_track, current_voice_audio);

    /* Play once — no looping */
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
    bool ok = MIX_PlayTrack(voice_track, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        SDL_Log("[%s] ModdedBGM: Failed to play voice %s: %s", ModdedBGM_GetGameStateString(), path, SDL_GetError());
        MIX_DestroyAudio(current_voice_audio);
        current_voice_audio = NULL;
        return false;
    }

    SDL_LogDebug(SDL_LOG_CATEGORY_APPLICATION, "[%s] ModdedBGM: Playing voice %s", ModdedBGM_GetGameStateString(), path);
    return true;
}

bool ModdedBGM_PlayVoice(const char* voice_name) {
    if (!is_initialized || !voice_name)
        return false;

    if (!Config_GetBool(CFG_KEY_MODDED_VOICE_ENABLED))
        return false;

    static const char* extensions[] = { "ogg", "flac", "opus", "mp3", "wav" };
    for (int i = 0; i < (int)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        if (try_load_voice(voice_name, extensions[i]))
            return true;
    }

    return false;
}

#define MAX_VOICE_MOD_CACHE 16
static struct {
    char name[32];
    bool is_modded;
    bool active;
} s_voice_mod_cache[MAX_VOICE_MOD_CACHE];

bool ModdedBGM_IsVoiceModded(const char* voice_name) {
    if (!voice_name)
        return false;

    // Check cache
    for (int i = 0; i < MAX_VOICE_MOD_CACHE; i++) {
        if (s_voice_mod_cache[i].active && strncmp(s_voice_mod_cache[i].name, voice_name, sizeof(s_voice_mod_cache[i].name)) == 0) {
            return s_voice_mod_cache[i].is_modded;
        }
    }

    bool found = false;
    static const char* extensions[] = { "ogg", "flac", "opus", "mp3", "wav" };
    for (int i = 0; i < (int)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        char path[1024];
        build_asset_path(path, sizeof(path), "voice_mod", voice_name, extensions[i]);
        SDL_IOStream* io = SDL_IOFromFile(path, "rb");
        if (io) {
            SDL_CloseIO(io);
            found = true;
            break;
        }
    }

    // Insert into cache
    static int insert_idx = 0;
    int target_idx = -1;
    for (int i = 0; i < MAX_VOICE_MOD_CACHE; i++) {
        if (!s_voice_mod_cache[i].active) {
            target_idx = i;
            break;
        }
    }
    
    if (target_idx == -1) {
        // If cache is full, we round-robin evict
        target_idx = insert_idx;
        insert_idx = (insert_idx + 1) % MAX_VOICE_MOD_CACHE;
    }
    
    s_voice_mod_cache[target_idx].active = true;
    s_voice_mod_cache[target_idx].is_modded = found;
    SDL_strlcpy(s_voice_mod_cache[target_idx].name, voice_name, sizeof(s_voice_mod_cache[0].name));

    return found;
}

int ModdedBGM_CountModdedTracks(void) {
    /* Return cached value if available to avoid per-frame filesystem scans */
    if (cached_bgm_count >= 0)
        return cached_bgm_count;

    char dir_path[1024];
    const char* base = Paths_GetBasePath();
    snprintf(dir_path, sizeof(dir_path), "%sassets/bgm_mod", base ? base : "");

    int count = 0;
    static const char* extensions[] = { "ogg", "flac", "opus", "mp3", "wav" };

    SDL_GlobFlags flags = 0;
    int num_results = 0;

    for (int e = 0; e < (int)(sizeof(extensions) / sizeof(extensions[0])); e++) {
        char pattern[32];
        snprintf(pattern, sizeof(pattern), "*.%s", extensions[e]);

        char** results = SDL_GlobDirectory(dir_path, pattern, flags, &num_results);
        if (results) {
            count += num_results;
            SDL_free(results);
        }
    }

    cached_bgm_count = count;
    return count;
}

bool ModdedSFX_Play(int reqNum, int ptix, int engine_code, int pan) {
    if (!is_initialized || reqNum < 0 || reqNum >= 65536)
        return false;

    char bank_dir_buf[8];
    const char* bank_dir = NULL;
    
    // Determine bank folder from the RUNTIME character loaded in this bank slot.
    // ptix is a bank SLOT index (always 1 for character sounds), not the character ID.
    // The actual character is tracked in g_cseSysWork.SpuBankId[slot].
    if (ptix == 0x7F) {
        /* BGM requests are modded via bgm_mod/, not voice_mod/ — log clearly */
        SDL_Log("[%s] ModdedSFX Check: req=%d → BGM track (mod via assets/bgm_mod/ folder)", ModdedBGM_GetGameStateString(), reqNum);
    } else if (ptix == 0) {
        bank_dir = "SE";
        SDL_Log("[%s] ModdedSFX Check: req=%d → assets/voice_mod/SE/%d.ogg", ModdedBGM_GetGameStateString(), reqNum, engine_code);
    } else {
        u32 char_data_id = g_cseSysWork.SpuBankId[ptix & 0xF];
        if (char_data_id >= 1 && char_data_id <= 20) {
            // char_data_id is 1-based index into cseTSBDataTable:
            // 1=PL00(Gill), 2=PL01(Alex), 3=PL02(Ryu), ...
            snprintf(bank_dir_buf, sizeof(bank_dir_buf), "PL%02d", (int)(char_data_id - 1));
            bank_dir = bank_dir_buf;
            SDL_Log("[%s] ModdedSFX Check: req=%d → assets/voice_mod/%s/%d.ogg", ModdedBGM_GetGameStateString(), reqNum, bank_dir, engine_code);
        } else {
            SDL_Log("[%s] ModdedSFX Check: req=%d → assets/voice_mod/%d.ogg (bank slot %d not loaded)", ModdedBGM_GetGameStateString(), reqNum, reqNum, ptix & 0xF);
        }
    }

    if (!Config_GetBool(CFG_KEY_MODDED_VOICE_ENABLED))
        return false;

    if (!sfx_attempted[reqNum]) {
        sfx_attempted[reqNum] = true;
        char path[1024];

        static const char* extensions[] = { "ogg", "wav", "flac", "opus", "mp3" };
        int num_exts = (int)(sizeof(extensions) / sizeof(extensions[0]));
        bool found = false;

        for (int i = 0; i < num_exts && !found; i++) {
            // Attempt 1: assets/voice_mod/{bank_dir}/{engine_code}.{ext} (Decimal Only)
            if (bank_dir) {
                snprintf(path, sizeof(path), "%sassets/voice_mod/%s/%d.%s",
                         Paths_GetBasePath() ? Paths_GetBasePath() : "",
                         bank_dir, engine_code, extensions[i]);
                SDL_IOStream* io = SDL_IOFromFile(path, "rb");
                if (io) {
                    SDL_CloseIO(io);
                    found = true;
                    break;
                }
            }

            // Attempt 2: assets/voice_mod/{reqNum}.{ext} (Legacy fallback - Decimal)
            snprintf(path, sizeof(path), "%sassets/voice_mod/%d.%s",
                     Paths_GetBasePath() ? Paths_GetBasePath() : "",
                     reqNum, extensions[i]);
            SDL_IOStream* io = SDL_IOFromFile(path, "rb");
            if (io) {
                SDL_CloseIO(io);
                found = true;
                break;
            }
        }

        if (found) {
            MIX_Audio* audio = MIX_LoadAudio(mixer, path, false);
            if (audio) {
                sfx_cache[reqNum] = audio;
                SDL_Log("[%s] ModdedSFX: Loaded %s for request %X (bank %s, idx %d)", ModdedBGM_GetGameStateString(), path, reqNum, bank_dir ? bank_dir : "?", engine_code);
            } else {
                SDL_Log("[%s] ModdedSFX: Failed to load %s: %s", ModdedBGM_GetGameStateString(), path, SDL_GetError());
            }
        }
    }

    if (sfx_cache[reqNum]) {
        MIX_Track* trk = sfx_tracks[next_sfx_track];
        if (trk) {
            MIX_StopTrack(trk, 0);
            MIX_SetTrackAudio(trk, sfx_cache[reqNum]);

            SDL_PropertiesID props = SDL_CreateProperties();
            SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, 0);
            MIX_PlayTrack(trk, props);
            SDL_DestroyProperties(props);

            // TODO: implement panning using MIX_SetTrackGain or MIX_SetTrackPanning if available in SDL3_mixer

            next_sfx_track = (next_sfx_track + 1) % NUM_SFX_TRACKS;
            return true;
        }
    }

    return false;
}

