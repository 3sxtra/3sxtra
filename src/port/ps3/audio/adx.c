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
#define ADX_RB32(p) \
    ((uint32_t)(((const uint8_t*)(p))[0] << 24 | ((const uint8_t*)(p))[1] << 16 | ((const uint8_t*)(p))[2] << 8 | \
                ((const uint8_t*)(p))[3]))

typedef struct ADXLoopInfo {
    bool looping_enabled;
    int start_sample;
    int end_sample;
    uint8_t* data;
    int data_size;
    int position;
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

static bool track_loop_filled(ADXTrack* track) {
    if (track->loop_info.looping_enabled) {
        return track->processed_samples >= track->loop_info.end_sample;
    } else {
        return false;
    }
}

static bool track_needs_decoding(ADXTrack* track) {
    if (track->loop_info.looping_enabled) {
        return !track_loop_filled(track);
    } else {
        return !track_reached_eof(track);
    }
}

static int track_add_samples_to_loop(ADXTrack* track, uint8_t* buf, int num_samples) {
    ADXLoopInfo* loop_info = &track->loop_info;
    if (!loop_info->looping_enabled) {
        return 0;
    }

    const int buf_sample_start = MAX(loop_info->start_sample - track->processed_samples, 0);
    const int buf_sample_end = MIN(loop_info->end_sample - track->processed_samples, num_samples);

    if (buf_sample_end > buf_sample_start) {
        const int buf_start = buf_sample_start * N_CHANNELS * BYTES_PER_SAMPLE;
        const int buf_end = buf_sample_end * N_CHANNELS * BYTES_PER_SAMPLE;
        const int buf_len = buf_end - buf_start;

        if (loop_info->position + buf_len <= loop_info->data_size) {
            memcpy(loop_info->data + loop_info->position, buf + buf_start, buf_len);
            loop_info->position += buf_len;

            if (loop_info->position == loop_info->data_size) {
                loop_info->position = 0;
            }
        }
    }

    const int overflow = MAX(track->processed_samples + num_samples - loop_info->end_sample, 0);
    track->processed_samples += num_samples;
    return overflow;
}

static void loop_info_init(ADXLoopInfo* info, const uint8_t* data) {
    if (!data) return;

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
            return;
        }
        info->data_size = (info->end_sample - info->start_sample) * BYTES_PER_SAMPLE * N_CHANNELS;
        info->data = malloc(info->data_size);
        if (!info->data) {
            printf("[ADX] loop_info_init: malloc failed for loop buffer (%d bytes)\n", info->data_size);
            info->looping_enabled = false;
            return;
        }
        info->position = 0;
    }
}

static void loop_info_destroy(ADXLoopInfo* info) {
    if (info->looping_enabled) {
        free(info->data);
    }
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

void ADX_Init(void) {
    sys_mutex_attribute_t attr;
    sys_mutex_attribute_initialize(attr);
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

void ADX_ResetEntry(void) {
}

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
    if (gain < 0.0f) gain = 0.0f;
    if (gain > 1.0f) gain = 1.0f;
    master_gain = gain;
}

void ADX_SetMono(bool mono) {
    (void)mono;
}

void ADX_ProcessTracks(void) {}

ADXState ADX_GetState(void) {
    if (!has_tracks) return ADX_STATE_STOP;
    if (is_paused) return ADX_STATE_STOP;
    return ADX_STATE_PLAYING;
}

// Float mixing function invoked by cellAudio callback
void ADX_MixFloatPCM(float* out_buffer, int num_frames) {
    int total_samples_req = num_frames * N_CHANNELS;
    memset(out_buffer, 0, total_samples_req * sizeof(float));

    if (is_paused || !has_tracks) return;

    sys_mutex_lock(adx_mutex, 0);

    for (int i = 0; i < num_tracks; i++) {
        const int track_idx = (first_track_index + i) % TRACKS_MAX;
        ADXTrack* track = &tracks[track_idx];
        
        if (track->exhausted) continue;

        /* Critical: if somehow a track made it here with NULL data, kill it. */
        if (!track->data) {
            track->exhausted = true;
            continue;
        }

        int samples_written = 0;
        int max_iters = 50; // Safety breakout

        while (samples_written < total_samples_req && max_iters-- > 0) {
            int samples_needed = total_samples_req - samples_written;
            
            // 1. Queue loaded loop buffer if we are looping
            if (track_loop_filled(track)) {
                if (!track->loop_info.data) {
                    track->exhausted = true;
                    break;
                }
                int loop_avail = (track->loop_info.data_size - track->loop_info.position) / sizeof(int16_t);
                int mix_amt = MIN(samples_needed, loop_avail);
                if (mix_amt <= 0) {
                    track->loop_info.position = 0;
                    continue;
                }
                
                int16_t* src_s16 = (int16_t*)(track->loop_info.data + track->loop_info.position);
                for (int m = 0; m < mix_amt; m++) {
                    float s = (float)src_s16[m] / 32768.0f;
                    out_buffer[samples_written + m] += s * master_gain;
                }
                
                track->loop_info.position += mix_amt * sizeof(int16_t);
                if (track->loop_info.position >= track->loop_info.data_size) {
                    track->loop_info.position = 0; // restart loop
                }
                samples_written += mix_amt;
            }
            // 2. Decode fresh samples if available
            else if (track_needs_decoding(track)) {
                int remaining = track->size - (int)track->used_bytes;
                if (remaining <= 0) {
                    track->exhausted = true;
                    break;
                }

                int16_t decode_buf[1024 * N_CHANNELS];
                int samples_to_decode = MIN(samples_needed, 1024 * N_CHANNELS);
                int bytes_consumed = 0;

                int ret = ADX_Decode(&track->ctx,
                                     track->data + track->used_bytes,
                                     remaining,
                                     decode_buf,
                                     &samples_to_decode,
                                     &bytes_consumed);

                if (ret < 0 || samples_to_decode == 0) {
                    track->exhausted = true;
                    break;
                }

                track->used_bytes += bytes_consumed;

                int spc = samples_to_decode / track->ctx.channels;
                int overflow = track_add_samples_to_loop(track, (uint8_t*)decode_buf, spc);
                int overflow_total = overflow * track->ctx.channels;
                int samples_to_queue = samples_to_decode - overflow_total;

                for (int m = 0; m < samples_to_queue; m++) {
                    float s = (float)decode_buf[m] / 32768.0f;
                    out_buffer[samples_written + m] += s * master_gain;
                }
                samples_written += samples_to_queue;
            }
            // 3. Exhausted and not looping
            else {
                track->exhausted = true;
                break;
            }
        }
    }

    // Cleanup exhausted tracks from the front
    while (num_tracks > 0) {
        ADXTrack* track = &tracks[first_track_index];
        if (!track->exhausted) break;
        
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
