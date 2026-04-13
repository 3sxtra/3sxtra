#include "ps3_renderer_gcm.h"
#include <cell/gcm.h>
#include "port/ps3/app/ps3_app.h"
#include <Cg/cg.h>
#include <sys/timer.h>
#include <sys/memory.h>
#include <sysutil/sysutil_sysparam.h>
#include <sys/spu_thread.h>
#include <sys/spu_thread_group.h>
#include <sysutil/sysutil_common.h>
#include <sys/spu_image.h>
#include <sys/synchronization.h>
#include "spu_sort_dispatch.h"

// G-11 Audit Fix: Label-based GPU sync replaces cellGcmFinish stall
#define LABEL_INDEX_FRAME_FENCE 64

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

// Defines the authoritative color format passed down from the engine into the rendering pipeline.
// The 3SX engine provides ARGB (0xAARRGGBB), but the RSX hardware expects RGBA (0xRRGGBBAA) 
// for the UB256 vertex attribute.
#define CPS3_NATIVE_COLOR_FORMAT_ARGB

#include "shaders/vpshader.vpo.h"
#include "shaders/fpshader.fpo.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "sf33rd/AcrSDK/ps2/flps2etc.h"
#include "sf33rd/AcrSDK/ps2/flps2vram.h"

// 2D Primitive Includes
#include "port/rendering/renderer.h"
#include "sf33rd/Source/Common/PPGFile.h"
#include "sf33rd/Source/Game/rendering/aboutspr.h"
#include "structs.h"

#define LO_16_BITS(x) ((x) & 0xFFFF)
#define HI_16_BITS(x) (((x) >> 16) & 0xFFFF)

// Size of the RSX Command Buffer
#define CB_SIZE (1024 * 1024)

// Include PS2 GS format defines if not present
#ifndef SCE_GS_PSMT4
#define SCE_GS_PSMCT32 0
#define SCE_GS_PSMCT24 1
#define SCE_GS_PSMCT16 2
#define SCE_GS_PSMCT16S 10
#define SCE_GS_PSMT8 19
#define SCE_GS_PSMT4 20
#define SCE_GS_PSMT8H 27
#define SCE_GS_PSMT4HL 36
#define SCE_GS_PSMT4HH 44
#endif

// Display properties
static uint32_t display_width = 0;
static uint32_t display_height = 0;
static uint32_t color_pitch = 0;
static uint32_t depth_pitch = 0;
#define BUFFER_COUNT 3
static uint32_t color_offset[BUFFER_COUNT];
static uint32_t depth_offset;

// Buffers
static void* host_addr = NULL;
static volatile uint32_t* s_label_addr = NULL; // Restored label for async sync
static sys_semaphore_t vblank_sem;
static sys_mutex_t tex_pool_mutex;

static void vblank_handler(const uint32_t head) {
    (void)head;
    sys_semaphore_post(vblank_sem, 1);
}

// Rendering State
static int frame_index = 0;
static uint32_t s_frame_labels[BUFFER_COUNT] = { 0, 0, 0 };
static uint32_t s_current_frame_id = 1;
static CellGcmSurface surface;
static CellGcmContextData* s_gcm_context = NULL;
static CGprogram cg_vp;
static CGprogram cg_fp;
static void* vp_ucode = NULL;
static void* fp_ucode = NULL;
static uint32_t fp_offset = 0;
static CGparameter cg_vp_mvp;
static CGparameter cg_fp_is_palettized;
static CGparameter cg_fp_tex_dimensions;

// Texture State Map
typedef struct {
    void* pixels;
    uint32_t offset;
    int w;
    int h;
    float pal_mode;
    CellGcmTexture rsx_texture;
    uint32_t last_mem_handle; // Finding #4: dirty tracking — skip re-upload if handle unchanged
    uint32_t pad[2];
} TextureState;

static TextureState system_textures[CELL_GCM_MAX_TEXTURES];

// Palette State Map
static CellGcmTexture rsx_palettes[CELL_GCM_MAX_PALETTES + 1];
static void* palette_pixels[CELL_GCM_MAX_PALETTES + 1] = { 0 };
static unsigned int current_th = 0;

// Sorting & Vertex Buffer
static GcmRenderTask render_tasks[RENDER_TASK_MAX];
static int render_task_count = 0;
static GcmVertex* vtx_buffer = NULL;
static uint32_t vtx_offset = 0;
static GcmVertex batch_vertices[RENDER_TASK_MAX * 6];
static GcmRenderTask merge_temp[RENDER_TASK_MAX];
// C-04 Audit Fix: Use the extern definition from rendering_stubs.c to avoid ODR violation
extern int g_resolution_scale;

// 2D Primitive Queue
#define RENDER_2D_PRIM_MAX 200
typedef struct {
    Vec3 v[4];
    union {
        u32 color;
        WORK* work;
    } attr;
    u32 type;
    s32 next;
} Render2DPrim;

typedef struct {
    s16 ix1st;
    s16 total;
    Render2DPrim prim[RENDER_2D_PRIM_MAX];
} Render2DQueue;

static Render2DQueue s_Render2DQueue;

static void Renderer_2DQueueInit(void) {
    s_Render2DQueue.ix1st = -1;
    s_Render2DQueue.total = 0;
}

// Matrix
static float mvp[16];

// Utilities
// G-LOW-02 Audit Fix: File-scope tracking for dynamic texture pool calculation (G-HIGH-03)
static uint32_t s_local_mem_allocated = 0x100000;

// NEW-8: This allocator is only safe during single-threaded init.
// All calls occur inside CRS_Renderer_Init which is guarded by a static flag.
// G-LOW-02 Audit Fix: alignment parameter to avoid wasting local memory
static void* local_memory_alloc_init(const uint32_t size, const uint32_t alignment) {
    // Display buffers and CRTC presentation surfaces on PS3 RSX MUST be strictly
    // aligned (preferably 1MB aligned) to prevent hardware/emulator scanout from
    // truncating the address to 0x0 or crashing the Main UI thread on swap.

    // Align the current offset to the requested alignment
    uint32_t mask = alignment - 1;
    s_local_mem_allocated = (s_local_mem_allocated + mask) & ~mask;
    uint32_t aligned_size = (size + mask) & ~mask;

    CellGcmConfig config;
    cellGcmGetConfiguration(&config);

    if (s_local_mem_allocated + aligned_size > config.localSize) {
        printf("[GCM] FATAL: RSX local memory exhausted! Requested %u, allocated %u / %u\n",
               aligned_size,
               s_local_mem_allocated,
               config.localSize);
        return NULL;
    }

    void* ptr = (uint8_t*)config.localAddress + s_local_mem_allocated;
    s_local_mem_allocated += aligned_size;
    memset(ptr, 0, aligned_size);
    return ptr;
}

// ---------------------------------------------------------
// Texture Memory Pool (Segregated Free-List Allocator)
// P5 Audit Fix: O(1) alloc/free via per-size-class free chains.
// Falls back to O(n) first-fit only when the fast path misses.
// ---------------------------------------------------------
#define POOL_MAX_BLOCKS 2048

typedef struct {
    uint32_t offset;
    uint32_t size;
    bool is_free;
    int prev_idx;
    int next_idx;
    int fl_next; /* P5: next in same-class free list (-1 = end) */
} TexMemBlock;

static TexMemBlock tex_pool[POOL_MAX_BLOCKS];
static int free_list_head = -1; /* node allocator free list */
static int mem_list_head = -1;  /* ordered memory block list */
static void* tex_pool_base = NULL;

/* P5: Segregated free-list overlay — 16 size classes (128B .. 4MB, powers of 2) */
#define TEX_FL_NUM_CLASSES 16
#define TEX_FL_MIN_SHIFT 7               /* 2^7 = 128 bytes */
static int fl_heads[TEX_FL_NUM_CLASSES]; /* head block index per class */

/* Map a byte size to a free-list class index (0..15) */
static inline int fl_class(uint32_t size) {
    if (size <= (1u << TEX_FL_MIN_SHIFT))
        return 0;
    /* clz = count leading zeros; class = log2(size) - MIN_SHIFT, capped */
    int log2 = 31 - __builtin_clz(size);
    int cls = log2 - TEX_FL_MIN_SHIFT;
    if (cls >= TEX_FL_NUM_CLASSES)
        cls = TEX_FL_NUM_CLASSES - 1;
    return cls;
}

/* Push a free block onto its size-class free list */
static inline void fl_push(int idx) {
    int cls = fl_class(tex_pool[idx].size);
    tex_pool[idx].fl_next = fl_heads[cls];
    fl_heads[cls] = idx;
}

/* Remove a block from its size-class free list (if present) */
static inline void fl_remove(int idx) {
    int cls = fl_class(tex_pool[idx].size);
    int* p = &fl_heads[cls];
    while (*p != -1) {
        if (*p == idx) {
            *p = tex_pool[idx].fl_next;
            tex_pool[idx].fl_next = -1;
            return;
        }
        p = &tex_pool[*p].fl_next;
    }
}

