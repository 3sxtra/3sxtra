#include "port/sdl/renderer/sdl_texture_util.h"
#include "port/rendering/renderer.h"
#include "port/sdl/app/sdl_app.h"

void* TextureUtil_Load(const char* filename) { return 0; }
void* TextureUtil_LoadScaled(const char* filename, float scale) { return 0; }
void TextureUtil_Free(void* texture_id) {}
void TextureUtil_GetSize(void* texture_id, int* w, int* h) { if(w) *w = 0; if(h) *h = 0; }
void TextureUtil_DrawQuadEx(void* texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y) {}

void Renderer_SetTexture(int textureId) {}
void Renderer_DrawTexturedQuadVtx(const RendererVertex* vertices, int count) {}
void Renderer_Queue2DPrimitive(const float* pos, float priority, unsigned int data, int type) {}
void Renderer_DrawSpriteVtx(const RendererVertex* vertices, int count) {}
// Structural / Engine / Window stubs for PS3 port
int Renderer_LZ77Enqueue(const unsigned char* a, unsigned int b, unsigned int c, int d, int e, unsigned int f, unsigned int g) { return 0; }
void Renderer_UpdateTexture(int a, const void* b, int c, int d, int w, int h) {}
void* LoadFullSpriteOverride(unsigned char a, unsigned short b, unsigned char c) { return 0; }
void Renderer_Init(void) {}
void Renderer_DrawSolidQuadVtx(const RendererVertex* vertices, int count) {}
void Renderer_Flush2DPrimitives(void) {}
void Renderer_SetCurrentTexture(Texture* tex) {}
void SDLGameRenderer_FlushSprite2Batch(void) {}
void LagtestRenderer_Render(void) {}

#include "port/ps3/app/ps3_app.h"

// SDL backend stubs redirected to PS3 native app layer
void SDLApp_BeginFrame(void) { PS3App_BeginFrame(); }
void SDLApp_EndFrame(void) { PS3App_EndFrame(); }
Uint64 SDLApp_GetTargetFrameTimeNS(void) { return PS3App_GetTargetFrameTimeNS(); }
SDL_Window* SDLApp_GetWindow(void) { return 0; }
int SDLApp_Init() { return PS3App_FullInit(); }
bool SDLApp_IsFrameRateUncapped(void) { return PS3App_IsFrameRateUncapped(); }
bool SDLApp_IsVSyncEnabled(void) { return PS3App_IsVSyncEnabled(); }
bool SDLApp_PollEvents() { return PS3App_PollEvents(); }
void SDLApp_PresentOnly(void) { PS3App_EndFrame(); }
void SDLApp_Quit(void) { PS3App_Quit(); }
void SDLApp_SetRenderer(RendererBackend r) {}
void SDLApp_SetWindowPosition(int x, int y) {}
void SDLApp_SetWindowSize(int w, int h) {}
short SDLPad_GetJoystickAxis(int a, int b) { return 0; }
int SDLPad_GetJoystickButton(int a, int b) { return 0; }
int SDLPad_GetJoystickHat(int a, int b) { return 0; }
int SDLPad_IsJoystick(int a) { return 0; }
int SDLPad_IsKeyboard(int a) { return 0; }
const unsigned char* SDL_GetKeyboardState(int* a) { if(a) *a = 0; return 0; }
void SDL_ShowOpenFolderDialog(void* a, void* b, void* c, void* d, void* e) {}

// Compression stubs
void zlib_Decompress(void* a, void* b, unsigned int c, unsigned int d) {}
void zlib_InitSpurs(void) {}
void zlib_Initialize(void) {}

// SPU/Testing/MenuBridge stubs
void MenuBridge_Init(void) {}
void MenuBridge_PostTick(void) {}
void MenuBridge_PreTick(int a) {}
int MenuBridge_StepGate(void) { return 0; }
void SPU_TickAudio(void) {}
void TestRunner_Epilogue(void) {}
void TestRunner_Prologue(void) {}
int is_joystick_input(int a) { return 0; }
int is_keyboard_input(int a) { return 0; }

