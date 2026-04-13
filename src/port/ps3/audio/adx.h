#ifndef _PS3_ADX_H_
#define _PS3_ADX_H_

#include "types.h"
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum ADXState {
    ADX_STATE_STOP,
    ADX_STATE_PLAYING,
    ADX_STATE_PLAYEND,
} ADXState;

void ADX_ProcessTracks(void);

void ADX_Init(void);
void ADX_Exit(void);
void ADX_Stop(void);
int ADX_IsPaused(void);
void ADX_Pause(int pause);
void ADX_StartMem(void* buf, size_t size);
int ADX_GetNumFiles(void);
void ADX_EntryAfs(int file_id);
void ADX_StartSeamless(void);
void ADX_ResetEntry(void);
void ADX_StartAfs(int file_id);
void ADX_SetOutVol(int volume);
void ADX_SetMono(bool mono);
ADXState ADX_GetState(void);

// Mixes currently playing ADX tracks into a 32-bit floating point stereo PCM buffer.
// out_buffer: interleaved stereo floats [L, R, L, R, ...]
// num_frames: number of stereo frames to mix (e.g. 256 frames = 512 floats)
void ADX_MixFloatPCM(float* out_buffer, int num_frames);

#ifdef __cplusplus
}
#endif

#endif // _PS3_ADX_H_