static void tex_pool_init(void* base, uint32_t size) {
    tex_pool_base = base;
    for (int i = 0; i < POOL_MAX_BLOCKS; i++) {
        tex_pool[i].prev_idx = -1;
        tex_pool[i].next_idx = (i < POOL_MAX_BLOCKS - 1) ? i + 1 : -1;
        tex_pool[i].fl_next = -1;
    }
    free_list_head = 0;

    /* Initialize all free-list class heads to empty */
    for (int c = 0; c < TEX_FL_NUM_CLASSES; c++) {
        fl_heads[c] = -1;
    }

    int head = free_list_head;
    free_list_head = tex_pool[head].next_idx;

    tex_pool[head].offset = 0;
    tex_pool[head].size = size;
    tex_pool[head].is_free = true;
    tex_pool[head].prev_idx = -1;
    tex_pool[head].next_idx = -1;
    tex_pool[head].fl_next = -1;

    mem_list_head = head;
    fl_push(head); /* Add initial block to its class */
}

/* Internal: common split+allocate logic used by both fast and slow paths */
static void* tex_pool_take_block(int curr, uint32_t align_size) {
    fl_remove(curr); /* Remove from class free list before modifying size */

    if (tex_pool[curr].size > align_size && free_list_head != -1) {
        int new_node = free_list_head;
        free_list_head = tex_pool[new_node].next_idx;

        tex_pool[new_node].offset = tex_pool[curr].offset + align_size;
        tex_pool[new_node].size = tex_pool[curr].size - align_size;
        tex_pool[new_node].is_free = true;
        tex_pool[new_node].fl_next = -1;

        tex_pool[new_node].prev_idx = curr;
        tex_pool[new_node].next_idx = tex_pool[curr].next_idx;

        if (tex_pool[curr].next_idx != -1) {
            tex_pool[tex_pool[curr].next_idx].prev_idx = new_node;
        }
        tex_pool[curr].next_idx = new_node;
        tex_pool[curr].size = align_size;

        fl_push(new_node); /* Remainder goes to its class */
    }
    tex_pool[curr].is_free = false;
    return (uint8_t*)tex_pool_base + tex_pool[curr].offset;
}

void* tex_pool_alloc(uint32_t size) {
    sys_mutex_lock(tex_pool_mutex, 0);
    uint32_t align_size = (size + 127) & ~127; // 128 byte align

    /* P5 Fast path: check the matching size class and larger classes */
    int cls = fl_class(align_size);
    for (int c = cls; c < TEX_FL_NUM_CLASSES; c++) {
        int idx = fl_heads[c];
        while (idx != -1) {
            if (tex_pool[idx].is_free && tex_pool[idx].size >= align_size) {
                void* ret = tex_pool_take_block(idx, align_size);
                sys_mutex_unlock(tex_pool_mutex);
                return ret;
            }
            idx = tex_pool[idx].fl_next;
        }
    }

    /* Slow path: full O(n) scan (shouldn't happen in normal operation) */
    int curr = mem_list_head;
    while (curr != -1) {
        if (tex_pool[curr].is_free && tex_pool[curr].size >= align_size) {
            void* ret = tex_pool_take_block(curr, align_size);
            sys_mutex_unlock(tex_pool_mutex);
            return ret;
        }
        curr = tex_pool[curr].next_idx;
    }

    printf("[GCM] ERROR: Texture Pool OOM! Failed to allocate %u bytes.\n", align_size);
    sys_mutex_unlock(tex_pool_mutex);
    return NULL;
}

void tex_pool_free(void* ptr) {
    if (!ptr)
        return;
    sys_mutex_lock(tex_pool_mutex, 0);
    uintptr_t offset_wide = (uintptr_t)((uint8_t*)ptr - (uint8_t*)tex_pool_base);
    uint32_t offset = (uint32_t)offset_wide;

    int curr = mem_list_head;
    while (curr != -1) {
        if (tex_pool[curr].offset == offset && !tex_pool[curr].is_free) {
            tex_pool[curr].is_free = true;

            // Merge with next block if free
            int nxt = tex_pool[curr].next_idx;
            if (nxt != -1 && tex_pool[nxt].is_free) {
                fl_remove(nxt);
                tex_pool[curr].size += tex_pool[nxt].size;
                tex_pool[curr].next_idx = tex_pool[nxt].next_idx;
                if (tex_pool[nxt].next_idx != -1) {
                    tex_pool[tex_pool[nxt].next_idx].prev_idx = curr;
                }
                tex_pool[nxt].next_idx = free_list_head;
                tex_pool[nxt].prev_idx = -1;
                tex_pool[nxt].fl_next = -1;
                free_list_head = nxt;
            }

            // Merge with previous block if free
            int prv = tex_pool[curr].prev_idx;
            if (prv != -1 && tex_pool[prv].is_free) {
                fl_remove(prv);
                tex_pool[prv].size += tex_pool[curr].size;
                tex_pool[prv].next_idx = tex_pool[curr].next_idx;
                if (tex_pool[curr].next_idx != -1) {
                    tex_pool[tex_pool[curr].next_idx].prev_idx = prv;
                }
                tex_pool[curr].next_idx = free_list_head;
                tex_pool[curr].prev_idx = -1;
                tex_pool[curr].fl_next = -1;
                free_list_head = curr;
                curr = prv;
            }

            fl_push(curr);
            sys_mutex_unlock(tex_pool_mutex);
            return;
        }
        curr = tex_pool[curr].next_idx;
    }
    sys_mutex_unlock(tex_pool_mutex);
}

static void build_ortho_matrix(float left, float right, float bottom, float top, float zNear, float zFar) {
    memset(mvp, 0, sizeof(mvp));
    mvp[0] = 2.0f / (right - left);
    mvp[3] = -(right + left) / (right - left);
    mvp[5] = 2.0f / (top - bottom);
    mvp[7] = -(top + bottom) / (top - bottom);
    mvp[10] = -2.0f / (zFar - zNear);
    mvp[11] = -(zFar + zNear) / (zFar - zNear);
    mvp[15] = 1.0f;
}

static void stable_sort_render_tasks(void) {
    const int n = render_task_count;
    if (n <= 1)
        return;

    GcmRenderTask* src = render_tasks;
    GcmRenderTask* dst = merge_temp;

    for (int width = 1; width < n; width *= 2) {
        for (int left = 0; left < n; left += 2 * width) {
            const int mid = left + width;
            int right = left + 2 * width;
            if (mid >= n) {
                memcpy(&dst[left], &src[left], (size_t)(n - left) * sizeof(GcmRenderTask));
                break;
            }
            if (right > n)
                right = n;

            int i = left, j = mid, k = left;
            while (i < mid && j < right) {
                if (src[i].z <= src[j].z) {
                    dst[k++] = src[i++];
                } else {
                    dst[k++] = src[j++];
                }
            }
            while (i < mid)
                dst[k++] = src[i++];
            while (j < right)
                dst[k++] = src[j++];
        }

        GcmRenderTask* tmp = src;
        src = dst;
        dst = tmp;
    }

    if (src != render_tasks) {
        memcpy(render_tasks, merge_temp, (size_t)n * sizeof(GcmRenderTask));
    }
}

static void setup_surface_struct(CellGcmSurface* surf, uint32_t offset) {
    memset(surf, 0, sizeof(CellGcmSurface));
    surf->type = CELL_GCM_SURFACE_PITCH;
    surf->antialias = CELL_GCM_SURFACE_CENTER_1;
    surf->colorFormat = CELL_GCM_SURFACE_A8R8G8B8;
    surf->colorTarget = CELL_GCM_SURFACE_TARGET_0;
    surf->colorLocation[0] = CELL_GCM_LOCATION_LOCAL;
    surf->colorOffset[0] = offset;
    surf->colorPitch[0] = color_pitch;

    // C-11 Audit Fix: Sony RSX driver requires all unused color targets to have a minimum pitch of 64.
    // If these are left as 0, any 2D surface clears (NV3089/M2MF) will crash or NOP.
    // FIX: Point unused targets to the same offset as target 0 to avoid zero offset/bounds warnings.
    for (int i = 1; i < 4; i++) {
        surf->colorLocation[i] = CELL_GCM_LOCATION_LOCAL;
        surf->colorOffset[i] = offset;
        surf->colorPitch[i] = 64;
    }

    surf->depthFormat = CELL_GCM_SURFACE_Z24S8;
    surf->depthLocation = CELL_GCM_LOCATION_LOCAL;
    surf->depthOffset = depth_offset;
    surf->depthPitch = depth_pitch;

    surf->width = display_width;
    surf->height = display_height;
    surf->x = 0;
    surf->y = 0;
}

