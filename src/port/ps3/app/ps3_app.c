#include "ps3_app.h"
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/spu_initialize.h>
#include <cell/spurs.h>
#include <cell/spurs/lv2_event_queue.h>
#include <sys/event.h>
#include <cell/error.h>
#include "port/ps3/app/ps3_app.h"
#include "port/ps3/renderer/ps3_renderer_gcm.h"
#include "port/ps3/audio/ps3_audio.h"
#include "rendering/game_renderer.h"
#include <cell/pad.h>

#include <sys/process.h>
#include <stdlib.h>
SYS_PROCESS_PARAM(1001, 0x100000)
#include <sysutil/sysutil_common.h>
#include <cell/sysmodule.h>

static bool s_is_running = true;
// NEW-4: System overlay rendering pause (for trophy notifications, dialogs, etc.)
static bool s_system_drawing = false;

static CellSpurs g_spurs;
static bool g_spurs_initialized = false;
static sys_event_queue_t g_spurs_event_queue = 0;
static uint8_t g_spurs_port = 0;
// No global renderer context needed at app level yet

struct CellSpurs* PS3App_GetSpurs(void) {
    return g_spurs_initialized ? &g_spurs : NULL;
}

int PS3App_PreInit(void) {
    printf("[PS3] App PreInit\n");
    return 0;
}

// L-03: EXITGAME2 not in all SDK versions; define if missing (value from SDK 4.x)
#ifndef CELL_SYSUTIL_REQUEST_EXITGAME2
#define CELL_SYSUTIL_REQUEST_EXITGAME2 (0x0104)
#endif

static void sysutil_callback(uint64_t status, uint64_t param, void *userdata) {
    (void)param;
    (void)userdata;
    // L-03 Audit Fix: Handle both EXITGAME and EXITGAME2 as required by SDK
    if (status == CELL_SYSUTIL_REQUEST_EXITGAME ||
        status == CELL_SYSUTIL_REQUEST_EXITGAME2) {
        s_is_running = false;
    }
    // NEW-4: Pause rendering when the OS draws system overlays (trophy popups,
    // XMB notifications, message dialogs). Without this, cellGcmSetClearSurface
    // will wipe the overlay's framebuffer region, making it invisible.
    else if (status == CELL_SYSUTIL_DRAWING_BEGIN) {
        s_system_drawing = true;
    }
    else if (status == CELL_SYSUTIL_DRAWING_END) {
        s_system_drawing = false;
    }
}