int g_resolution_scale = 1;

// UI/RmlUi Linker Stubs
int rmlui_casual_lobby_is_visible(void) { return 0; }
int rmlui_tournament_lobby_is_visible(void) { return 0; }
void rmlui_network_replay_picker_hide(void) {}
void rmlui_network_replay_picker_show(void) {}
void rmlui_network_replay_picker_poll(void) {}
void rmlui_network_replay_picker_update(void) {}
void rmlui_player_profile_hide(void) {}
void rmlui_player_profile_show(void) {}
void rmlui_player_profile_poll(void) {}
void rmlui_player_profile_update(void) {}
int g_lobby_reenter_from_match = 0;
int g_lobby_reenter_to_replay = 0;

// Training Mode Linker Stubs
void update_training_state(void) {}
void training_state_add_combo_hit(int a, int b) {}
void training_hud_draw(void) {}
void training_dummy_update_input(void* w) {}
void trials_init(void) {}
void trials_on_hit_registered(void) {}
int g_lua_dummy_active = 0;
short g_lua_dummy_player_id = 0;
void* g_dummy_settings = 0;
char g_training_state[1024] = {0}; // Dummy block to satisfy global struct linkage


// Modded Stage / UI Globals Linker Stubs
int game_paused = 0;
int mods_menu_fast_pre_game = 0;


// AcrSDK/PS2 hardware stubs for PS3 port
//
// The AcrSDK/ps2/ directory is excluded from the PS3 build, so we must
// provide the global variables and functions that were defined there.
// However, AcrSDK/common/ IS compiled in, so we can (and must) delegate
// to the real fms/mfl/mmAlloc infrastructure for memory management.

#include "main.h"
#include "sf33rd/AcrSDK/common/plcommon.h"
#include "sf33rd/AcrSDK/common/memfound.h"
#include "sf33rd/AcrSDK/common/fbms.h"
#include "sf33rd/AcrSDK/ps2/foundaps2.h"
#include "structs.h"
#include <stdlib.h>
#include <string.h>

// Forward declarations for functions defined later in this file
static void flPS2SystemTmpBuffFlush(void);
static void flPS2SystemTmpBuffInit(void);

// ============================================================================
// Constants — mirrored from foundaps2.c
// ============================================================================
#define FMS_HEAP_SIZE    0x01800000  // 24 MB
#define FMS_ALIGNMENT    0x40       // 64-byte alignment
#define SYSTEM_MEMORY_SIZE 0xA00000 // 10 MB system memory pool
#define SYSTEM_TMP_BUFF_SIZE 0x80000 // 512 KB per temp buffer\n// FL_TEXTURE_MAX and FL_PALETTE_MAX come from foundaps2.h

// ============================================================================
// Globals — these were defined in foundaps2.c / flps2etc.c / flps2vram.c
// ============================================================================
FL_FMS flFMS;
FLPS2State flPs2State;
FLTexture flTexture[FL_TEXTURE_MAX];
FLTexture flPalette[FL_PALETTE_MAX];

s32 flWidth;
s32 flHeight;
u32 flSystemRenderOperation;
s32 flVramStaticNum;
u32 flDebugStrHan;
u32 flDebugStrCol;
u32 flDebugStrCtr;
int fltpad_config_basic[64];
int flpad_io_map[25] = {0};

// ============================================================================
// FMS-based memory functions (replacing broken bump allocator)
// ============================================================================

/** @brief Allocate from the bottom of the frame memory stack. */
void* flAllocMemory(s32 size) {
    return fmsAllocMemory(&flFMS, size, 0);
}

/** @brief Allocate from the top of the frame memory stack. */
void* flAllocMemoryS(s32 size) {
    return fmsAllocMemory(&flFMS, size, 1);
}