static void CRS_Renderer_ResetState(void) {
    if (!s_gcm_context)
        return;

    // G-LOW-01: Explicitly disable Multiple Render Targets (MRTs)
    // This is critical to prevent the RSX from trying to write to Target 1-3
    // which may not have valid Vulkan images mapped in the emulator.
    cellGcmSetColorMaskMrt(s_gcm_context, 0);

    // Set viewport & scissor
    float scale[4], offset[4];
    scale[0] = display_width * 0.5f;
    scale[1] = display_height * -0.5f;
    scale[2] = 0.5f;
    scale[3] = 0.0f;
    offset[0] = display_width * 0.5f;
    offset[1] = display_height * 0.5f;
    offset[2] = 0.5f;
    offset[3] = 0.0f;
    cellGcmSetViewport(s_gcm_context, 0, 0, display_width, display_height, 0.0f, 1.0f, scale, offset);
    cellGcmSetScissor(s_gcm_context, 0, 0, display_width, display_height);

    // Default color mask: allow all channels
    cellGcmSetColorMask(s_gcm_context,
                        CELL_GCM_COLOR_MASK_B | CELL_GCM_COLOR_MASK_G | CELL_GCM_COLOR_MASK_R | CELL_GCM_COLOR_MASK_A);

    // Default depth state: disabled for 2D UI
    cellGcmSetDepthMask(s_gcm_context, CELL_GCM_FALSE);
    cellGcmSetDepthTestEnable(s_gcm_context, CELL_GCM_FALSE);
    cellGcmSetCullFaceEnable(s_gcm_context, CELL_GCM_FALSE);

    // Default blend state: standard transparency
    cellGcmSetBlendEnable(s_gcm_context, CELL_GCM_TRUE);
    cellGcmSetBlendEquation(s_gcm_context, CELL_GCM_FUNC_ADD, CELL_GCM_FUNC_ADD);
    cellGcmSetBlendFunc(s_gcm_context,
                        CELL_GCM_SRC_ALPHA,
                        CELL_GCM_ONE_MINUS_SRC_ALPHA,
                        CELL_GCM_SRC_ALPHA,
                        CELL_GCM_ONE_MINUS_SRC_ALPHA);
}

// ---------------------------------------------------------
// Init
// ---------------------------------------------------------
static void dummy_flip_callback(const uint32_t head) {
    (void)head; // Dummy callback for PPU
}

void CRS_Renderer_UpdateTexture(int textureId, const void* data, int x, int y, int width, int height) {
    (void)data;
    (void)x;
    (void)y;
    (void)width;
    (void)height;
    uint32_t th = (uint32_t)textureId;
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;
    const int tex_handle = LO_16_BITS(th);
    if (tex_handle < 1 || tex_handle > FL_TEXTURE_MAX)
        return;
    int tex_idx = tex_handle - 1;

    assert(tex_idx < CELL_GCM_MAX_TEXTURES);
    // tex_pool_free acquires tex_pool_mutex internally
    if (system_textures[tex_idx].pixels) {
        tex_pool_free(system_textures[tex_idx].pixels);
        system_textures[tex_idx].pixels = NULL;
        system_textures[tex_idx].last_mem_handle = 0; // Fix: Reset handle so SetTexture re-uploads
    }
}

void CRS_Renderer_Init(void) {
    static int initialized = 0;
    if (initialized)
        return;
    initialized = 1;

    sys_semaphore_attribute_t sem_attr;
    sys_semaphore_attribute_initialize(sem_attr);
    // M-07 Audit Fix: max_count=2 prevents losing a VBlank signal under heavy CPU load
    sys_semaphore_create(&vblank_sem, &sem_attr, 0, 2);

    sys_mutex_attribute_t mut_attr;
    sys_mutex_attribute_initialize(mut_attr);
    sys_mutex_create(&tex_pool_mutex, &mut_attr);

    printf("[GCM] Initializing libgcm...\n");

// 1. Initialize GCM
// The PS3 command buffer is an infinite, continuous FIFO stream managed by
// libgcm internally. We allocate a ring buffer via cellGcmInit and NEVER
// manually reset context->current/begin/end. When the buffer nears capacity,
// libgcm's inline macros automatically inject a JUMP to wrap the GPU back
// to context->begin without breaking the PUT/GET relationship.
#define GCM_CB_SIZE (1 * 1024 * 1024)
#define GCM_HOST_SIZE (2 * 1024 * 1024)
    host_addr = memalign(1024 * 1024, GCM_HOST_SIZE);

    // Clear the memory to prevent emulator RSX parsers from interpreting garbage as commands (e.g. call 0x0)
    memset(host_addr, 0, GCM_HOST_SIZE);

    int init_ret = cellGcmInit(GCM_CB_SIZE, GCM_HOST_SIZE, host_addr);
    s_gcm_context = gCellGcmCurrentContext;
    printf("[GCM] cellGcmInit returned: %d, context->begin: %p, context->end: %p\n",
           init_ret,
           (void*)s_gcm_context->begin,
           (void*)s_gcm_context->end);

    // G-11 Audit Fix: Setup label for async polling to prevent PPU thread starvation
    s_label_addr = cellGcmGetLabelAddress(LABEL_INDEX_FRAME_FENCE);
    *s_label_addr = 0;
    // SYS-MED-02 Audit Fix: Check display capabilities before configuring
    // Required by Application Requirements for real hardware certification
    uint32_t active_resolution_id;
    if (cellVideoOutGetResolutionAvailability(
            CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_720, CELL_VIDEO_OUT_ASPECT_AUTO, 0)) {
        display_width = 1280;
        display_height = 720;
        active_resolution_id = CELL_VIDEO_OUT_RESOLUTION_720;
    } else if (cellVideoOutGetResolutionAvailability(
                   CELL_VIDEO_OUT_PRIMARY, CELL_VIDEO_OUT_RESOLUTION_480, CELL_VIDEO_OUT_ASPECT_AUTO, 0)) {
        display_width = 720;
        display_height = 480;
        active_resolution_id = CELL_VIDEO_OUT_RESOLUTION_480;
    } else {
        // Ultimate fallback: 576i for PAL regions
        display_width = 720;
        display_height = 576;
        active_resolution_id = CELL_VIDEO_OUT_RESOLUTION_576;
    }
    printf("[GCM] Selected resolution: %ux%u\n", display_width, display_height);

    CellVideoOutConfiguration videocfg;
    memset(&videocfg, 0, sizeof(CellVideoOutConfiguration));
    videocfg.resolutionId = active_resolution_id;
    videocfg.format = CELL_VIDEO_OUT_BUFFER_COLOR_FORMAT_X8R8G8B8;
    videocfg.pitch = display_width * 4;
    int32_t vret = cellVideoOutConfigure(CELL_VIDEO_OUT_PRIMARY, &videocfg, NULL, 0);
    if (vret != 0) {
        printf("[GCM] Warning: cellVideoOutConfigure returned %x. Resc may still proceed if emulated.\n", vret);
    }

    CellVideoOutState videoState;
    cellVideoOutGetState(CELL_VIDEO_OUT_PRIMARY, 0, &videoState);
    // 3. Setup Surface & Buffers in Local Memory (1MB alignment for display/RPCS3 compat)
    color_pitch = display_width * 4;
    depth_pitch = display_width * 4;

    void* color_buf_0 = local_memory_alloc_init(color_pitch * display_height, 0x100000);
    void* color_buf_1 = local_memory_alloc_init(color_pitch * display_height, 0x100000);
    void* color_buf_2 = local_memory_alloc_init(color_pitch * display_height, 0x100000);
    // G-LOW-02: Depth buffer only needs 64KB alignment per SDK
    void* depth_buf = local_memory_alloc_init(depth_pitch * display_height, 0x10000);

    cellGcmAddressToOffset(color_buf_0, &color_offset[0]);
    cellGcmAddressToOffset(color_buf_1, &color_offset[1]);
    cellGcmAddressToOffset(color_buf_2, &color_offset[2]);
    cellGcmAddressToOffset(depth_buf, &depth_offset);

    // Diagnostic: Verify display buffers are in non-overlapping local memory regions
    // (NOT in IO/host space where the command buffer slices live)
    printf("[GCM] Display Buffer 0: local offset=0x%x, size=0x%x\n", color_offset[0], color_pitch * display_height);
    printf("[GCM] Display Buffer 1: local offset=0x%x, size=0x%x\n", color_offset[1], color_pitch * display_height);
    printf("[GCM] Display Buffer 2: local offset=0x%x, size=0x%x\n", color_offset[2], color_pitch * display_height);
    printf("[GCM] Depth Buffer:     local offset=0x%x, size=0x%x\n", depth_offset, depth_pitch * display_height);

    // We are deliberately skipping cellResc. The PS3's cellResc library performs
    // an internal blit right before flipping. Because we did not fully configure
    // physical display sub-rectangles, it performed a 0x0 size blit, destroying the FIFO queue.
    // Since our game renders natively at our chosen resolution (e.g. 720p), scaling is unnecessary.
    // We register the display buffers natively through libgcm!

    cellGcmSetDisplayBuffer(0, color_offset[0], color_pitch, display_width, display_height);
    cellGcmSetDisplayBuffer(1, color_offset[1], color_pitch, display_width, display_height);
    cellGcmSetDisplayBuffer(2, color_offset[2], color_pitch, display_width, display_height);

    // We must reset the flip status, enable vsync, and provide a dummy flip handler.
    cellGcmResetFlipStatus();
    cellGcmSetFlipMode(CELL_GCM_DISPLAY_VSYNC);

    // cellGcmSetFlipHandler sets a PPU callback that runs during the flip interrupt.
    cellGcmSetFlipHandler(dummy_flip_callback);
    cellGcmSetVBlankHandler(vblank_handler);
    cellGcmSetGraphicsHandler(dummy_flip_callback);
    cellGcmSetQueueHandler(dummy_flip_callback);

    // 3. Setup initial surface
    setup_surface_struct(&surface, color_offset[0]);
    cellGcmSetSurface(s_gcm_context, &surface);
    CRS_Renderer_ResetState();

    // 4. Initialize shaders
    cg_vp = (CGprogram)vpshader_vpo;
    cg_fp = (CGprogram)fpshader_fpo;

    cellGcmCgInitProgram(cg_vp);
    cellGcmCgInitProgram(cg_fp);

    // Allocate space for the fragment shader ucode in high performance RSX memory
    uint32_t vp_size;
    cellGcmCgGetUCode(cg_vp, &vp_ucode, &vp_size);
    uint32_t fp_size;
    void* fp_src;
    cellGcmCgGetUCode(cg_fp, &fp_src, &fp_size);

    // G-LOW-02: Fragment shader ucode needs 64-byte alignment per SDK
    fp_ucode = local_memory_alloc_init(fp_size, 64);
    memcpy(fp_ucode, fp_src, fp_size);
    cellGcmAddressToOffset(fp_ucode, &fp_offset);

    cg_vp_mvp = cellGcmCgGetNamedParameter(cg_vp, "mvp");
    cg_fp_is_palettized = cellGcmCgGetNamedParameter(cg_fp, "is_palettized");
    cg_fp_tex_dimensions = cellGcmCgGetNamedParameter(cg_fp, "tex_dimensions");

    // No Vsync Semaphore setup needed - handled via CPU reference syncing

    // G-LOW-02: Vertex data needs 16-byte alignment per SDK
    vtx_buffer = (GcmVertex*)local_memory_alloc_init(sizeof(GcmVertex) * RENDER_TASK_MAX * 6 * BUFFER_COUNT, 16);
    cellGcmAddressToOffset(vtx_buffer, &vtx_offset);

    // G-HIGH-03 Audit Fix: Calculate remaining RSX local memory dynamically
    // instead of blindly requesting 128MB which may exceed what's available
    CellGcmConfig tex_config;
    cellGcmGetConfiguration(&tex_config);
    uint32_t remaining = tex_config.localSize - s_local_mem_allocated;
    // Reserve 1MB headroom for any future small allocations
    uint32_t tex_pool_size = (remaining > (1 * 1024 * 1024)) ? (remaining - (1 * 1024 * 1024)) : 0;
    if (tex_pool_size == 0) {
        printf("[GCM] FATAL: No RSX local memory remaining for texture pool!\n");
    }
    printf("[GCM] Texture pool: %u MB (of %u MB total local memory)\n",
           tex_pool_size / (1024 * 1024),
           tex_config.localSize / (1024 * 1024));
    // G-LOW-02: Texture data needs 128-byte alignment per SDK
    void* tex_pool_mem = local_memory_alloc_init(tex_pool_size, 128);
    tex_pool_init(tex_pool_mem, tex_pool_size);

    // G-04 Audit Fix: Use ±1.0 Z range for 24-bit depth precision (Z24S8)
    build_ortho_matrix(0.0f, 384.0f, 224.0f, 0.0f, -1.0f, 1.0f);

    Renderer_2DQueueInit();

    // Flush all init commands so the RSX processes them.
    // We do NOT park the RSX or manually manipulate context pointers.
    // The FIFO is a continuous stream — libgcm manages wrapping internally.
    cellGcmFlush(s_gcm_context);

    printf("[GCM] Context created! Res: %dx%d\n", display_width, display_height);
}

