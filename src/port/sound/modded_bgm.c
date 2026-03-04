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
static Mix_Music* current_music = NULL;

void ModdedBGM_Init(void) {
    if (is_initialized)
        return;

    // Use typical audio spec parameters. Mix_OpenAudio is usually straightforward in SDL3.
    SDL_AudioSpec spec;
    spec.freq = 48000;
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;

    if (!Mix_OpenAudio(0, &spec)) {
        fprintf(stderr, "ModdedBGM: Mix_OpenAudio failed: %s\n", SDL_GetError());
        return;
    }

    is_initialized = true;
    current_music = NULL;
}

void ModdedBGM_Exit(void) {
    if (!is_initialized)
        return;

    ModdedBGM_Stop();
    Mix_CloseAudio();
    is_initialized = false;
}

static bool try_load_and_play(const char* ext, int file_id) {
    char filename[256];
    snprintf(filename, sizeof(filename), "bgm_mod/%d.%s", file_id, ext);

    char path[1024];
    Paths_GetAssetPath(filename, path, sizeof(path));

    // Check if file exists by opening via SDL IO
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    if (!io) {
        return false;
    }
    SDL_CloseIO(io);

    current_music = Mix_LoadMUS(path);
    if (!current_music) {
        fprintf(stderr, "ModdedBGM: Found file %s but failed to load: %s\n", path, SDL_GetError());
        return false;
    }

    // Play infinitely
    if (Mix_PlayMusic(current_music, -1) == -1) {
        fprintf(stderr, "ModdedBGM: Failed to play %s: %s\n", path, SDL_GetError());
        Mix_FreeMusic(current_music);
        current_music = NULL;
        return false;
    }

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

    // Check extensions in order of preference
    if (try_load_and_play("ogg", file_id))
        return true;
    if (try_load_and_play("mp3", file_id))
        return true;
    if (try_load_and_play("wav", file_id))
        return true;

    return false;
}

void ModdedBGM_Stop(void) {
    if (!is_initialized)
        return;

    Mix_HaltMusic();
    if (current_music) {
        Mix_FreeMusic(current_music);
        current_music = NULL;
    }
}

void ModdedBGM_Pause(bool pause) {
    if (!is_initialized)
        return;

    if (pause) {
        Mix_PauseMusic();
    } else {
        Mix_ResumeMusic();
    }
}

void ModdedBGM_SetVolume(int volume_db10) {
    if (!is_initialized)
        return;

    // volume_db10 comes in roughly between -999 and 0
    // We convert it to linear gain (0.0 to 1.0) then scale to Mix_VolumeMusic (0 to MIX_MAX_VOLUME)
    float gain = powf(10.0f, (float)volume_db10 / 200.0f);
    int mix_vol = (int)(gain * MIX_MAX_VOLUME);

    if (mix_vol < 0)
        mix_vol = 0;
    if (mix_vol > MIX_MAX_VOLUME)
        mix_vol = MIX_MAX_VOLUME;

    Mix_VolumeMusic(mix_vol);
}
