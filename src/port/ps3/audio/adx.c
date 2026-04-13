/**
 * @file adx.c
 * @brief CRI ADX audio playback engine with loop support for PS3 cellAudio.
 */
#include "port/ps3/audio/adx.h"
#include "common.h"
#include "port/io/afs.h"
#include "port/ps3/audio/adx_decoder.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ppu_thread.h>
#include <sys/synchronization.h>

// ⚡ Bolt: Pre-allocated buffer pool for ADX file loading.
#define ADX_POOL_BUF_SIZE (512 * 1024)
// A-LOW-01 Audit Fix: 4 concurrent ADX streams sufficient for a fighting game (saves 3MB BSS)
#define ADX_POOL_COUNT 4
static uint8_t adx_pool[ADX_POOL_COUNT][ADX_POOL_BUF_SIZE];
static bool adx_pool_used[ADX_POOL_COUNT] = { false };

static sys_mutex_t adx_mutex;

static void* pool_alloc(size_t size) {
    // A-04 Audit Fix: Lock mutex — pool_used[] accessed from main + audio threads
    sys_mutex_lock(adx_mutex, 0);
    if (size <= ADX_POOL_BUF_SIZE) {
        for (int i = 0; i < ADX_POOL_COUNT; i++) {
            if (!adx_pool_used[i]) {
                adx_pool_used[i] = true;
                sys_mutex_unlock(adx_mutex);
                return adx_pool[i];
            }
        }
    }
    sys_mutex_unlock(adx_mutex);
    return malloc(size); // Fallback
}

static void pool_free(void* ptr) {
    uint8_t* p = (uint8_t*)ptr;
    uint8_t* pool_start = &adx_pool[0][0];
    uint8_t* pool_end = &adx_pool[ADX_POOL_COUNT - 1][ADX_POOL_BUF_SIZE];
    if (p >= pool_start && p < pool_end) {
        int index = (int)((p - pool_start) / ADX_POOL_BUF_SIZE);
        // A-04 Audit Fix: Lock mutex for pool_used[] write
        sys_mutex_lock(adx_mutex, 0);
        adx_pool_used[index] = false;
        sys_mutex_unlock(adx_mutex);
        return;
    }
    free(ptr);
}

#define SAMPLE_RATE 48000
#define N_CHANNELS 2
#define BYTES_PER_SAMPLE 2
#define TRACKS_MAX 10

#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))

#define ADX_RB16(p) ((uint16_t)(((const uint8_t*)(p))[0] << 8 | ((const uint8_t*)(p))[1]))
#define ADX_RB32(p)                                                                                                    \
    ((uint32_t)(((const uint8_t*)(p))[0] << 24 | ((const uint8_t*)(p))[1] << 16 | ((const uint8_t*)(p))[2] << 8 |      \
                ((const uint8_t*)(p))[3]))

typedef struct ADXLoopInfo {
    bool looping_enabled;
    int start_sample;
    int end_sample;
} ADXLoopInfo;

typedef struct ADXTrack {
    int size;
    uint8_t* data;
    bool should_free_data_after_use;
    int used_bytes;
    int processed_samples;
    ADXLoopInfo loop_info;
    ADXContext ctx;
    bool exhausted;
    s16 leftover_buf[1024 * ADX_MAX_CHANNELS];
    int leftover_samples;
} ADXTrack;

static ADXTrack tracks[TRACKS_MAX] = { 0 };
static int num_tracks = 0;
static int first_track_index = 0;
static bool has_tracks = false;
static float master_gain = 1.0f;
static bool is_paused = false;

static void* load_file(int file_id, int* size) {
    const unsigned int file_size = AFS_GetSize(file_id);
    if (file_size == 0) {
        printf("[ADX] load_file: file_id %d has zero size\n", file_id);
        *size = 0;
        return NULL;
    }
    *size = file_size;
    const unsigned int sectors = (file_size + 2048 - 1) / 2048;
    const size_t buff_size = (size_t)sectors * 2048;

    void* buff = pool_alloc(buff_size);
    if (!buff) {
        printf("[ADX] load_file: allocation failed for %zu bytes (file_id %d)\n", buff_size, file_id);
        *size = 0;
        return NULL;
    }

    AFSHandle handle = AFS_Open(file_id);
    // F-HIGH-01 Audit Fix: Use audio-specific read path to avoid mutex contention
    // with main thread disk I/O on real hardware.
    AFS_ReadSyncAudio(handle, sectors, buff);
    AFS_Close(handle);

    return buff;
}

static bool track_reached_eof(ADXTrack* track) {
    if (track->ctx.frame_size > 0) {
        return (track->size - (int)track->used_bytes) < track->ctx.frame_size;
    }
    return (track->size - (int)track->used_bytes) <= 0;
}