// ---------------------------------------------------------
// Frame lifecycle
// ---------------------------------------------------------
void CRS_Renderer_BeginFrame(void) {
#if DEBUG
    printf("[GCM] BeginFrame\n");
#endif

    // Wait for the GPU to finish rendering the frame whose display buffer we're
    // about to reuse. The label is written by cellGcmSetWriteBackEndLabel at the
    // end of each frame — if the GPU hasn't reached it yet, stall the CPU.
    uint32_t wait_for_frame = s_frame_labels[frame_index];
    if (wait_for_frame > 0) {
        // P3 Audit Fix: Bounded watchdog prevents infinite hang on RSX stall
        int rsx_watchdog = 0;
        while (*s_label_addr < wait_for_frame) {
            sys_timer_usleep(100);        // 0.1ms micro-yield to keep OS responsive
            if (++rsx_watchdog > 10000) { // 1 second timeout
                printf(
                    "[GCM] FATAL: RSX hung waiting for frame label %u (current: %u)\n", wait_for_frame, *s_label_addr);
                // Force-advance label to prevent permanent hang — frame will be corrupt
                *s_label_addr = wait_for_frame;
                break;
            }
        }
    }

    // DO NOT reset context->begin/current/end here!
    // The PS3 command buffer is an infinite continuous stream. libgcm manages
    // ring-buffer wrapping internally via its callback mechanism. Manually
    // rewinding the pointers breaks the PUT/GET relationship and kills the FIFO.

    // Use current backbuffer
    setup_surface_struct(&surface, color_offset[frame_index]);
    cellGcmSetSurface(s_gcm_context, &surface);

    // Reset recurring viewport/blend/mask state
    CRS_Renderer_ResetState();

    // Clear screen — always clear regardless of system draw state
    // Force clear color to opaque black (0xFF000000 = A,R,G,B)
    // to prevent green bleed on transparent 8-bit texture layers
    cellGcmSetClearColor(s_gcm_context, 0xFF000000);
    // Explicitly add Z and S clears to ensure depth buffer validity
    cellGcmSetClearSurface(s_gcm_context, CELL_GCM_CLEAR_R | CELL_GCM_CLEAR_G | CELL_GCM_CLEAR_B | CELL_GCM_CLEAR_A | CELL_GCM_CLEAR_Z | CELL_GCM_CLEAR_S);

    // G-10 Audit Fix: Invalidate texture cache once per frame, not per-bind
    cellGcmSetInvalidateTextureCache(s_gcm_context, CELL_GCM_INVALIDATE_TEXTURE);

    render_task_count = 0;
}