/** @brief Snapshot the current frame pointer for heap 0. */
s32 flGetFrame(FMS_FRAME* frame) {
    return fmsGetFrame(&flFMS, 0, frame);
}

/** @brief Return remaining space in the frame memory stack. */
s32 flGetSpace(void) {
    return fmsCalcSpace(&flFMS);
}

// ============================================================================
// System memory helpers (were in flps2etc.c)
// ============================================================================

/** @brief Register a system memory handle, compacting if needed. */
u32 flPS2GetSystemMemoryHandle(s32 len, s32 type) {
    (void)type;
    u32 handle = mflRegisterS(len);
    if (handle == 0) {
        mflCompact();
        handle = mflRegister(len);
    }
    return handle;
}

/** @brief Release a system memory handle. */
void flPS2ReleaseSystemMemory(u32 handle) {
    mflRelease(handle);
}

/** @brief Retrieve the address of a system memory handle. */
void* flPS2GetSystemBuffAdrs(u32 handle) {
    return mflRetrieve(handle);
}

// ============================================================================
// Temporary buffer management (were in flps2etc.c)
// ============================================================================

/** @brief Allocate the double-buffered temporary buffer pool. */
static void flPS2SystemTmpBuffInit(void) {
    s32 i;
    for (i = 0; i < 2; i++) {
        flPs2State.SystemTmpBuffHandle[i] = flPS2GetSystemMemoryHandle(SYSTEM_TMP_BUFF_SIZE, 1);
    }
    flPS2SystemTmpBuffFlush();
}

/** @brief Reset the current temporary buffer pointer to the start. */
static void flPS2SystemTmpBuffFlush(void) {
    u32 len;
    switch (flPs2State.SystemStatus) {
    case 0:
    case 1:
    case 2:
        len = SYSTEM_TMP_BUFF_SIZE;
        flPs2State.SystemTmpBuffStartAdrs =
            (uintptr_t)flPS2GetSystemBuffAdrs(flPs2State.SystemTmpBuffHandle[flPs2State.SystemIndex]);
        flPs2State.SystemTmpBuffNow = flPs2State.SystemTmpBuffStartAdrs;
        flPs2State.SystemTmpBuffEndAdrs = flPs2State.SystemTmpBuffStartAdrs + len;
        break;
    default:
        break;
    }
}

/** @brief Allocate an aligned chunk from the current temporary buffer. */
uintptr_t flPS2GetSystemTmpBuff(s32 len, s32 align) {
    uintptr_t now = flPs2State.SystemTmpBuffNow;
    now = ~(align - 1) & (now + align - 1);
    uintptr_t new_now = now + len;
    if (flPs2State.SystemTmpBuffEndAdrs < new_now) {
        // Overflow — wrap to start (matches desktop behavior)
        now = flPs2State.SystemTmpBuffStartAdrs;
        new_now = now + len;
    }
    flPs2State.SystemTmpBuffNow = new_now;
    return now;
}

// ============================================================================
// VRAM texture/palette handle stubs
// ============================================================================

/** @brief Find the first unused texture slot. */
u32 flPS2GetTextureHandle(void) {
    for (s32 i = 0; i < FL_TEXTURE_MAX; i++) {
        if (!flTexture[i].be_flag) {
            return i + 1;
        }
    }
    return 0;
}

/** @brief Find the first unused palette slot. */
u32 flPS2GetPaletteHandle(void) {
    for (s32 i = 0; i < 1088; i++) {
        if (!flPalette[i].be_flag) {
            return (i + 1) << 16;
        }
    }
    return 0;
}