static void loop_info_init(ADXLoopInfo* info, const uint8_t* data) {
    if (!data)
        return;

    const uint8_t version = data[0x12];
    switch (version) {
    case 3:
        if (ADX_RB16(data + 0x16) == 1) {
            info->looping_enabled = true;
            info->start_sample = ADX_RB32(data + 0x1C);
            info->end_sample = ADX_RB32(data + 0x24);
        }
        break;
    case 4:
        if (ADX_RB32(data + 0x24) == 1) {
            info->looping_enabled = true;
            info->start_sample = ADX_RB32(data + 0x28);
            info->end_sample = ADX_RB32(data + 0x30);
        }
        break;
    default:
        break;
    }

    if (info->looping_enabled) {
        if (info->end_sample <= info->start_sample) {
            info->looping_enabled = false;
        }
    }
}

static void loop_info_destroy(ADXLoopInfo* info) {
    memset(info, 0, sizeof(ADXLoopInfo));
}

static void track_init(ADXTrack* track, int file_id, void* buf, size_t buf_size, bool looping_allowed) {
    if (file_id != -1) {
        track->data = load_file(file_id, &track->size);
        track->should_free_data_after_use = true;
    } else {
        track->data = buf;
        track->size = (int)buf_size;
        track->should_free_data_after_use = false;
    }

    /* If data is NULL (allocation failure or invalid file), mark exhausted
     * immediately so the mixer thread never tries to decode from it. */
    if (!track->data || track->size <= 0) {
        printf("[ADX] track_init: NULL data for file_id=%d, marking exhausted\n", file_id);
        track->exhausted = true;
        return;
    }

    if (ADX_InitContext(&track->ctx, track->data, track->size) < 0) {
        printf("[ADX] track_init: ADX_InitContext failed for file_id=%d, marking exhausted\n", file_id);
        track->exhausted = true;
        return;
    }

    track->used_bytes = track->ctx.data_offset;
    track->processed_samples = 0;
    track->leftover_samples = 0;
    track->exhausted = false;

    if (looping_allowed) {
        loop_info_init(&track->loop_info, track->data);
    }
}

static void track_destroy(ADXTrack* track) {
    loop_info_destroy(&track->loop_info);
    if (track->should_free_data_after_use) {
        pool_free(track->data);
    }
    memset(track, 0, sizeof(ADXTrack));
}

static ADXTrack* alloc_track() {
    const int index = (first_track_index + num_tracks) % TRACKS_MAX;
    num_tracks += 1;
    has_tracks = true;
    return &tracks[index];
}

static bool adx_initialized = false;

void ADX_Init(void) {
    if (adx_initialized) return;
    adx_initialized = true;
    
    if (master_gain <= 0.01f) {
        master_gain = 1.0f;
    }
    if (master_gain > 1.0f) {
        master_gain = 1.0f;
    }

    sys_mutex_attribute_t attr;
    memset(&attr, 0, sizeof(attr));
    sys_mutex_attribute_initialize(attr);
    attr.attr_recursive = SYS_SYNC_RECURSIVE;
    sys_mutex_create(&adx_mutex, &attr);
}

void ADX_Exit(void) {
    ADX_Stop();
    sys_mutex_destroy(adx_mutex);
}

void ADX_Stop(void) {
    sys_mutex_lock(adx_mutex, 0);
    for (int i = 0; i < num_tracks; i++) {
        const int j = (first_track_index + i) % TRACKS_MAX;
        track_destroy(&tracks[j]);
    }
    num_tracks = 0;
    first_track_index = 0;
    has_tracks = false;
    is_paused = true;
    sys_mutex_unlock(adx_mutex);
}

int ADX_IsPaused(void) {
    return is_paused;
}

void ADX_Pause(int pause) {
    is_paused = pause;
}

void ADX_StartMem(void* buf, size_t size) {
    ADX_Stop();
    sys_mutex_lock(adx_mutex, 0);
    ADXTrack* track = alloc_track();
    track_init(track, -1, buf, size, true);
    has_tracks = true;
    sys_mutex_unlock(adx_mutex);
}

int ADX_GetNumFiles(void) {
    return num_tracks;
}

void ADX_EntryAfs(int file_id) {
    sys_mutex_lock(adx_mutex, 0);
    ADXTrack* track = alloc_track();
    track_init(track, file_id, NULL, 0, false);
    has_tracks = true;
    sys_mutex_unlock(adx_mutex);
}

void ADX_StartSeamless(void) {
    ADX_Pause(false);
}

void ADX_ResetEntry(void) {}