void CRS_Renderer_RenderFrame(void) {
    if (render_task_count == 0)
        return;
    {
        static int rf_diag = 0;
        if (rf_diag < 5) {
            printf("[GCM] RenderFrame: %d tasks, z[0]=%f\n", render_task_count, render_tasks[0].z);
            rf_diag++;
        }
    }

    // P1 Audit Fix: Attempt SPU-accelerated sort, fallback to PPU merge sort
    if (!SPUSort_Execute(render_tasks, render_task_count)) {
        stable_sort_render_tasks();
    }

    assert(g_resolution_scale > 0.0f);

    cellGcmSetVertexProgram(s_gcm_context, cg_vp, vp_ucode);
    cellGcmSetVertexProgramParameter(s_gcm_context, cg_vp_mvp, mvp);

    cellGcmSetFragmentProgram(s_gcm_context, cg_fp, fp_offset);

    uint32_t current_vtx_offset = vtx_offset + frame_index * (RENDER_TASK_MAX * 6 * sizeof(GcmVertex));
    cellGcmSetVertexDataArray(s_gcm_context,
                              0,
                              0,
                              sizeof(GcmVertex),
                              3,
                              CELL_GCM_VERTEX_F,
                              CELL_GCM_LOCATION_LOCAL,
                              current_vtx_offset + offsetof(GcmVertex, x));
    cellGcmSetVertexDataArray(s_gcm_context,
                              1,
                              0,
                              sizeof(GcmVertex),
                              2,
                              CELL_GCM_VERTEX_F,
                              CELL_GCM_LOCATION_LOCAL,
                              current_vtx_offset + offsetof(GcmVertex, u));
    cellGcmSetVertexDataArray(s_gcm_context,
                              2,
                              0,
                              sizeof(GcmVertex),
                              4,
                              CELL_GCM_VERTEX_UB256,
                              CELL_GCM_LOCATION_LOCAL,
                              current_vtx_offset + offsetof(GcmVertex, color));

    cellGcmSetInvalidateVertexCache(s_gcm_context);
    cellGcmSetDepthTestEnable(s_gcm_context, CELL_GCM_FALSE);

    // C-01 Audit Fix: Pack all vertex data contiguously into the RSX buffer.
    // After sorting, original_index maps back to batch_vertices[]. We copy
    // ALL tasks' vertices into vtx_buffer in sorted order FIRST, then
    // issue batched draw calls by texture handle.
    int frame_vtx_idx = frame_index * (RENDER_TASK_MAX * 6);
    for (int i = 0; i < render_task_count; i++) {
        GcmRenderTask* task = &render_tasks[i];
        memcpy(&vtx_buffer[frame_vtx_idx + i * 6], &batch_vertices[task->original_index * 6], sizeof(GcmVertex) * 6);
    }

    // C-02 Audit Fix: Rewritten batch loop. We track batch_start and flush
    // whenever the texture handle changes OR we reach the last task.
    // The i > 0 guard that prevented single-task scenes from drawing is removed.
    uint32_t current_texture_handle = render_tasks[0].texture_handle;
    int batch_start = 0;

    for (int i = 1; i <= render_task_count; i++) {
        uint32_t tex_handle = (i < render_task_count) ? render_tasks[i].texture_handle : ~current_texture_handle;

        if (tex_handle != current_texture_handle) {
            // Flush batch [batch_start, i)
            int batch_count = i - batch_start;
            int vtx_count = batch_count * 6;

            // Setup texture state for this batch
            int tex_id = LO_16_BITS(current_texture_handle) - 1;
            int pal_id = HI_16_BITS(current_texture_handle) - 1;
            TextureState* tstate = (tex_id >= 0 && tex_id < CELL_GCM_MAX_TEXTURES) ? &system_textures[tex_id] : NULL;
            bool textured = (tstate != NULL && tstate->pixels != NULL);

            if (textured) {
                cellGcmSetTexture(s_gcm_context, 0, &tstate->rsx_texture);
                cellGcmSetTextureControl(s_gcm_context, 0, CELL_GCM_TRUE, 0, 0, 0);
                // M-02 Audit Fix: Use CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX (standard) instead of CONVOLUTION_MIN
                cellGcmSetTextureFilter(s_gcm_context,
                                        0,
                                        0,
                                        CELL_GCM_TEXTURE_NEAREST,
                                        CELL_GCM_TEXTURE_NEAREST,
                                        CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
                cellGcmSetTextureAddress(s_gcm_context,
                                         0,
                                         CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                         CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                         CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                         CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,
                                         CELL_GCM_TEXTURE_ZFUNC_LESS,
                                         0);

                if (pal_id >= 0 && palette_pixels[pal_id]) {
                    cellGcmSetTexture(s_gcm_context, 1, &rsx_palettes[pal_id]);
                    cellGcmSetTextureControl(s_gcm_context, 1, CELL_GCM_TRUE, 0, 0, 0);
                    cellGcmSetTextureFilter(s_gcm_context,
                                            1,
                                            0,
                                            CELL_GCM_TEXTURE_NEAREST,
                                            CELL_GCM_TEXTURE_NEAREST,
                                            CELL_GCM_TEXTURE_CONVOLUTION_QUINCUNX);
                    cellGcmSetTextureAddress(s_gcm_context,
                                             1,
                                             CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                             CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                             CELL_GCM_TEXTURE_CLAMP_TO_EDGE,
                                             CELL_GCM_TEXTURE_UNSIGNED_REMAP_NORMAL,
                                             CELL_GCM_TEXTURE_ZFUNC_LESS,
                                             0);

                    float is_pal[4] = { tstate->pal_mode, 0.0f, 0.0f, 0.0f };
                    cellGcmSetFragmentProgramParameter(s_gcm_context, cg_fp, cg_fp_is_palettized, is_pal, fp_offset);

                    float t_dim[4] = { (float)tstate->w, (float)tstate->h, 0.0f, 0.0f };
                    cellGcmSetFragmentProgramParameter(s_gcm_context, cg_fp, cg_fp_tex_dimensions, t_dim, fp_offset);
                } else {
                    cellGcmSetTextureControl(s_gcm_context, 1, CELL_GCM_FALSE, 0, 0, 0);
                    float is_pal[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
                    cellGcmSetFragmentProgramParameter(s_gcm_context, cg_fp, cg_fp_is_palettized, is_pal, fp_offset);
                }
            } else {
                cellGcmSetTextureControl(s_gcm_context, 0, CELL_GCM_FALSE, 0, 0, 0);
                cellGcmSetTextureControl(s_gcm_context, 1, CELL_GCM_FALSE, 0, 0, 0);
                float is_pal[4] = { -1.0f, 0.0f, 0.0f, 0.0f };
                cellGcmSetFragmentProgramParameter(s_gcm_context, cg_fp, cg_fp_is_palettized, is_pal, fp_offset);
            }

            // C-01 Audit Fix: Draw using contiguous vertex indices (vertices are now packed in sorted order)
            {
                static int draw_diag = 0;
                if (draw_diag < 10) {
                    printf("[GCM] Draw: tex_id=%d pal_id=%d textured=%d batch_start=%d vtx_count=%d is_pal=%.1f\n",
                           tex_id,
                           pal_id,
                           textured ? 1 : 0,
                           batch_start,
                           vtx_count,
                           textured ? tstate->pal_mode : -1.0f);
                    // Log first vertex of this batch
                    int vi = frame_index * (RENDER_TASK_MAX * 6) + batch_start * 6;
                    printf("[GCM]   vtx[0]: x=%.1f y=%.1f z=%.1f u=%.2f v=%.2f color=0x%08X\n",
                           vtx_buffer[vi].x,
                           vtx_buffer[vi].y,
                           vtx_buffer[vi].z,
                           vtx_buffer[vi].u,
                           vtx_buffer[vi].v,
                           vtx_buffer[vi].color);
                    draw_diag++;
                }
            }
            cellGcmSetDrawArrays(s_gcm_context, CELL_GCM_PRIMITIVE_TRIANGLES, batch_start * 6, vtx_count);
            batch_start = i;
            current_texture_handle = tex_handle;
        }
    }
}

void CRS_Renderer_EndFrame(void) {
    {
        static int ef_diag = 0;
        if (ef_diag < 5) {
            printf("[GCM] EndFrame: frame_index=%d, frame_id=%u\n", frame_index, s_current_frame_id);
            ef_diag++;
        }
    }

    // Issue flip for the current display buffer
    cellGcmSetFlip(s_gcm_context, frame_index);

    // Write frame fence label so the CPU can detect when the GPU finishes this frame.
    // BeginFrame polls this label to prevent overwriting in-flight display buffers.
    cellGcmSetWriteBackEndLabel(s_gcm_context, LABEL_INDEX_FRAME_FENCE, s_current_frame_id);
    s_frame_labels[frame_index] = s_current_frame_id;

    // Flush the command stream to advance PUT so the RSX sees all our commands.
    cellGcmFlush(s_gcm_context);
    cellGcmSetWaitFlip(s_gcm_context); // SDK pattern: stall RSX until flip occurs

    // Wait for the flip we just issued to be acknowledged by the display controller.
    // This is placed AFTER the flush so the RSX can actually execute the flip command.
    // On the first call, cellGcmResetFlipStatus() was already called during init,
    // so the first flip will be accepted immediately.
    while (cellGcmGetFlipStatus() != 0) {
        sys_timer_usleep(200);
    }
    cellGcmResetFlipStatus();

    frame_index = (frame_index + 1) % BUFFER_COUNT;
    s_current_frame_id++;

    // Check OS callbacks every frame before VSync stall to guarantee exit signals are processed
    cellSysutilCheckCallback();

    // Pace main logic tick to exactly 60Hz. If a VBlank already passed, this immediately consumes it.
    sys_semaphore_wait(vblank_sem, 0);
}

// ---------------------------------------------------------
// Texture and Resource Management
// ---------------------------------------------------------

static const uint8_t s_5to8[32] = { 0,   8,   16,  25,  33,  41,  49,  58,  66,  74,  82,  90,  99,  107, 115, 123,
                                    132, 140, 148, 156, 165, 173, 181, 189, 197, 206, 214, 222, 230, 239, 247, 255 };

static const uint8_t ps2_clut_shuffle[256] = {
    0,   1,   2,   3,   4,   5,   6,   7,   16,  17,  18,  19,  20,  21,  22,  23,  8,   9,   10,  11,  12,  13,
    14,  15,  24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  48,  49,  50,  51,
    52,  53,  54,  55,  40,  41,  42,  43,  44,  45,  46,  47,  56,  57,  58,  59,  60,  61,  62,  63,  64,  65,
    66,  67,  68,  69,  70,  71,  80,  81,  82,  83,  84,  85,  86,  87,  72,  73,  74,  75,  76,  77,  78,  79,
    88,  89,  90,  91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 112, 113, 114, 115, 116, 117,
    118, 119, 104, 105, 106, 107, 108, 109, 110, 111, 120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
    132, 133, 134, 135, 144, 145, 146, 147, 148, 149, 150, 151, 136, 137, 138, 139, 140, 141, 142, 143, 152, 153,
    154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164, 165, 166, 167, 176, 177, 178, 179, 180, 181, 182, 183,
    168, 169, 170, 171, 172, 173, 174, 175, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194, 195, 196, 197,
    198, 199, 208, 209, 210, 211, 212, 213, 214, 215, 200, 201, 202, 203, 204, 205, 206, 207, 216, 217, 218, 219,
    220, 221, 222, 223, 224, 225, 226, 227, 228, 229, 230, 231, 240, 241, 242, 243, 244, 245, 246, 247, 232, 233,
    234, 235, 236, 237, 238, 239, 248, 249, 250, 251, 252, 253, 254, 255
};

static void read_rgba16_color(const uint8_t* p, uint8_t* out) {
    // 16-bit PSMCT16 from flPS2ConvertContext (LE order):
    // p holds bits 0-7 = R + partial G, bits 8-15 = partial G + B + A
    uint16_t pixel = p[0] | (p[1] << 8);   // Read as little-endian
    out[0] = s_5to8[pixel & 0x1F];         // R (bits 0-4)
    out[1] = s_5to8[(pixel >> 5) & 0x1F];  // G (bits 5-9)
    out[2] = s_5to8[(pixel >> 10) & 0x1F]; // B (bits 10-14)
    out[3] = (pixel & 0x8000) ? 255 : 0;   // A (bit 15)
}

static uint32_t build_argb8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    return (a << 24) | (r << 16) | (g << 8) | b;
}

void CRS_Renderer_CreateTexture(unsigned int th) {}
void CRS_Renderer_DestroyTexture(unsigned int texture_handle) {
    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX)
        return;
    int tex_idx = texture_handle - 1;
    assert(tex_idx < CELL_GCM_MAX_TEXTURES);
    if (system_textures[tex_idx].pixels) {
        tex_pool_free(system_textures[tex_idx].pixels);
        system_textures[tex_idx].pixels = NULL;
    }
}

