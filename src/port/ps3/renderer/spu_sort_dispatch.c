/**
 * @file spu_sort_dispatch.c
 * @brief PPU-side dispatch for SPU-accelerated render task sorting.
 *
 * Loads the spu_sort_task ELF (embedded as a binary object by CMake),
 * creates a 1-thread SPU group, and provides an Execute() call that
 * sends render task z-values + indices to the SPU for merge sort.
 *
 * The SPU binary is embedded via ppu-lv2-objcopy and exposed as:
 *   extern char _binary_spu_sort_task_spu_elf_start[];
 *   extern char _binary_spu_sort_task_spu_elf_end[];
 */

#include "spu_sort_dispatch.h"
#include "ps3_renderer_gcm.h"
#include <ppu_intrinsics.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include <sys/spu_thread.h>
#include <sys/spu_thread_group.h>
#include <sys/spu_image.h>
#include <sys/timer.h>
#include <cell/atomic.h>

/* ── SPU binary symbols (injected by ppu-lv2-objcopy) ────────── */
extern char _binary_spu_sort_task_spu_elf_start[];
extern char _binary_spu_sort_task_spu_elf_end[];

/* ── Shared context (must match SortContext in spu_sort_task.c) ── */
typedef struct {
    uint32_t ea_items;    /* EA of SPU_SortItem array */
    uint32_t item_count;  /* Number of items to sort */
    uint32_t run_command; /* 0=idle, 1=sort, 2=exit */
    uint32_t status;      /* 0=done, 1=ready */
} SortContext __attribute__((aligned(128)));

typedef struct {
    float z;
    uint32_t index;
} SPU_SortItem __attribute__((aligned(8)));

/* ── Module state ─────────────────────────────────────────────── */
static SortContext s_ctx __attribute__((aligned(128)));
static SPU_SortItem s_items[RENDER_TASK_MAX] __attribute__((aligned(128)));

static sys_spu_thread_t s_spu_thread;
static sys_spu_thread_group_t s_spu_group;
static sys_spu_image_t s_spu_image;
static bool s_initialized = false;

void SPUSort_Init(void) {
    if (s_initialized)
        return;

    /* Import the embedded SPU ELF image */
    uint32_t elf_size = (uint32_t)(_binary_spu_sort_task_spu_elf_end - _binary_spu_sort_task_spu_elf_start);
    int ret = sys_spu_image_import(
        &s_spu_image, _binary_spu_sort_task_spu_elf_start, SYS_SPU_IMAGE_PROTECT | SYS_SPU_IMAGE_DIRECT);
    if (ret != 0) {
        printf("[SPUSort] FATAL: sys_spu_image_import failed: 0x%x (size=%u)\n", ret, elf_size);
        return;
    }

    /* Create a 1-thread SPU group for sorting */
    sys_spu_thread_group_attribute_t grp_attr;
    sys_spu_thread_group_attribute_initialize(grp_attr);
    sys_spu_thread_group_attribute_name(grp_attr, "spu_sort");

    ret = sys_spu_thread_group_create(&s_spu_group, 1, 200, &grp_attr);
    if (ret != 0) {
        printf("[SPUSort] FATAL: sys_spu_thread_group_create failed: 0x%x\n", ret);
        return;
    }

    /* Initialize the shared context */
    memset(&s_ctx, 0, sizeof(s_ctx));

    sys_spu_thread_attribute_t thr_attr;
    sys_spu_thread_attribute_initialize(thr_attr);
    sys_spu_thread_attribute_name(thr_attr, "sort_thr");

    /* Pass the EA of s_ctx as arg1 */
    sys_spu_thread_argument_t thr_arg;
    memset(&thr_arg, 0, sizeof(thr_arg));
    thr_arg.arg1 = (uint64_t)(uintptr_t)&s_ctx;

    ret = sys_spu_thread_initialize(&s_spu_thread, s_spu_group, 0, &s_spu_image, &thr_attr, &thr_arg);
    if (ret != 0) {
        printf("[SPUSort] FATAL: sys_spu_thread_initialize failed: 0x%x\n", ret);
        sys_spu_thread_group_destroy(s_spu_group);
        return;
    }

    /* Start the SPU group — the SPU will enter its polling loop */
    ret = sys_spu_thread_group_start(s_spu_group);
    if (ret != 0) {
        printf("[SPUSort] FATAL: sys_spu_thread_group_start failed: 0x%x\n", ret);
        sys_spu_thread_group_destroy(s_spu_group);
        return;
    }

    /* Wait until the SPU signals it's ready (status = 1) */
    int timeout = 0;
    while (s_ctx.status != 1 && timeout < 1000) {
        sys_timer_usleep(100);
        timeout++;
    }

    if (s_ctx.status != 1) {
        printf("[SPUSort] WARNING: SPU did not signal ready after 100ms\n");
        /* Continue anyway — Execute will fall back to PPU */
    } else {
        printf("[SPUSort] SPU sort thread ready\n");
    }

    s_initialized = true;
}

void SPUSort_Shutdown(void) {
    if (!s_initialized)
        return;

    /* Signal the SPU to exit */
    s_ctx.run_command = 2;
    __lwsync();

    /* Wait for the thread group to finish */
    int cause, status;
    sys_spu_thread_group_join(s_spu_group, &cause, &status);
    sys_spu_thread_group_destroy(s_spu_group);
    sys_spu_image_close(&s_spu_image);

    s_initialized = false;
    printf("[SPUSort] Shutdown complete\n");
}

bool SPUSort_Execute(GcmRenderTask* tasks, int task_count) {
    if (!s_initialized || task_count <= 1)
        return false;
    if (task_count > RENDER_TASK_MAX)
        task_count = RENDER_TASK_MAX;

    /* Check that SPU is idle (status == 0 means last sort completed) */
    if (s_ctx.status != 0 && s_ctx.run_command != 0) {
        /* SPU still busy from a previous sort — fall back to PPU */
        return false;
    }

    /* Pack z + original_index into the staging buffer */
    for (int i = 0; i < task_count; i++) {
        s_items[i].z = tasks[i].z;
        s_items[i].index = (uint32_t)tasks[i].original_index;
    }

    /* Setup the context and signal the SPU */
    s_ctx.ea_items = (uint32_t)(uintptr_t)s_items;
    s_ctx.item_count = (uint32_t)task_count;
    __lwsync(); /* Ensure items and context fields are visible */
    s_ctx.run_command = 1;
    __lwsync(); /* Ensure run_command is visible to SPU */

    /* Poll for completion with a bounded timeout (10ms max) */
    int watchdog = 0;
    while (s_ctx.run_command != 0) {
        sys_timer_usleep(50);
        if (++watchdog > 200) {
            printf("[SPUSort] WARNING: SPU sort timed out after 10ms, falling back to PPU\n");
            s_ctx.run_command = 0; /* Reset so SPU doesn't stale-process */
            return false;
        }
    }

    /* Copy sorted indices back to render_tasks.
     * After sorting, s_items[] is in sorted z-order.
     * We need to reorder render_tasks to match without losing texture_handles. */
    static GcmRenderTask temp_tasks[RENDER_TASK_MAX];
    for (int i = 0; i < task_count; i++) {
        int orig_idx = (int)s_items[i].index;
        temp_tasks[i] = tasks[orig_idx];
    }
    for (int i = 0; i < task_count; i++) {
        tasks[i] = temp_tasks[i];
    }

    return true;
}
