#include "port/ps3/audio/adx.h"
#include <ppu_intrinsics.h>
#include <cell/spurs/lv2_event_queue.h>
#include <cell/audio.h>
#include <sys/ppu_thread.h>
#include <sys/sys_time.h>
#include <sys/timer.h>
#include <sys/event.h>
#include <cell/sysmodule.h>
#include <sysutil/sysutil_common.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "port/ps3/app/ps3_app.h"

static uint32_t audio_port_num;
static sys_ppu_thread_t audio_thread_id;
static volatile int audio_thread_running = 0;
static volatile int audio_subsystem_ready = 0;
static int audio_port_opened = 0;
static int audio_module_loaded = 0; // A-03: Track whether CELL_SYSMODULE_AUDIO loaded OK
static sys_event_queue_t audio_event_queue;
static int audio_event_queue_created = 0;
static sys_ipc_key_t audio_queue_key;
static uint8_t audio_spurs_port; // SPU Port mapping for SPURS audio signaling
static sys_semaphore_t audio_ready_sem;
static int audio_ready_sem_created = 0;

extern void SPU_TickAudio(int16_t* outbuf, uint32_t samples_per_channel);

/* Called by the main thread after emlShimInit()/SPU_Init() have completed.
 * This unblocks the audio feeder thread from calling SPU_TickAudio. */
void ps3_audio_signal_ready(void) {
    printf("[PS3] Audio subsystem signaled ready\n");
    __lwsync(); /* P2: Store-release barrier */
    audio_subsystem_ready = 1;
    __lwsync(); /* Ensure visible before sem post */
    if (audio_ready_sem_created) {
        sys_semaphore_post(audio_ready_sem, 1);
    }
}