void CRS_Renderer_CreatePalette(unsigned int ph) {
    const int palette_index = HI_16_BITS(ph) - 1;
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;

    const FLTexture* fl_palette = &flPalette[palette_index];
    const void* pixels = flPS2GetSystemBuffAdrs(fl_palette->mem_handle);
    if (!pixels)
        return;

    if (!palette_pixels[palette_index]) {
        palette_pixels[palette_index] = tex_pool_alloc(256 * 4);
        if (!palette_pixels[palette_index])
            return;

        CellGcmTexture* ptex = &rsx_palettes[palette_index];
        ptex->format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
        ptex->mipmap = 1;
        ptex->dimension = CELL_GCM_TEXTURE_DIMENSION_2;
        ptex->cubemap = CELL_GCM_FALSE;
        ptex->remap = CELL_GCM_TEXTURE_REMAP_ORDER_XYXY << 16 | CELL_GCM_TEXTURE_REMAP_FROM_A << 14 |
                      CELL_GCM_TEXTURE_REMAP_FROM_R << 12 | CELL_GCM_TEXTURE_REMAP_FROM_G << 10 |
                      CELL_GCM_TEXTURE_REMAP_FROM_B << 8 | CELL_GCM_TEXTURE_REMAP_REMAP << 6 |
                      CELL_GCM_TEXTURE_REMAP_REMAP << 4 | CELL_GCM_TEXTURE_REMAP_REMAP << 2 |
                      CELL_GCM_TEXTURE_REMAP_REMAP;
        ptex->width = 256;
        ptex->height = 1;
        ptex->depth = 1;
        ptex->pitch = 256 * 4;
        ptex->location = CELL_GCM_LOCATION_LOCAL;
        uint32_t offset;
        cellGcmAddressToOffset(palette_pixels[palette_index], &offset);
        ptex->offset = offset;
    }
    uint32_t* pal_cache = (uint32_t*)palette_pixels[palette_index];

    const int color_count = fl_palette->width * fl_palette->height;
    const bool is_16bit = (fl_palette->format == SCE_GS_PSMCT16);

    const uint8_t* raw = (const uint8_t*)pixels;

    if (color_count == 16) {
        for (int i = 0; i < 16; i++) {
            if (is_16bit) {
                uint8_t out[4] = { 0 };
                read_rgba16_color(&raw[i * 2], out);
                pal_cache[i] = build_argb8888(out[0], out[1], out[2], out[3]);
            } else {
                // PS2 GS PSMCT32 bytes in memory: [R, G, B, A]
                const uint8_t* p = raw + (i * 4);
                uint8_t a = (p[3] >= 128) ? 255 : (p[3] * 2); // A is byte 3
                pal_cache[i] = build_argb8888(p[0], p[1], p[2], a); // ARGB8888 for RSX
            }
        }
        memset(&pal_cache[16], 0, (256 - 16) * 4); // clear to safe black
    } else if (color_count == 256) {
        for (int i = 0; i < 256; i++) {
            int src_idx = ps2_clut_shuffle[i];
            if (is_16bit) {
                uint8_t out[4] = { 0 };
                read_rgba16_color(&raw[src_idx * 2], out);
                pal_cache[i] = build_argb8888(out[0], out[1], out[2], out[3]);
            } else {
                // PS2 GS PSMCT32 bytes in memory: [R, G, B, A]
                const uint8_t* p = raw + (src_idx * 4);
                uint8_t a = (p[3] >= 128) ? 255 : (p[3] * 2); // A is byte 3
                pal_cache[i] = build_argb8888(p[0], p[1], p[2], a); // ARGB8888 for RSX
            }
        }
    }
}

void CRS_Renderer_DestroyPalette(unsigned int palette_handle) {
    // H-01 Audit Fix: Use HI_16_BITS to match CreatePalette's encoding convention.
    // If palette_handle has the index in the low bits (raw handle), extract it directly.
    // If it has it in the high bits (encoded ph), extract from high bits.
    int palette_index;
    if (palette_handle > 0xFFFF) {
        palette_index = HI_16_BITS(palette_handle) - 1;
    } else {
        palette_index = (int)palette_handle - 1;
    }
    if (palette_index < 0 || palette_index >= FL_PALETTE_MAX)
        return;
    if (palette_pixels[palette_index]) {
        tex_pool_free(palette_pixels[palette_index]);
        palette_pixels[palette_index] = NULL;
    }
}

void CRS_Renderer_UnlockTexture(unsigned int th) {
    const int tex_handle = LO_16_BITS(th);
    if (tex_handle < 1 || tex_handle > FL_TEXTURE_MAX) return;
    int tex_idx = tex_handle - 1;
    assert(tex_idx < CELL_GCM_MAX_TEXTURES);
    system_textures[tex_idx].last_mem_handle = 0; // Force re-upload on next SetTexture
}
void CRS_Renderer_UnlockPalette(unsigned int ph) {
    CRS_Renderer_DestroyPalette(ph);
    if (ph <= 0xFFFF) ph = ph << 16;
    CRS_Renderer_CreatePalette(ph);
}

