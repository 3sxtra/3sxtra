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

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

static bool is_initialized = false;
static MIX_Mixer* mixer = NULL;
static MIX_Track* music_track = NULL;
static MIX_Track* voice_track = NULL;
static MIX_Audio* current_audio = NULL;
static MIX_Audio* current_voice_audio = NULL;

/* Fade-out state — not needed, MIX_StopTrack has native fade.
 * We track whether a fade is in progress to avoid restarting. */
static bool fade_active = false;

/* Cached track count to avoid per-frame filesystem scans */
static int cached_bgm_count = -1;  /* -1 = needs refresh */

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
    const char* base = Paths_GetBasePath();
    snprintf(out, out_size, "%sassets/%s/%s.%s", base ? base : "", subdir, name, ext);
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
        snprintf(loop_path, sizeof(loop_path), "%sassets/bgm_mod/%d.loop",
                 Paths_GetBasePath() ? Paths_GetBasePath() : "", file_id);
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
    cached_bgm_count = -1;  /* New track added/removed — invalidate cache */
    SDL_Log("ModdedBGM: Playing %s", path);
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
    cached_bgm_count = -1;  /* Track state changed — invalidate */
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
        fade_frames = 1;  /* Fallback: at least 1 frame */

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
        SDL_Log("ModdedBGM: Failed to play voice %s: %s", path, SDL_GetError());
        MIX_DestroyAudio(current_voice_audio);
        current_voice_audio = NULL;
        return false;
    }

    SDL_Log("ModdedBGM: Playing voice %s", path);
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

bool ModdedBGM_IsVoiceModded(const char* voice_name) {
    if (!voice_name)
        return false;

    static const char* extensions[] = { "ogg", "flac", "opus", "mp3", "wav" };
    for (int i = 0; i < (int)(sizeof(extensions) / sizeof(extensions[0])); i++) {
        char path[1024];
        build_asset_path(path, sizeof(path), "voice_mod", voice_name, extensions[i]);
        SDL_IOStream* io = SDL_IOFromFile(path, "rb");
        if (io) {
            SDL_CloseIO(io);
            return true;
        }
    }
    return false;
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