int PS3App_FullInit(void) {
    printf("[PS3] App FullInit\n");
    cellSysutilRegisterCallback(0, sysutil_callback, NULL);
    
    // Audit Fix: Load required system modules explicitly (check return per Application Requirements)
    int mod_ret;
    mod_ret = cellSysmoduleLoadModule(CELL_SYSMODULE_GCM);
    if (mod_ret < 0 && mod_ret != CELL_SYSMODULE_ERROR_DUPLICATED) {
        printf("[PS3] FATAL: cellSysmoduleLoadModule(GCM) failed: 0x%x\n", mod_ret);
    }
    mod_ret = cellSysmoduleLoadModule(CELL_SYSMODULE_IO);
    if (mod_ret < 0 && mod_ret != CELL_SYSMODULE_ERROR_DUPLICATED) {
        printf("[PS3] FATAL: cellSysmoduleLoadModule(IO) failed: 0x%x\n", mod_ret);
    }
    // G-LOW-01: Direct libgcm pipeline does not use the RESC library.
    // Removing it prevents internal system blits from interfering with offsets.

    // Audit Fix: Initialize the renderer context BEFORE any SPU systems start firing.
    // This ensures that s_gcm_context is valid for any early memory mapping or texture setup.
    CRS_Renderer_Init();

    // I-MED-01 Audit Fix: Centralize pad initialization here instead of duplicating
    // in SDLPad_Init and tarPADInit. Must happen after CELL_SYSMODULE_IO is loaded.
    {
        int pad_ret = cellPadInit(7);
        if (pad_ret == CELL_PAD_OK || pad_ret == (int)0x80121101 /* ALREADY_INITIALIZED */) {
            // I-MED-02 Audit Fix: Only configure connected ports
            CellPadInfo2 pad_info;
            if (cellPadGetInfo2(&pad_info) == CELL_PAD_OK) {
                for (int p = 0; p < CELL_PAD_MAX_PORT_NUM; p++) {
                    if (pad_info.port_status[p] & CELL_PAD_STATUS_CONNECTED) {
                        cellPadSetPortSetting(p, CELL_PAD_SETTING_PRESS_ON);
                    }
                }
            }
        }
    }

    // S-01 Audit Fix: Only 6 SPUs available to games; was requesting 6+2=8 (fails on real HW)
    int ret = sys_spu_initialize(6, 0);
    if (ret != CELL_OK) {
        printf("[PS3] FATAL: sys_spu_initialize failed: %x\n", ret);
        exit(1);
    } else {

        CellSpursAttribute spursAttr;
        // S-02 Audit Fix: Priority 100 for game workloads (was 250, too low for edgeZlib decompression)
        // S-HIGH-01 Audit Fix: Use 5 SPUs (max for games, 1 reserved for OS hypervisor)
        ret = cellSpursAttributeInitialize(&spursAttr, 5, 100, 8, false);
        if (ret == CELL_OK) {
            ret = cellSpursInitializeWithAttribute(&g_spurs, &spursAttr);
            if (ret == CELL_OK) {
                g_spurs_initialized = true;
                printf("[PS3] SPURS Initialized successfully!\n");

                sys_event_queue_attribute_t eq_attr;
                memset(&eq_attr, 0, sizeof(sys_event_queue_attribute_t));
                sys_event_queue_attribute_initialize(eq_attr);
                
                int eq_ret = sys_event_queue_create(&g_spurs_event_queue, &eq_attr, SYS_EVENT_QUEUE_LOCAL, 32);
                if (eq_ret == CELL_OK) {
                    eq_ret = cellSpursAttachLv2EventQueue(&g_spurs, g_spurs_event_queue, &g_spurs_port, 0);
                    if (eq_ret == CELL_OK) {
                        printf("[PS3] SPURS Event Queue attached on port: %d\n", g_spurs_port);

                        /* NOTE: CELL_ENOTCONN warnings on spup=1 are benign.
                         * They come from the CellSpursKernel's internal trace notification
                         * system (SPU address 0x0ab98) trying to signal on an unbound port.
                         * RPCS3 rejects a second cellSpursAttachLv2EventQueue with
                         * CELL_SPURS_CORE_ERROR_STAT (0x8041070a), so we cannot attach
                         * a queue to port 1. These warnings are harmless and do not affect
                         * SPURS workload execution. */

                        /* Initialize audio after SPURS event queue is attached */
                        ps3_audio_init();

                        {
                            extern void zlib_InitSpurs(void);
                            zlib_InitSpurs();
                        }
                    } else {
                        printf("[PS3] FATAL: cellSpursAttachLv2EventQueue failed: %x\n", eq_ret);
                        exit(1);
                    }
                } else {
                    printf("[PS3] FATAL: sys_event_queue_create failed: %x\n", eq_ret);
                    exit(1);
                }
                

            } else {
                printf("[PS3] FATAL: cellSpursInitializeWithAttribute failed: %x\n", ret);
                exit(1);
            }
        } else {
            printf("[PS3] FATAL: cellSpursAttributeInitialize failed: %x\n", ret);
            exit(1);
        }
    }

    return 0;
}

void PS3App_Quit(void) {
    printf("[PS3] App Quit\n");

    // I-LOW-01 Audit Fix: Properly shut down pad subsystem (Application Requirements)
    cellPadEnd();

    // L-02 Audit Fix: Clean up GCM renderer resources
    // Note: cellGcm does not have a formal teardown API — the RSX context,
    // display buffers, and local memory are reclaimed by the OS at process exit.
    // The host memory from memalign could be freed but is about to be reclaimed anyway.
    
    ps3_audio_quit();

    if (g_spurs_initialized) {
        if (g_spurs_event_queue) {
            cellSpursDetachLv2EventQueue(&g_spurs, g_spurs_port);
            sys_event_queue_destroy(g_spurs_event_queue, 0);
        }
        cellSpursFinalize(&g_spurs);
    }

    cellSysutilUnregisterCallback(0);
}

bool PS3App_PollEvents(void) {
    cellSysutilCheckCallback();
    return s_is_running;
}

bool PS3App_IsSystemDrawing(void) {
    return s_system_drawing;
}

void PS3App_BeginFrame(void) {
    // NEW-4: Skip framebuffer writes during system overlay drawing
    // We MUST call this every frame to reset the render task counters,
    // even if s_system_drawing is true, otherwise the vertex buffer will overflow
    // as the game engine continues to push tasks!
    /* Reset render state for the new frame */
    CRS_Renderer_BeginFrame();
}

void PS3App_EndFrame(void) {
    // NEW-4: During system overlay, still flip to maintain vsync timing
    // but skip the render pass so the OS overlay isn't overwritten
    if (s_system_drawing) {
        CRS_Renderer_EndFrame();
        return;
    }
    /* Flush all queued render tasks to the framebuffer, then swap */
    CRS_Renderer_RenderFrame();
    CRS_Renderer_EndFrame();
}

void PS3App_Exit(void) {
    printf("[PS3] App Exit Requested\n");
    s_is_running = false;
}

bool PS3App_IsFrameRateUncapped(void) {
    return false;
}

bool PS3App_IsVSyncEnabled(void) {
    return true;
}

uint64_t PS3App_GetTargetFrameTimeNS(void) {
    return 16778667; /* 59.59949Hz in nanoseconds */
}