void CRS_Renderer_SetTexture(unsigned int th) {
    if ((th & 0xFFFF) == 0)
        th = (th & 0xFFFF0000) | 1000;

    const int texture_handle = LO_16_BITS(th);
    const int palette_handle = HI_16_BITS(th);

    current_th = th;

    if (texture_handle < 1 || texture_handle > FL_TEXTURE_MAX)
        return;

    const int tex_idx = texture_handle - 1;

    if (palette_handle > 0 && !palette_pixels[palette_handle - 1]) {
        CRS_Renderer_CreatePalette(palette_handle << 16);
    }

    const FLTexture* fl_texture = &flTexture[tex_idx];
    uint8_t* raw = (uint8_t*)flPS2GetSystemBuffAdrs(fl_texture->mem_handle);
    if (!raw)
        return;

    int pixels = fl_texture->width * fl_texture->height;
    if (pixels == 0)
        return;

    assert(tex_idx < CELL_GCM_MAX_TEXTURES);
    TextureState* tstate = &system_textures[tex_idx];

    // Finding #1: PSMCT32 (0) and PSMCT24 (1) are direct-color formats, same as PSMCT16 (2)
    const bool is_color = (fl_texture->format == SCE_GS_PSMCT16 || fl_texture->format == SCE_GS_PSMCT32 ||
                           fl_texture->format == SCE_GS_PSMCT24);

    if (!tstate->pixels) {
        uint32_t bpp = is_color ? 4 : 1;
        uint32_t raw_data_width = (fl_texture->format == SCE_GS_PSMT4) ? (fl_texture->width / 2) : fl_texture->width;
        uint32_t raw_pitch = raw_data_width * bpp;
        uint32_t aligned_pitch = (raw_pitch + 63) & ~63;

        // Allocate space in local memory via formal allocator
        tstate->pixels = tex_pool_alloc(aligned_pitch * fl_texture->height);

        if (!tstate->pixels)
            return; // OOM safety

        cellGcmAddressToOffset(tstate->pixels, &tstate->offset);

        CellGcmTexture* tex = &tstate->rsx_texture;
        if (is_color) {
            // H-04 Reconsidered: Keep textures normalized (no NR flag). The fragment shader
            // expects tex0 in 0.0-1.0 range. Palette shader recovers bytes via *255.0.
            // The msg_rsx_texture using NR is the odd one out (fixed in H-02).
            tex->format = CELL_GCM_TEXTURE_A8R8G8B8 | CELL_GCM_TEXTURE_LN;
            tex->remap = CELL_GCM_TEXTURE_REMAP_ORDER_XYXY << 16 | CELL_GCM_TEXTURE_REMAP_FROM_A << 14 |
                         CELL_GCM_TEXTURE_REMAP_FROM_R << 12 | CELL_GCM_TEXTURE_REMAP_FROM_G << 10 |
                         CELL_GCM_TEXTURE_REMAP_FROM_B << 8 | CELL_GCM_TEXTURE_REMAP_REMAP << 6 |
                         CELL_GCM_TEXTURE_REMAP_REMAP << 4 | CELL_GCM_TEXTURE_REMAP_REMAP << 2 |
                         CELL_GCM_TEXTURE_REMAP_REMAP;
            tex->pitch = fl_texture->width * 4;
            tex->pitch = (tex->pitch + 63) & ~63;
        } else {
            // Index texture - map native single channel (intensity) directly to A
            tex->format = CELL_GCM_TEXTURE_B8 | CELL_GCM_TEXTURE_LN;
            tex->remap = CELL_GCM_TEXTURE_REMAP_ORDER_XYXY << 16 | CELL_GCM_TEXTURE_REMAP_FROM_B << 14 |
                         CELL_GCM_TEXTURE_REMAP_FROM_B << 12 | CELL_GCM_TEXTURE_REMAP_FROM_B << 10 |
                         CELL_GCM_TEXTURE_REMAP_FROM_B << 8 | CELL_GCM_TEXTURE_REMAP_REMAP << 6 |
                         CELL_GCM_TEXTURE_REMAP_REMAP << 4 | CELL_GCM_TEXTURE_REMAP_REMAP << 2 |
                         CELL_GCM_TEXTURE_REMAP_REMAP;
            tex->pitch = (fl_texture->format == SCE_GS_PSMT4) ? (fl_texture->width / 2) : fl_texture->width;
            tex->pitch = (tex->pitch + 63) & ~63;
            if (tex->pitch == 0)
                tex->pitch = 64; // Fallback to avoid empty regions
        }
        tex->mipmap = 1;
        tex->dimension = CELL_GCM_TEXTURE_DIMENSION_2;
        tex->cubemap = CELL_GCM_FALSE;
        tex->width = (fl_texture->format == SCE_GS_PSMT4) ? (fl_texture->width / 2) : fl_texture->width;
        if (tex->width == 0)
            tex->width = 1;
        tex->height = fl_texture->height;
        if (tex->height == 0)
            tex->height = 1;
        tex->depth = 1;
        tex->location = CELL_GCM_LOCATION_LOCAL;
        tex->offset = tstate->offset;
    }
    
    tstate->w = fl_texture->width;
    tstate->h = fl_texture->height;
    if (fl_texture->format == SCE_GS_PSMT4)
        tstate->pal_mode = 2.0f;
    else if (fl_texture->format == SCE_GS_PSMT8)
        tstate->pal_mode = 1.0f;
    else
        tstate->pal_mode = 0.0f;

    // Finding #4: Skip re-upload if the underlying system memory buffer hasn't changed.
    if (tstate->last_mem_handle == fl_texture->mem_handle && fl_texture->mem_handle != 0) {
        return;
    }
    tstate->last_mem_handle = fl_texture->mem_handle;

    {
        uint8_t* conv = (uint8_t*)tstate->pixels;

        // Decoding — convert PS2 GS pixel formats to RSX-native textures
        switch (fl_texture->format) {
        case SCE_GS_PSMT8:
        case SCE_GS_PSMT4: {
            // Indexed textures — raw byte copy (palette lookup done in fragment shader)
            uint32_t raw_data_width =
                (fl_texture->format == SCE_GS_PSMT4) ? (fl_texture->width / 2) : fl_texture->width;
            uint32_t aligned_pitch = (raw_data_width + 63) & ~63;
            for (int y = 0; y < fl_texture->height; y++) {
                memcpy(conv + (y * aligned_pitch), raw + (y * raw_data_width), raw_data_width);
            }
            break;
        }
        case SCE_GS_PSMCT16: {
            // 16-bit direct color (RGB5A1) → expand to ARGB8888
            uint32_t raw_pitch_16 = fl_texture->width * 2;
            uint32_t aligned_pitch = (fl_texture->width * 4 + 63) & ~63;
            for (int y = 0; y < fl_texture->height; y++) {
                const uint8_t* src_row = raw + (y * raw_pitch_16);
                uint32_t* dst_row = (uint32_t*)(conv + (y * aligned_pitch));
                for (int x = 0; x < fl_texture->width; x++) {
                    uint8_t out[4] = { 0 };
                    read_rgba16_color(&src_row[x * 2], out);
                    dst_row[x] = build_argb8888(out[0], out[1], out[2], out[3]);
                }
            }
            break;
        }
        case SCE_GS_PSMCT32: {
            uint32_t raw_pitch_32 = fl_texture->width * 4;
            uint32_t aligned_pitch = (fl_texture->width * 4 + 63) & ~63;
            for (int y = 0; y < fl_texture->height; y++) {
                const uint8_t* src_row = raw + (y * raw_pitch_32);
                uint32_t* dst_row = (uint32_t*)(conv + (y * aligned_pitch));
                for (int x = 0; x < fl_texture->width; x++) {
                    const uint8_t* p = &src_row[x * 4]; // PS2: [0]=R, [1]=G, [2]=B, [3]=A
                    uint8_t a = (p[3] >= 128) ? 255 : (p[3] * 2);
                    dst_row[x] = build_argb8888(p[0], p[1], p[2], a);
                }
            }
            break;
        }
        case SCE_GS_PSMCT24: {
            // Finding #1: 24-bit direct color — PS2 stores 3 bytes per pixel [R,G,B], no alpha
            uint32_t raw_pitch_24 = fl_texture->width * 3;
            uint32_t aligned_pitch = (fl_texture->width * 4 + 63) & ~63;
            for (int y = 0; y < fl_texture->height; y++) {
                const uint8_t* src_row = raw + (y * raw_pitch_24);
                uint32_t* dst_row = (uint32_t*)(conv + (y * aligned_pitch));
                for (int x = 0; x < fl_texture->width; x++) {
                    const uint8_t* p = &src_row[x * 3];
                    dst_row[x] = build_argb8888(p[0], p[1], p[2], 255);
                }
            }
            break;
        }
        default:
            // Finding #5: Diagnostic for unhandled formats
            printf("[GCM] WARNING: Unsupported texture format %d for tex_handle %d (size %dx%d)\n",
                   fl_texture->format,
                   texture_handle,
                   fl_texture->width,
                   fl_texture->height);
            break;
        }
    } // end of scope for conv
}

// ---------------------------------------------------------
// Draw Logic
// ---------------------------------------------------------

// Convert ARGB to RGBA in memory for CELL_GCM_VERTEX_UB256 which maps Byte 0 -> R
static inline unsigned int swap_color_for_vertex(unsigned int c) {
#ifdef CPS3_NATIVE_COLOR_FORMAT_ARGB
    unsigned int a = (c >> 24) & 0xFF;
    unsigned int r = (c >> 16) & 0xFF;
    unsigned int g = (c >> 8) & 0xFF;
    unsigned int b_ch = c & 0xFF;
    return (r << 24) | (g << 16) | (b_ch << 8) | a;
#else
    return c;
#endif
}

static void draw_quad(const GcmVertex* v, bool textured) {
    if (render_task_count >= RENDER_TASK_MAX)
        return;

    int t_idx = render_task_count;
    GcmRenderTask* task = &render_tasks[t_idx];
    task->texture_handle = current_th;
    task->z = v[0].z;
    task->index = t_idx;
    task->original_index = t_idx;

    GcmVertex* b_vertices = &batch_vertices[t_idx * 6];
    int draw_order[6] = { 0, 1, 2, 2, 1, 3 };

    for (int i = 0; i < 6; i++) {
        int v_idx = draw_order[i];
        b_vertices[i].x = v[v_idx].x * g_resolution_scale;
        b_vertices[i].y = v[v_idx].y * g_resolution_scale;
        b_vertices[i].z = (task->z >= 0.0f && task->z <= 1.0f) ? task->z : 0.5f;
        b_vertices[i].u = v[v_idx].u;
        b_vertices[i].v = v[v_idx].v;

        // Correctly transform the 32-bit ARGB engine color into physical RGBA format for RSX reading
        b_vertices[i].color = swap_color_for_vertex(v[v_idx].color);

        if (textured) {
            b_vertices[i].u = v[v_idx].u;
            b_vertices[i].v = v[v_idx].v;
        } else {
            b_vertices[i].u = 0.0f;
            b_vertices[i].v = 0.0f;
        }
    }

    render_task_count++;
}