/** @brief Create a texture handle — stub that just allocates the slot. */
u32 flCreateTextureHandle(s32 id, plContext* bits, u32 flag) {
    (void)id;
    u32 th = flPS2GetTextureHandle();
    if (th == 0) return 0;
    FLTexture* t = &flTexture[th - 1];
    memset(t, 0, sizeof(FLTexture));
    t->be_flag = 1;
    t->flag = flag;
    if (bits) {
        t->desc = bits->desc;
        t->width = bits->width;
        t->height = bits->height;
        t->bitdepth = bits->bitdepth;
        // Compute a reasonable size
        s32 bpp = bits->bitdepth;
        if (bpp == 0) bpp = 1; // 4-bit = half byte per pixel, but min 1 for size calc
        t->size = bits->width * bits->height * bpp;
        if (bits->ptr != NULL) {
            // Source data provided — allocate system memory and copy
            t->mem_handle = flPS2GetSystemMemoryHandle(t->size, 2);
            if (t->mem_handle) {
                void* dst = flPS2GetSystemBuffAdrs(t->mem_handle);
                if (dst) memcpy(dst, bits->ptr, t->size);
            }
        } else {
            t->mem_handle = flPS2GetSystemMemoryHandle(t->size, 2);
        }
    }
    return th;
}

/** @brief Create a palette handle — stub that just allocates the slot. */
u32 flCreatePaletteHandle(plContext* lpcontext, u32 flag) {
    u32 ph = flPS2GetPaletteHandle();
    if (ph == 0) return 0;
    FLTexture* p = &flPalette[(ph >> 16) - 1];
    memset(p, 0, sizeof(FLTexture));
    p->be_flag = 1;
    p->flag = flag;
    if (lpcontext) {
        p->desc = lpcontext->desc;
        if (lpcontext->width == 256) {
            p->width = 16; p->height = 16;
        } else {
            p->width = 8; p->height = 2;
        }
        p->bitdepth = lpcontext->bitdepth;
        p->size = p->width * p->height * (p->bitdepth ? p->bitdepth : 1);
        if (lpcontext->ptr != NULL) {
            p->mem_handle = flPS2GetSystemMemoryHandle(p->size, 2);
            if (p->mem_handle) {
                void* dst = flPS2GetSystemBuffAdrs(p->mem_handle);
                if (dst) memcpy(dst, lpcontext->ptr, p->size);
            }
        } else {
            p->mem_handle = flPS2GetSystemMemoryHandle(p->size, 2);
        }
    }
    return ph >> 16;
}

/** @brief Release a texture handle. */
s32 flReleaseTextureHandle(u32 th) {
    if (th == 0 || th > FL_TEXTURE_MAX) return 0;
    FLTexture* t = &flTexture[th - 1];
    if (!t->be_flag) return 0;
    if (t->mem_handle) flPS2ReleaseSystemMemory(t->mem_handle);
    memset(t, 0, sizeof(FLTexture));
    return 1;
}

/** @brief Release a palette handle. */
s32 flReleasePaletteHandle(u32 ph) {
    if (ph == 0 || ph > 1088) return 0;
    FLTexture* p = &flPalette[ph - 1];
    if (!p->be_flag) return 0;
    if (p->mem_handle) flPS2ReleaseSystemMemory(p->mem_handle);
    memset(p, 0, sizeof(FLTexture));
    return 1;
}

/** @brief Lock a texture for CPU access — allocates temp buffer and populates plContext. */
s32 flLockTexture(Rect* lprect, u32 th, plContext* lpcontext, u32 flag) {
    (void)lprect;
    if (th == 0 || th > FL_TEXTURE_MAX) return 0;
    FLTexture* t = &flTexture[th - 1];
    if (!t->be_flag) return 0;

    u8* buff_ptr;
    if (t->mem_handle) {
        buff_ptr = (u8*)flPS2GetSystemBuffAdrs(t->mem_handle);
    } else {
        buff_ptr = (u8*)mflTemporaryUse(t->size > 0 ? t->size : 1024);
    }
    t->lock_ptr = (uintptr_t)buff_ptr;
    t->lock_flag = flag;

    lpcontext->desc = t->desc;
    lpcontext->width = t->width;
    lpcontext->height = t->height;
    lpcontext->ptr = buff_ptr;
    // Set reasonable defaults based on bitdepth
    lpcontext->bitdepth = t->bitdepth;
    lpcontext->pitch = t->width * (t->bitdepth ? t->bitdepth : 1);
    return 1;
}