static void audio_thread_entry(uint64_t arg) {
    (void)arg;
    float mix_buffer[CELL_AUDIO_BLOCK_SAMPLES * 2] __attribute__((aligned(16))); // Stereo (512 floats)
    int16_t sfx_buffer[CELL_AUDIO_BLOCK_SAMPLES * 2] __attribute__((aligned(16)));

    // printf removed to prevent TTY deadlock during initialization

    /* Wait until the main thread has finished initializing SPU_Init /
     * emlShimInit so that soundLock and timer_cb are valid pointers. */
    while (audio_thread_running && !audio_subsystem_ready) {
        if (audio_ready_sem_created) {
            int err = sys_semaphore_wait(audio_ready_sem, 100000); /* 100ms timeout */
            if (err == CELL_OK) {
                break;
            }
        } else {
            sys_timer_usleep(1000); /* sleep 1ms fallback */
        }
    }
    __isync(); /* P2: Load-acquire barrier */

    if (!audio_thread_running) {
        // Thread shutdown before ready
        sys_ppu_thread_exit(0);
        return;
    }

    // Thread ready to pump audio
    printf("[PS3] Priming CellAudio buffer...\n");
    float audio_buffer[256 * 2] __attribute__((aligned(16))) = {0}; // PS3 audio expects interleaved (L, R) buffer
    
    // Seed buffer with silence so the port immediately has data to consume, triggering an event
    for (int i = 0; i < 8; i++) {
        cellAudioAddData(audio_port_num, audio_buffer, CELL_AUDIO_BLOCK_SAMPLES, 1.0f);
    }

    printf("[PS3] Starting CellAudio port now that subsystem is ready...\n");
    int port_ret = cellAudioPortStart(audio_port_num);
    if (port_ret != CELL_OK) {
        printf("[PS3] ERROR: cellAudioPortStart failed with 0x%x\n", port_ret);
    }

    while (audio_thread_running) {
        sys_event_t audio_event;
        // Use a 100ms timeout instead of waiting forever (SYS_NO_TIMEOUT)
        // to ensure the thread can gracefully shut down if the main app crashes
        int res = sys_event_queue_receive(audio_event_queue, &audio_event, 100000);

        if (res == (int)0x80010008 /* CELL_ETIMEDOUT */) {
            if (!audio_thread_running)
                break;
            // Fall through and try to pump data to recover from starvation
        } else if (res != CELL_OK) {
            // Event queue receive failed, shut down.
            break;
        }

        // Ensure buffer is clean
        memset(mix_buffer, 0, sizeof(mix_buffer));

        // Pull decoded and mixed floats from our custom software mixer
        ADX_MixFloatPCM(mix_buffer, CELL_AUDIO_BLOCK_SAMPLES);

        // Fetch SPU SFX and mix it with floats
        memset(sfx_buffer, 0, sizeof(sfx_buffer));
        SPU_TickAudio(sfx_buffer, CELL_AUDIO_BLOCK_SAMPLES);

        // CellAudio expects interleaved data: [L, R, L, R...]
        for (int i = 0; i < CELL_AUDIO_BLOCK_SAMPLES; i++) {
            float sfx_l = sfx_buffer[i * 2 + 0] / 32768.0f;
            float sfx_r = sfx_buffer[i * 2 + 1] / 32768.0f;

            float out_l = mix_buffer[i * 2 + 0] + sfx_l;
            float out_r = mix_buffer[i * 2 + 1] + sfx_r;

            if (out_l > 1.0f)
                out_l = 1.0f;
            if (out_l < -1.0f)
                out_l = -1.0f;
            if (out_r > 1.0f)
                out_r = 1.0f;
            if (out_r < -1.0f)
                out_r = -1.0f;

            audio_buffer[i * 2 + 0] = out_l;
            audio_buffer[i * 2 + 1] = out_r;
        }

        // Send to PS3 audio system (passing exactly CELL_AUDIO_BLOCK_SAMPLES frames)
        int ret;
        int retry_count = 0;
        do {
            ret = cellAudioAddData(audio_port_num, audio_buffer, CELL_AUDIO_BLOCK_SAMPLES, 1.0f);
            if (ret != CELL_OK) {
                if (ret == (int)0x80310701 /* CELL_AUDIO_ERROR_NOT_INIT */ ||
                    ret == (int)0x80310704 /* CELL_AUDIO_ERROR_PARAM */) {
                    // Port error, stopping feeder
                    audio_thread_running = 0;
                    break; /* Port is dead, exit cleanly */
                } else if (ret == (int)0x80310703 /* CELL_AUDIO_ERROR_BUSY */) {
                    // Buffer is full, wait a bit
                    sys_timer_usleep(1000);
                } else if (ret == (int)0x80310705 /* CELL_AUDIO_ERROR_TRANS */ || ret == (int)0x80310706 /* CELL_AUDIO_ERROR_PORT_FULL */) {
                    // Starvation / Port State error. The hardware buffer underran or state is invalid.
                    // We must forcibly restart the port to resume playback without deadlocking.
                    printf("[PS3 Audio] Starvation detected (0x%X). Restarting port %d.\n", ret, audio_port_num);
                    cellAudioPortStop(audio_port_num);
                    cellAudioPortStart(audio_port_num);
                    sys_timer_usleep(500); // Give it a moment to recover
                } else {
                    sys_timer_usleep(500);
                }
                
                retry_count++;
                if (retry_count > 100) {
                    printf("[PS3 Audio] Buffer pump failed permanently with error: 0x%X\n", ret);
                    audio_thread_running = 0;
                    break;
                }
            }
        } while (ret != CELL_OK && audio_thread_running);
    }

    sys_ppu_thread_exit(0);
}