void CRS_Renderer_DrawTexturedQuad(const Sprite* sprite, unsigned int color) {
    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = sprite->v[i].x;
        v[i].y = sprite->v[i].y;
        v[i].z = sprite->v[i].z;
        v[i].color = color;
        v[i].u = sprite->t[i].s;
        v[i].v = sprite->t[i].t;
    }
    draw_quad(v, true);
}

void CRS_Renderer_DrawSolidQuad(const Quad* quad, unsigned int color) {
    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = quad->v[i].x;
        v[i].y = quad->v[i].y;
        v[i].z = quad->v[i].z;
        v[i].color = color;
        v[i].u = 0;
        v[i].v = 0;
    }
    draw_quad(v, false);
}

void CRS_Renderer_DrawSprite(const Sprite* sprite, unsigned int color) {
    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].z = sprite->v[0].z;
        v[i].color = color;
    }
    v[0].x = sprite->v[0].x;
    v[0].y = sprite->v[0].y;
    v[3].x = sprite->v[3].x;
    v[3].y = sprite->v[3].y;
    v[1].x = v[3].x;
    v[1].y = v[0].y;
    v[2].x = v[0].x;
    v[2].y = v[3].y;

    v[0].u = sprite->t[0].s;
    v[0].v = sprite->t[0].t;
    v[3].u = sprite->t[3].s;
    v[3].v = sprite->t[3].t;
    v[1].u = v[3].u;
    v[1].v = v[0].v;
    v[2].u = v[0].u;
    v[2].v = v[3].v;

    draw_quad(v, true);
}

void CRS_Renderer_DrawSprite2(const Sprite2* sprite2) {
    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].z = sprite2->v[0].z;
        v[i].color = sprite2->vertex_color;
    }
    v[0].x = sprite2->v[0].x;
    v[0].y = sprite2->v[0].y;
    v[3].x = sprite2->v[1].x;
    v[3].y = sprite2->v[1].y;
    v[1].x = v[3].x;
    v[1].y = v[0].y;
    v[2].x = v[0].x;
    v[2].y = v[3].y;

    v[0].u = sprite2->t[0].s;
    v[0].v = sprite2->t[0].t;
    v[3].u = sprite2->t[1].s;
    v[3].v = sprite2->t[1].t;
    v[1].u = v[3].u;
    v[1].v = v[0].v;
    v[2].u = v[0].u;
    v[2].v = v[3].v;

    draw_quad(v, true);
}

// ---------------------------------------------------------
// Engine Native 2D Overlays and Spline Text Queueing
// ---------------------------------------------------------

void Renderer_Init(void) {}

void Renderer_DrawSolidQuadVtx(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;
    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = vertices[i].x;
        v[i].y = vertices[i].y;
        v[i].z = vertices[i].z;
        v[i].color = vertices[0].color;
        v[i].u = 0;
        v[i].v = 0;
    }
    draw_quad(v, false);
}

void Renderer_DrawSpriteVtx(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;

    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = vertices[i].x;
        v[i].y = vertices[i].y;
        v[i].z = vertices[i].z;
        v[i].color = vertices[0].color;
        v[i].u = vertices[i].u;
        v[i].v = vertices[i].v;
    }
    draw_quad(v, true);
}

// Static state for PPG engine integration
static int s_CurrentPPGPageIndex = -1;
static Texture* s_CurrentTexture = NULL;

void Renderer_SetCurrentTexture(Texture* tex) {
    s_CurrentTexture = tex;
}

int Renderer_GetCurrentPPGPageIndex(void) {
    return s_CurrentPPGPageIndex;
}

void Renderer_SetTexture(int textureId) {
    u32 texCode;

    if (textureId >= 0x10000) {
        texCode = (u32)textureId;
    } else if (textureId < 0) {
        u16 palHandle = ppgGetCurrentPaletteHandle();
        texCode = (u32)(-textureId) | ((u32)palHandle << 16);
    } else if (s_CurrentTexture != NULL) {
        s32 ix = textureId - s_CurrentTexture->ixNum1st;
        if (ix >= 0 && ix < s_CurrentTexture->total && s_CurrentTexture->handle != NULL) {
            u16 texHandle = s_CurrentTexture->handle[ix].b16[0];
            if (texHandle == 0)
                return;
            s_CurrentPPGPageIndex = ix;
            u16 palHandle = 0;
            if (s_CurrentTexture->handle[ix].b16[1] & 0x4000) {
                palHandle = ppgGetCurrentPaletteHandle();
            }
            texCode = texHandle | ((u32)palHandle << 16);
        } else {
            u16 palHandle = ppgGetCurrentPaletteHandle();
            texCode = (u32)textureId | ((u32)palHandle << 16);
        }
    } else {
        u16 palHandle = ppgGetCurrentPaletteHandle();
        texCode = (u32)textureId | ((u32)palHandle << 16);
    }

    if (texCode == 0)
        return;

    // Instead of SDLGameRenderer_SetTexture, we natively update current_th
    CRS_Renderer_SetTexture(texCode);
}

void Renderer_DrawTexturedQuadVtx(const RendererVertex* vertices, int count) {
    if (count != 4)
        return;

    GcmVertex v[4];
    for (int i = 0; i < 4; i++) {
        v[i].x = vertices[i].x;
        v[i].y = vertices[i].y;
        v[i].z = vertices[i].z;
        v[i].color = vertices[0].color;
        v[i].u = vertices[i].u;
        v[i].v = vertices[i].v;
    }
    draw_quad(v, true);
}

void Renderer_Queue2DPrimitive(const f32* pos, f32 priority, uintptr_t data, int type) {
    s32 i;
    s32 ix = s_Render2DQueue.total;
    s32 prev;

    if (ix >= RENDER_2D_PRIM_MAX) {
        printf("[GCM] Renderer: 2D primitive buffer overflow\n");
        return;
    }

    if (type == 0) {
        s_Render2DQueue.prim[ix].v[0].z = s_Render2DQueue.prim[ix].v[1].z = s_Render2DQueue.prim[ix].v[2].z =
            s_Render2DQueue.prim[ix].v[3].z = priority;
        s_Render2DQueue.prim[ix].v[0].x = pos[0];
        s_Render2DQueue.prim[ix].v[0].y = pos[1];
        s_Render2DQueue.prim[ix].v[1].x = pos[2];
        s_Render2DQueue.prim[ix].v[1].y = pos[3];
        s_Render2DQueue.prim[ix].v[2].x = pos[4];
        s_Render2DQueue.prim[ix].v[2].y = pos[5];
        s_Render2DQueue.prim[ix].v[3].x = pos[6];
        s_Render2DQueue.prim[ix].v[3].y = pos[7];
        s_Render2DQueue.prim[ix].type = 0;
        s_Render2DQueue.prim[ix].attr.color = (u32)data;
    } else if (type == 1) {
        s_Render2DQueue.prim[ix].v[0].z = priority;
        s_Render2DQueue.prim[ix].v[0].y = pos[0];
        s_Render2DQueue.prim[ix].type = 1;
        s_Render2DQueue.prim[ix].attr.work = (WORK*)data;
    }

    s_Render2DQueue.prim[ix].next = -1;

    if (s_Render2DQueue.ix1st == -1) {
        s_Render2DQueue.ix1st = ix;
    } else {
        i = s_Render2DQueue.ix1st;
        prev = -1;

        while (1) {
            if (priority > s_Render2DQueue.prim[i].v[0].z) {
                if (prev == -1) {
                    s_Render2DQueue.ix1st = ix;
                    s_Render2DQueue.prim[ix].next = i;
                } else {
                    s_Render2DQueue.prim[prev].next = ix;
                    s_Render2DQueue.prim[ix].next = i;
                }
                break;
            }

            if (s_Render2DQueue.prim[i].next == -1) {
                s_Render2DQueue.prim[i].next = ix;
                break;
            }

            prev = i;
            i = s_Render2DQueue.prim[i].next;
        }
    }

    s_Render2DQueue.total += 1;
}

void Renderer_Flush2DPrimitives(void) {
    Quad prm;
    s32 i, j;

    for (i = s_Render2DQueue.ix1st; i != -1; i = s_Render2DQueue.prim[i].next) {
        switch (s_Render2DQueue.prim[i].type) {
        case 0:
            for (j = 0; j < 4; j++) {
                prm.v[j] = s_Render2DQueue.prim[i].v[j];
            }
            CRS_Renderer_DrawSolidQuad(&prm, s_Render2DQueue.prim[i].attr.color);
            break;

        case 1:
            shadow_drawing(s_Render2DQueue.prim[i].attr.work, (s16)s_Render2DQueue.prim[i].v[0].y);
            break;
        }
    }

    Renderer_2DQueueInit();
}
