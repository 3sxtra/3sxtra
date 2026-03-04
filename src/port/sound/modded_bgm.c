#include "port/sound/modded_bgm.h"
#include "port/config.h"
#include "port/paths.h"

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool is_initialized = false;
static MIX_Mixer* mixer = NULL;
static MIX_Track* music_track = NULL;
static MIX_Audio* current_audio = NULL;

void ModdedBGM_Init(void) {
    if (is_initialized)
        return;

    if (!MIX_Init()) {
        fprintf(stderr, "ModdedBGM: MIX_Init failed: %s\n", SDL_GetError());
        return;
    }

    SDL_AudioSpec spec;
    spec.freq = 48000;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec);
    if (!mixer) {
        fprintf(stderr, "ModdedBGM: MIX_CreateMixerDevice failed: %s\n", SDL_GetError());
        MIX_Quit();
        return;
    }

    music_track = MIX_CreateTrack(mixer);
    if (!music_track) {
        fprintf(stderr, "ModdedBGM: MIX_CreateTrack failed: %s\n", SDL_GetError());
        MIX_DestroyMixer(mixer);
        mixer = NULL;
        MIX_Quit();
        return;
    }

    is_initialized = true;
    current_audio = NULL;
}

void ModdedBGM_Exit(void) {
    if (!is_initialized)
        return;

    ModdedBGM_Stop();

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

static bool try_load_and_play(const char* ext, int file_id) {
    char path[1024];
    const char* base = Paths_GetBasePath();
    snprintf(path, sizeof(path), "%sassets/bgm_mod/%d.%s", base ? base : "", file_id, ext);

    // Check if file exists by opening via SDL IO
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) {
        return false;
    }
    SDL_CloseIO(io);

    current_audio = MIX_LoadAudio(mixer, path, false);
    if (!current_audio) {
        fprintf(stderr, "ModdedBGM: Found file %s but failed to load: %s\n", path, SDL_GetError());
        return false;
    }

    MIX_SetTrackAudio(music_track, current_audio);

    // Play with infinite looping
    SDL_PropertiesID props = SDL_CreateProperties();
    SDL_SetNumberProperty(props, MIX_PROP_PLAY_LOOPS_NUMBER, -1);
    bool ok = MIX_PlayTrack(music_track, props);
    SDL_DestroyProperties(props);

    if (!ok) {
        fprintf(stderr, "ModdedBGM: Failed to play %s: %s\n", path, SDL_GetError());
        MIX_DestroyAudio(current_audio);
        current_audio = NULL;
        return false;
    }

    fprintf(stderr, "ModdedBGM: Playing %s\n", path);
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
    //   ogg  — Vorbis (stb_vorbis / libvorbisfile)
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

    MIX_StopTrack(music_track, 0);
    if (current_audio) {
        MIX_SetTrackAudio(music_track, NULL);
        MIX_DestroyAudio(current_audio);
        current_audio = NULL;
    }
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