void ps3_audio_init(void) {
    printf("[PS3] Audio init: cellAudio starting via ADX\n");

    // A-05 Audit Fix: Removed unused CELL_SYSMODULE_RTC load (no RTC functions used)
    int ret = cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO);
    if (ret < 0) {
        printf("[PS3] Warning: cellSysmoduleLoadModule(CELL_SYSMODULE_AUDIO) returned 0x%X\n", ret);
    } else {
        audio_module_loaded = 1; // A-03: Track successful load for safe unload
    }

    ret = cellAudioInit();
    if (ret != CELL_OK && ret != CELL_AUDIO_ERROR_ALREADY_INIT) {
        printf("[PS3] Audio init failed permanently: 0x%X\n", ret);
        char err_msg[128];
        snprintf(err_msg, sizeof(err_msg), "cellAudioInit failed: 0x%X\nPlease ensure audio output is configured correctly in XMB.", ret);
        PS3App_ShowFatalError(err_msg);
    }
    printf("[PS3] cellAudio initialized successfully!\n");

    CellAudioPortParam audio_param;
    audio_param.nChannel = CELL_AUDIO_PORT_2CH;
    audio_param.nBlock = CELL_AUDIO_BLOCK_8;
    audio_param.attr = 0;
    audio_param.level = 1.0f;

    sys_semaphore_attribute_t sem_attr;
    memset(&sem_attr, 0, sizeof(sem_attr));
    sys_semaphore_attribute_initialize(sem_attr);
    if (sys_semaphore_create(&audio_ready_sem, &sem_attr, 0, 1) == CELL_OK) {
        audio_ready_sem_created = 1;
    }

    if (cellAudioPortOpen(&audio_param, &audio_port_num) == CELL_OK) {
        audio_port_opened = 1;

        int qret = cellAudioCreateNotifyEventQueue(&audio_event_queue, &audio_queue_key);
        if (qret == CELL_OK) {
            audio_event_queue_created = 1;
            cellAudioSetNotifyEventQueue(audio_queue_key);

        } else {
            printf("[PS3] Error: Failed to create audio event queue (0x%X). Deadlock inevitable.\n", qret);
        }

        ADX_Init();

        audio_thread_running = 1;
        audio_subsystem_ready = 0; /* Will be set by ps3_audio_signal_ready() */

        // Create audio feeder thread
        // A-MED-01 Audit Fix: Priority 150 (below SPURS@100 but more headroom vs main@1001)
        // Prevents audio starvation under heavy edgeZlib decompression load on real HW
        sys_ppu_thread_create(
            &audio_thread_id, audio_thread_entry, 0, 150, 131072, SYS_PPU_THREAD_CREATE_JOINABLE, "ps3_audio_feeder");

        printf("[PS3] Audio port opened and thread spawned (waiting for ready signal)\n");
    }
}

void ps3_audio_quit(void) {
    printf("[PS3] Audio quit...\n");

    // A-01 Audit Fix: Correct shutdown order to avoid race conditions.
    // 1. Signal thread to stop
    // 2. Remove event queue notification (unblocks sys_event_queue_receive)
    // 3. Join thread (now guaranteed to exit)
    // 4. Stop/close port
    // 5. Destroy event queue
    // 6. Quit cellAudio

    // Step 1: Signal stop
    int was_running = audio_thread_running;
    audio_thread_running = 0;

    // Step 2: Remove notification FIRST to unblock the event queue receive
    if (audio_event_queue_created) {
        cellAudioRemoveNotifyEventQueue(audio_queue_key);
    }

    // Step 3: Join thread (the receive call will return error, thread exits)
    if (was_running) {
        uint64_t retval;
        sys_ppu_thread_join(audio_thread_id, &retval);
    }

    // Step 4: Stop and close audio port
    if (audio_port_opened) {
        cellAudioPortStop(audio_port_num);
        cellAudioPortClose(audio_port_num);
        audio_port_opened = 0;
    }

    // Step 5: Destroy event queue and semaphore
    if (audio_event_queue_created) {
        sys_event_queue_destroy(audio_event_queue, 0);
        audio_event_queue_created = 0;
    }
    if (audio_ready_sem_created) {
        sys_semaphore_destroy(audio_ready_sem);
        audio_ready_sem_created = 0;
    }

    // Step 6: Quit cellAudio system
    cellAudioQuit();

    // A-03 Audit Fix: Only unload module if it was successfully loaded
    if (audio_module_loaded) {
        cellSysmoduleUnloadModule(CELL_SYSMODULE_AUDIO);
        audio_module_loaded = 0;
    }
    // A-05: Removed CELL_SYSMODULE_RTC unload (was never used)

    ADX_Exit();
}