void ADX_StartAfs(int file_id) {
    ADX_Stop();
    sys_mutex_lock(adx_mutex, 0);
    ADXTrack* track = alloc_track();
    track_init(track, file_id, NULL, 0, true);
    has_tracks = true;
    sys_mutex_unlock(adx_mutex);
}

void ADX_SetOutVol(int volume) {
    // M-11 Audit Fix: Implement dB-to-linear gain conversion
    // ADX volume is typically in centiBels (1/100 dB), range roughly -1000 to 0
    float gain = powf(10.0f, (float)volume / 2000.0f);
    if (gain < 0.0f)
        gain = 0.0f;
    if (gain > 1.0f)
        gain = 1.0f;
    master_gain = gain;
}

void ADX_SetMono(bool mono) {
    (void)mono;
}

void ADX_ProcessTracks(void) {}

ADXState ADX_GetState(void) {
    if (!has_tracks)
        return ADX_STATE_STOP;
    if (is_paused)
        return ADX_STATE_STOP;
    return ADX_STATE_PLAYING;
}

// Float mixing function invoked by cellAudio callback
void ADX_MixFloatPCM(float* out_buffer, int num_frames) {
    int total_samples_req = num_frames * N_CHANNELS;
    memset(out_buffer, 0, total_samples_req * sizeof(float));

    if (is_paused || !has_tracks)
        return;

    sys_mutex_lock(adx_mutex, 0);

    for (int i = 0; i < num_tracks; i++) {
        const int track_idx = (first_track_index + i) % TRACKS_MAX;
        ADXTrack* track = &tracks[track_idx];

        if (track->exhausted)
            continue;

        /* Critical: if somehow a track made it here with NULL data, kill it. */
        if (!track->data) {
            track->exhausted = true;
            continue;
        }

        int samples_written = 0;
        int max_iters = 50; // Safety breakout

        while (samples_written < total_samples_req && max_iters-- > 0) {
            int samples_needed = total_samples_req - samples_written;

            if (track->loop_info.looping_enabled) {
                int samples_until_loop = (track->loop_info.end_sample - track->processed_samples) * track->ctx.channels;
                if (samples_needed > samples_until_loop) {
                    samples_needed = samples_until_loop;
                }
            }

            if (samples_needed <= 0 && track->loop_info.looping_enabled) {
                // Seek back to start_sample (aligned to block boundary)
                if (track->ctx.samples_per_block > 0) {
                    int frame_idx = track->loop_info.start_sample / track->ctx.samples_per_block;
                    track->used_bytes = track->ctx.data_offset + frame_idx * track->ctx.frame_size;
                    track->processed_samples = frame_idx * track->ctx.samples_per_block;
                }
                memset(track->ctx.ch_state, 0, sizeof(track->ctx.ch_state));
                track->leftover_samples = 0;
                continue;
            }

            int samples_to_copy = 0;

            if (track->leftover_samples > 0) {
                samples_to_copy = MIN(samples_needed, track->leftover_samples);
                for (int m = 0; m < samples_to_copy; m++) {
                    float s = (float)track->leftover_buf[m] / 32768.0f;
                    out_buffer[samples_written + m] += s * master_gain;
                }

                if (samples_to_copy < track->leftover_samples) {
                    memmove(track->leftover_buf, track->leftover_buf + samples_to_copy,
                           (track->leftover_samples - samples_to_copy) * sizeof(s16));
                }
                track->leftover_samples -= samples_to_copy;
                
                samples_written += samples_to_copy;
                track->processed_samples += (samples_to_copy / track->ctx.channels);

            } else {
                int remaining_bytes = track->size - (int)track->used_bytes;
                if (remaining_bytes < track->ctx.frame_size) {
                    track->exhausted = true;
                    break;
                }

                int samples_to_decode = 1024 * track->ctx.channels;
                int bytes_consumed = 0;

                int ret = ADX_Decode(&track->ctx,
                                     track->data + track->used_bytes,
                                     remaining_bytes,
                                     track->leftover_buf,
                                     &samples_to_decode,
                                     &bytes_consumed);

                if (ret < 0 || samples_to_decode == 0) {
                    track->exhausted = true;
                    break;
                }

                track->used_bytes += bytes_consumed;
                track->leftover_samples = samples_to_decode;
            }
        }
    }

    // Cleanup exhausted tracks from the front
    while (num_tracks > 0) {
        ADXTrack* track = &tracks[first_track_index];
        if (!track->exhausted)
            break;

        track_destroy(track);
        num_tracks -= 1;
        first_track_index = (first_track_index + 1) % TRACKS_MAX;
    }

    if (num_tracks == 0) {
        first_track_index = 0;
        has_tracks = false;
    }

    sys_mutex_unlock(adx_mutex);
}