/** @brief Unlock a texture. */
s32 flUnlockTexture(u32 th) {
    if (th == 0 || th > FL_TEXTURE_MAX) return 0;
    flTexture[th - 1].lock_flag = 0;
    return 1;
}

/** @brief Lock a palette for CPU access. */
s32 flLockPalette(Rect* lprect, u32 ph, plContext* lpcontext, u32 flag) {
    (void)lprect;
    if (ph == 0 || ph > 1088) return 0;
    FLTexture* p = &flPalette[ph - 1];
    if (!p->be_flag) return 0;

    u8* buff_ptr;
    if (p->mem_handle) {
        buff_ptr = (u8*)flPS2GetSystemBuffAdrs(p->mem_handle);
    } else {
        buff_ptr = (u8*)mflTemporaryUse(p->size > 0 ? p->size : 1024);
    }
    p->lock_ptr = (uintptr_t)buff_ptr;
    p->lock_flag = flag;

    lpcontext->desc = p->desc;
    lpcontext->ptr = buff_ptr;
    lpcontext->bitdepth = p->bitdepth;
    // Restore original dimensions for palette contexts
    if (p->width == 16 && p->height == 16) {
        lpcontext->width = 256; lpcontext->height = 1;
    } else {
        lpcontext->width = 16; lpcontext->height = 1;
    }
    lpcontext->pitch = lpcontext->width * lpcontext->bitdepth;
    return 1;
}

/** @brief Unlock a palette. */
s32 flUnlockPalette(u32 ph) {
    if (ph == 0 || ph > 1088) return 0;
    flPalette[ph - 1].lock_flag = 0;
    return 1;
}

// ============================================================================
// Misc stubs
// ============================================================================
s32 flFlip(u32 flag) {
    (void)flag;
    flPS2SystemTmpBuffFlush();
    return 1;
}
s32 flLogOut(s8* fmt, ...) { return 0; }
void flPrintL(int x, int y, const char* fmt, ...) {}
void flPrintColor(int color) {}
void flPS2ConvScreenFZ(int a, int b) {}
void flSetRenderState(int a, int b) {}
void flMemset(void* dst, s32 pat, s32 size) { memset(dst, pat, size); }
void flMemcpy(void* dst, void* src, s32 size) { memcpy(dst, src, size); }
void flPS2SystemError(s32 code, const char* msg) { (void)code; (void)msg; }
void flPS2DebugInit(void) {}

// Note: Renderer_CreateTexture, Renderer_DestroyTexture, etc. are provided
// by rendering/renderer.c which routes to ps3_stubs.c -> CRS_Renderer_*

// ============================================================================
// Initialization
// ============================================================================
extern int flPADInitialize(void);

s32 flInitialize(void) {
    // 1. Zero out global state
    memset(&flPs2State, 0, sizeof(FLPS2State));
    memset(flTexture, 0, sizeof(flTexture));
    memset(flPalette, 0, sizeof(flPalette));

    // 2. Allocate and initialize the FMS heap (matches foundaps2.c)
    void* heap = memalign(FMS_ALIGNMENT, FMS_HEAP_SIZE);
    if (!heap) return 0;
    fmsInitialize(&flFMS, heap, FMS_HEAP_SIZE, FMS_ALIGNMENT);

    // 3. Allocate system memory from the top of the FMS heap (matches foundaps2.c)
    void* sysmem = flAllocMemoryS(SYSTEM_MEMORY_SIZE);
    if (!sysmem) return 0;
    mflInit(sysmem, SYSTEM_MEMORY_SIZE, FMS_ALIGNMENT);

    // 4. Initialize temporary buffers
    flPS2SystemTmpBuffInit();

    // 5. Initialize pads
    flPADInitialize();

    return 1;
}
