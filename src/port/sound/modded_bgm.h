/**
 * @file modded_bgm.h
 * @brief Modded background music API.
 */
#ifndef MODDED_BGM_H
#define MODDED_BGM_H

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void ModdedBGM_Init(void);
void ModdedBGM_Exit(void);

/**
 * @brief Attempts to play a modded BGM track for the given file_id.
 *
 * Checks if modded BGM is enabled, and if a corresponding file
 * (e.g. 89.ogg, 89.mp3, 89.wav) exists in the assets/bgm_mod/ folder.
 *
 * @param file_id The track ID (fnum) to attempt to play.
 * @return true if a modded track was found and playback started successfully.
 *         false if no modded track was found or modding is disabled.
 */
bool ModdedBGM_Play(int file_id);

void ModdedBGM_Stop(void);
void ModdedBGM_SetVolume(int volume_db10);
void ModdedBGM_Pause(bool pause);

#ifdef __cplusplus
}
#endif

#endif // MODDED_BGM_H
