#ifndef SDL3_SDL_STUBS_H
#define SDL3_SDL_STUBS_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include <cell/cell_fs.h> /* P4: cellFsMkdir for save directory creation */

/* Basic Macros */
#ifndef SDLCALL
#define SDLCALL
#endif

/* Basic Types */
typedef uint8_t Uint8;
typedef uint16_t Uint16;
typedef uint32_t Uint32;
typedef uint64_t Uint64;
typedef int8_t Sint8;
typedef int16_t Sint16;
typedef int32_t Sint32;
typedef int64_t Sint64;
typedef float float32;

/* Rects */
typedef struct SDL_Rect {
    int x, y, w, h;
} SDL_Rect;
typedef struct SDL_FRect {
    float x, y, w, h;
} SDL_FRect;

/* Scancodes for Keymap */
typedef enum SDL_Scancode {
    SDL_SCANCODE_UNKNOWN = 0,
    SDL_SCANCODE_A = 4,
    SDL_SCANCODE_B = 5,
    SDL_SCANCODE_C = 6,
    SDL_SCANCODE_D = 7,
    SDL_SCANCODE_E = 8,
    SDL_SCANCODE_F = 9,
    SDL_SCANCODE_G = 10,
    SDL_SCANCODE_H = 11,
    SDL_SCANCODE_I = 12,
    SDL_SCANCODE_J = 13,
    SDL_SCANCODE_K = 14,
    SDL_SCANCODE_L = 15,
    SDL_SCANCODE_M = 16,
    SDL_SCANCODE_N = 17,
    SDL_SCANCODE_O = 18,
    SDL_SCANCODE_P = 19,
    SDL_SCANCODE_Q = 20,
    SDL_SCANCODE_R = 21,
    SDL_SCANCODE_S = 22,
    SDL_SCANCODE_T = 23,
    SDL_SCANCODE_U = 24,
    SDL_SCANCODE_V = 25,
    SDL_SCANCODE_W = 26,
    SDL_SCANCODE_X = 27,
    SDL_SCANCODE_Y = 28,
    SDL_SCANCODE_Z = 29,
    SDL_SCANCODE_1 = 30,
    SDL_SCANCODE_2 = 31,
    SDL_SCANCODE_3 = 32,
    SDL_SCANCODE_4 = 33,
    SDL_SCANCODE_5 = 34,
    SDL_SCANCODE_6 = 35,
    SDL_SCANCODE_7 = 36,
    SDL_SCANCODE_8 = 37,
    SDL_SCANCODE_9 = 38,
    SDL_SCANCODE_0 = 39,
    SDL_SCANCODE_RETURN = 40,
    SDL_SCANCODE_ESCAPE = 41,
    SDL_SCANCODE_BACKSPACE = 42,
    SDL_SCANCODE_TAB = 43,
    SDL_SCANCODE_SPACE = 44,
    SDL_SCANCODE_SEMICOLON = 51,
    SDL_SCANCODE_GRAVE = 53,
    SDL_SCANCODE_F1 = 58,
    SDL_SCANCODE_F11 = 68,
    SDL_SCANCODE_RIGHT = 79,
    SDL_SCANCODE_LEFT = 80,
    SDL_SCANCODE_DOWN = 81,
    SDL_SCANCODE_UP = 82,
} SDL_Scancode;

typedef enum SDL_Keycode {
    SDLK_UNKNOWN = 0,
    SDLK_RETURN = '\r',
    SDLK_ESCAPE = '\033',
    SDLK_BACKSPACE = '\b',
    SDLK_TAB = '\t',
    SDLK_SPACE = ' ',
    SDLK_GRAVE = '`',
    SDLK_F11 = 1073741892,
} SDL_Keycode;

typedef uint32_t SDL_Keymod;
#define SDL_KMOD_ALT 0x01

typedef struct SDL_KeyboardEvent {
    SDL_Scancode scancode;
    SDL_Keycode key;
    SDL_Keymod mod;
    bool down;
    bool repeat;
} SDL_KeyboardEvent;

/* Endianness */
#define SDL_Swap32BE(x) (x)
#define SDL_Swap16BE(x) (x)
#define SDL_Swap32LE(x)                                                                                                \
    ((((x) & 0xFF000000u) >> 24) | (((x) & 0x00FF0000u) >> 8) | (((x) & 0x0000FF00u) << 8) |                           \
     (((x) & 0x000000FFu) << 24))
#define SDL_Swap16LE(x) ((((x) & 0xFF00u) >> 8) | (((x) & 0x00FFu) << 8))

/* Audio Stubs */
typedef void* SDL_AudioStream;
typedef uint32_t SDL_AudioFormat;
#define SDL_AUDIO_S16 (SDL_AudioFormat)0x8010
typedef struct SDL_AudioSpec {
    SDL_AudioFormat format;
    int channels;
    int freq;
} SDL_AudioSpec;

typedef uint32_t SDL_AudioDeviceID;
#define SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK ((SDL_AudioDeviceID)0xFFFFFFFFu)

inline int SDL_PutAudioStreamData(SDL_AudioStream* s, const void* d, int l) {
    return 0;
}
inline SDL_AudioStream* SDL_CreateAudioStream(const SDL_AudioSpec* i, const SDL_AudioSpec* o) {
    return NULL;
}
inline SDL_AudioStream* SDL_OpenAudioDeviceStream(SDL_AudioDeviceID d, const SDL_AudioSpec* s, void* cb, void* u) {
    return NULL;
}
inline int SDL_ResumeAudioStreamDevice(SDL_AudioStream* s) {
    return 0;
}
inline int SDL_PauseAudioStreamDevice(SDL_AudioStream* s) {
    return 0;
}

/* Mutex/Sync Stubs */
#include <sys/synchronization.h>

#ifndef SDL_MUTEX_H
#define SDL_MUTEX_H
typedef struct ps3_mutex_t {
    sys_lwmutex_t lw;
} SDL_Mutex;
#endif

static inline SDL_Mutex* SDL_CreateMutex(void) {
    SDL_Mutex* m = (SDL_Mutex*)malloc(sizeof(SDL_Mutex));
    sys_lwmutex_attribute_t attr;
    sys_lwmutex_attribute_initialize(attr);
    /* STUB-MED-02: Changed to non-recursive to match SDL3 semantics.
     * SPU audio callback now unlocks before dispatching to avoid deadlocks. */
    sys_lwmutex_create(&m->lw, &attr);
    return m;
}

static inline void SDL_LockMutex(SDL_Mutex* m) {
    if (m)
        sys_lwmutex_lock(&m->lw, 0);
}

static inline void SDL_UnlockMutex(SDL_Mutex* m) {
    if (m)
        sys_lwmutex_unlock(&m->lw);
}

static inline void SDL_DestroyMutex(SDL_Mutex* m) {
    if (m) {
        sys_lwmutex_destroy(&m->lw);
        free(m);
    }
}

/* Atomics — C-08 Audit Fix: Use PPU memory barriers for thread safety.
 * cell/atomic.h includes ppu_intrinsics.h which defines __lwsync()/__isync().
 * Store-release uses __lwsync() (LoadStore+StoreStore ordering).
 * Load-acquire uses __isync() after the load (prevents speculative reordering).
 * Per CBE Architecture v1.01: lwsync does NOT order store-followed-by-load. */
#include <cell/atomic.h>
typedef int SDL_AtomicInt;
static inline void ps3_set_atomic_int(int* a, int v) {
    *a = v;
    __lwsync();
}
static inline int ps3_get_atomic_int(int* a) {
    int v = *a;
    __isync();
    return v;
}
#define SDL_SetAtomicInt(a, v) ps3_set_atomic_int((a), (v))
#define SDL_GetAtomicInt(a) ps3_get_atomic_int((a))
static inline void ps3_set_atomic_ptr(void** a, void* v) {
    *a = v;
    __lwsync();
}
static inline void* ps3_get_atomic_ptr(void** a) {
    void* v = *a;
    __isync();
    return v;
}
#define SDL_SetAtomicPointer(a, v) ps3_set_atomic_ptr((void**)(a), (void*)(v))
#define SDL_GetAtomicPointer(a) ps3_get_atomic_ptr((void**)(a))
#define SDL_MemoryBarrierRelease() __lwsync()
#define SDL_MemoryBarrierAcquire() __isync()

/* Thread Stubs */
typedef void* SDL_Thread;
typedef int(SDLCALL* SDL_ThreadFunction)(void* data);
extern void fatal_error(const char* fmt, ...);
/* STUB-MED-01 Audit Fix: Surface accidental usage instead of silent NULL crash */
#define SDL_CreateThread(f, n, d) (fatal_error("SDL_CreateThread not implemented on PS3"), (SDL_Thread*)NULL)
#define SDL_DetachThread(t) fatal_error("SDL_DetachThread not implemented on PS3")
#define SDL_WaitThread(t, s) fatal_error("SDL_WaitThread not implemented on PS3")

/* Video/Window Stubs */
typedef void* SDL_Window;
typedef void* SDL_GLContext;
typedef void* SDL_Renderer;
typedef void* SDL_GPUDevice;
typedef uint32_t SDL_WindowFlags;
#define SDL_WINDOW_RESIZABLE 0
#define SDL_WINDOW_HIGH_PIXEL_DENSITY 0
#define SDL_WINDOW_FULLSCREEN 0
#define SDL_GetWindowFlags(w) (0)
#define SDL_SetWindowFullscreen(w, f) (0)
#define SDL_CreateWindow(t, w, h, f) (NULL)
#define SDL_DestroyWindow(w) ((void)0)
#define SDL_CreateWindowAndRenderer(t, w, h, f, pw, pr) (0)
#define SDL_DestroyRenderer(r) ((void)0)

/* Rendering Stubs */
typedef void* SDL_Texture;
typedef uint32_t SDL_PixelFormat;
#define SDL_PIXELFORMAT_ARGB32 0
typedef enum SDL_TextureAccess { SDL_TEXTUREACCESS_TARGET } SDL_TextureAccess;
typedef enum SDL_ScaleMode { SDL_SCALEMODE_LINEAR, SDL_SCALEMODE_NEAREST, SDL_SCALEMODE_INVALID } SDL_ScaleMode;
typedef uint32_t SDL_BlendMode;
#define SDL_BLENDMODE_BLEND 0
#define SDL_ALPHA_OPAQUE 255
typedef struct SDL_Point {
    int x, y;
} SDL_Point;

#define SDL_SetRenderDrawBlendMode(r, b) (0)
#define SDL_SetRenderDrawColor(r, r1, g, b, a) (0)
#define SDL_SetRenderTarget(r, t) (0)
#define SDL_RenderClear(r) (0)
#define SDL_RenderPresent(r) ((void)0)
#define SDL_GetRenderOutputSize(r, w, h) (0)
#define SDL_CreateTexture(r, f, a, w, h) (NULL)
#define SDL_SetTextureScaleMode(t, m) (0)
#define SDL_DestroyTexture(t) ((void)0)
#define SDL_RenderTexture(r, t, s, d) (0)

/* Event Stubs */
typedef struct SDL_GamepadDeviceEvent {
    uint32_t type;
    uint32_t which;
} SDL_GamepadDeviceEvent;
typedef union SDL_Event {
    uint32_t type;
    SDL_KeyboardEvent key;
    SDL_GamepadDeviceEvent gdevice;
    uint8_t padding[128];
} SDL_Event;
#define SDL_EVENT_QUIT 0
#define SDL_EVENT_KEY_DOWN 1
#define SDL_EVENT_KEY_UP 2
#define SDL_EVENT_GAMEPAD_REMOVED 4
#define SDL_EVENT_MOUSE_MOTION 5

typedef struct SDL_GamepadButtonEvent SDL_GamepadButtonEvent;
typedef struct SDL_GamepadAxisEvent SDL_GamepadAxisEvent;
typedef struct SDL_JoyDeviceEvent SDL_JoyDeviceEvent;
typedef struct SDL_JoyButtonEvent SDL_JoyButtonEvent;
typedef struct SDL_JoyAxisEvent SDL_JoyAxisEvent;
typedef struct SDL_JoyHatEvent SDL_JoyHatEvent;
#define SDL_EVENT_WINDOW_RESIZED 6
#define SDL_PushEvent(e) (0)
/* Color Stubs */
typedef struct SDL_Color {
    uint8_t r, g, b, a;
} SDL_Color;
typedef struct SDL_FColor {
    float r, g, b, a;
} SDL_FColor;
#define SDL_ALPHA_OPAQUE_FLOAT 1.0f
#define SDL_ALPHA_TRANSPARENT 0

/* Surface/Palette Stubs */
typedef void* SDL_Surface;
typedef struct SDL_Palette {
    int ncolors;
    SDL_Color* colors;
} SDL_Palette;
#define SDL_CreateSurfaceFrom(w, h, f, p, pitch) (NULL)
#define SDL_DestroySurface(s) ((void)0)
#define SDL_SetSurfacePalette(s, p) (0)
#define SDL_CreatePalette(n) (NULL)
#define SDL_SetPaletteColors(p, c, f, n) (0)
#define SDL_DestroyPalette(p) ((void)0)
#define SDL_CreateTextureFromSurface(r, s) (NULL)
#define SDL_SetTextureBlendMode(t, m) (0)
#define SDL_RenderReadPixels(r, rect) (NULL)
#define SDL_SaveBMP(s, f) (0)

/* Vertex Stubs (for SDL_RenderGeometry) */
typedef struct SDL_Vertex {
    struct {
        float x, y;
    } position;
    SDL_FColor color;
    struct {
        float x, y;
    } tex_coord;
} SDL_Vertex;
#define SDL_RenderGeometry(r, t, v, nv, i, ni) (0)
#define SDL_SetRenderDrawColorFloat(r, rf, gf, bf, af) (0)
#define SDL_RenderRect(r, rect) (0)
#define SDL_SetRenderScale(r, sx, sy) (0)
#define SDL_RenderDebugTextFormat(r, x, y, ...) (0)

/* Window extras */
#define SDL_WINDOW_HIDDEN 0
#define SDL_ShowWindow(w) (0)
#define SDL_RaiseWindow(w) (0)
#define SDL_ShowOpenFileDialog(cb, u, w, f, n, d, m) ((void)0)
#define SDL_MESSAGEBOX_INFORMATION 0
#define SDL_MESSAGEBOX_ERROR 1
#define SDL_RemovePath(p) (0)
#define SDL_strncmp strncmp
#define SDL_strstr strstr

typedef enum { SDL_ENUM_CONTINUE, SDL_ENUM_FAILURE, SDL_ENUM_SUCCESS } SDL_EnumerationResult;
typedef SDL_EnumerationResult(SDLCALL* SDL_EnumerateDirectoryCallback)(void* userdata, const char* dirname,
                                                                       const char* fname);

/* IO / Async Stubs */
typedef enum { SDL_ASYNCIO_TASK_READ, SDL_ASYNCIO_TASK_WRITE, SDL_ASYNCIO_TASK_CLOSE } SDL_AsyncIOTaskType;
typedef enum { SDL_ASYNCIO_COMPLETE, SDL_ASYNCIO_FAILURE, SDL_ASYNCIO_CANCELED } SDL_AsyncIOResult;
typedef void* SDL_AsyncIOQueue;
typedef void* SDL_AsyncIO;
typedef struct SDL_AsyncIOOutcome {
    SDL_AsyncIOTaskType type;
    SDL_AsyncIOResult result;
    void* userdata;
    int32_t bytes_transferred;
} SDL_AsyncIOOutcome;
typedef void* SDL_IOStream;
#define SDL_CreateAsyncIOQueue() (NULL)
#define SDL_AsyncIOFromFile(p, m) (NULL)
#define SDL_WaitAsyncIOResult(q, o, t) (false)
#define SDL_GetAsyncIOResult(q, o) (false)
#define SDL_CloseAsyncIO(a, f, q, u) (0)
#define SDL_ReadAsyncIO(a, b, o, s, q, u) (0)
#define SDL_DestroyAsyncIOQueue(q) ((void)0)
#define SDL_CloseIO(i) (0)
#define SDL_ReadU32LE(i, v) (*(v) = 0, 0)
#define SDL_ReadU32BE(i, v) (*(v) = 0, 0)
#define SDL_ReadU16BE(i, v) (*(v) = 0, 0)
#define SDL_ReadS16BE(i, v) (*(v) = 0, 0)
#define SDL_ReadU8(i, v) (*(v) = 0, 0)
#define SDL_GetIOSize(i) (0)
#define SDL_SeekIO(i, o, w) (0)
#define SDL_ReadIO(i, p, s) (0)
#define SDL_WriteIO(i, p, s) (0)
#define SDL_IOFromFile(p, m) (NULL)
#define SDL_IOFromConstMem(p, s) (NULL)

typedef enum { SDL_IO_SEEK_SET, SDL_IO_SEEK_CUR, SDL_IO_SEEK_END } SDL_IOWhence;

/* File / Path Stubs */
typedef enum {
    SDL_PATHTYPE_NONE,
    SDL_PATHTYPE_FILE,
    SDL_PATHTYPE_DIR,
    SDL_PATHTYPE_OTHER,
    SDL_PATHTYPE_DIRECTORY = SDL_PATHTYPE_DIR
} SDL_PathType;
typedef struct SDL_PathInfo {
    SDL_PathType type;
    uint64_t size;
    uint64_t create_time;
    uint64_t modify_time;
    uint64_t access_time;
} SDL_PathInfo;
#define SDL_GetBasePath() (NULL)
#define SDL_GetPathInfo(p, i) (false)
#define SDL_CopyFile(s, d) (-1)
#define SDL_GetPrefPath(o, a) (NULL)

/* Shared Object Stubs */
typedef void* SDL_SharedObject;
#define SDL_LoadObject(p) (NULL)
#define SDL_LoadFunction(o, n) (NULL)
#define SDL_UnloadObject(o) ((void)0)
/* String/Utils */
#define SDL_arraysize(a) (sizeof(a) / sizeof((a)[0]))
/* C-07 Audit Fix: SDL_strlcpy must null-terminate (strncpy does not) */
static inline size_t ps3_strlcpy(char* dst, const char* src, size_t n) {
    if (n > 0) {
        strncpy(dst, src, n - 1);
        dst[n - 1] = '\0';
    }
    return strlen(src);
}
#define SDL_strlcpy ps3_strlcpy
/* C-10 Audit Fix: SDL_strtok_r has 3 args; map to strtok_r (POSIX) */
#define SDL_strtok_r strtok_r
#define SDL_snprintf snprintf
#define SDL_vsnprintf vsnprintf
/* STUB-MED-01 Audit Fix: Implement asprintf dynamically */
static inline int ps3_vasprintf(char** strp, const char* fmt, va_list ap) {
    va_list ap2;
    va_copy(ap2, ap);
    int size = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (size < 0) {
        *strp = NULL;
        return -1;
    }
    *strp = (char*)malloc(size + 1);
    if (!*strp)
        return -1;
    int ret = vsnprintf(*strp, size + 1, fmt, ap);
    if (ret < 0) {
        free(*strp);
        *strp = NULL;
        return -1;
    }
    return ret;
}

static inline int ps3_asprintf(char** strp, const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int ret = ps3_vasprintf(strp, fmt, ap);
    va_end(ap);
    return ret;
}

#define SDL_asprintf ps3_asprintf
#define SDL_vasprintf ps3_vasprintf
#define SDL_free(p) free(p)
#define SDL_malloc(s) malloc(s)
#define SDL_calloc(n, s) calloc(n, s)
#define SDL_memset memset
#define SDL_memcpy memcpy
#define SDL_memmove memmove
#define SDL_strdup strdup
#define SDL_atoi atoi
/* M-10 Audit Fix: Support hex radix in SDL_itoa */
static inline char* ps3_itoa(int v, char* s, int r) {
    if (r == 16)
        sprintf(s, "%x", v);
    else if (r == 8)
        sprintf(s, "%o", v);
    else
        sprintf(s, "%d", v);
    return s;
}
#define SDL_itoa ps3_itoa
#define SDL_strlen strlen
#define SDL_strcmp strcmp
#define SDL_isspace isspace
#define SDL_isdigit isdigit
#define SDL_qsort qsort

#define SDL_zero(x) memset(&(x), 0, sizeof(x))
#define SDL_zeroa(x) memset((x), 0, sizeof(x))
#define SDL_zerop(x) memset((x), 0, sizeof(*(x)))
#define SDL_copyp(dst, src) memcpy((dst), (src), sizeof(*(src)))

/* ssize_t provided by PS3 SDK's sys/sys_types.h */
#include <sys/sys_types.h>

#define SDL_IOFromDynamicMem() (NULL)
#define SDL_IOFromMem(p, s) (NULL)

/* Logging */
#define SDL_Log(...) ((void)0)
#define SDL_LogError(c, ...) ((void)0)
#define SDL_LogInfo(c, ...) ((void)0)
#define SDL_LogDebug(c, ...) ((void)0)
#define SDL_LogWarn(c, ...) ((void)0)
#define SDL_LOG_CATEGORY_APPLICATION 0

/* M-04/M-08 Audit Fix: Implement timing using PS3 sys_time */
#include <sys/sys_time.h>
#include <sys/timer.h>
static inline uint64_t ps3_get_ticks(void) {
    return sys_time_get_system_time() / 1000ULL; /* microseconds → milliseconds */
}
static inline uint64_t ps3_get_ticks_ns(void) {
    return sys_time_get_system_time() * 1000ULL; /* microseconds → nanoseconds */
}
#define SDL_Delay(m) sys_timer_usleep((m) * 1000)
#define SDL_DelayNS(n) sys_timer_usleep((n) / 1000)
#define SDL_GetTicks() ps3_get_ticks()
#define SDL_GetTicksNS() ps3_get_ticks_ns()
#define SDL_GetPerformanceCounter() sys_time_get_system_time()
#define SDL_GetPerformanceFrequency() (1000000ULL) /* microsecond resolution */

/* System */
#define SDL_GetError() ""
#define SDL_PollEvent(e) (0)
#define SDL_PumpEvents() ((void)0)
#define SDL_assert(condition) ((void)0)
#define SDL_max(a, b) ((a) > (b) ? (a) : (b))
#define SDL_rand_bits() (rand())
/* P4 Audit Fix: Create directories on PS3 HDD for save data */
static inline int ps3_create_directory(const char* path) {
    CellFsErrno ret = cellFsMkdir(path, CELL_FS_DEFAULT_CREATE_MODE_1);
    /* EEXIST is OK — directory already exists */
    return (ret == CELL_FS_SUCCEEDED || ret == CELL_FS_EEXIST) ? 0 : -1;
}
#define SDL_CreateDirectory(p) ps3_create_directory(p)
#define SDL_EnumerateDirectory(p, c, u) (0)
#define SDL_ShowSimpleMessageBox(f, t, m, w) (0)
#define SDL_ShowCursor() ((void)0)
#define SDL_HideCursor() ((void)0)
#define SDL_Init(f) (1)
#define SDL_Quit() ((void)0)
#define SDL_SetAppMetadata(n, v, i) (0)
#define SDL_SetHint(n, v) (0)
#define SDL_HINT_VIDEO_WAYLAND_PREFER_LIBDECOR ""
#define SDL_HINT_NO_SIGNAL_HANDLERS ""
#define SDL_INIT_VIDEO 0
#define SDL_INIT_AUDIO 0
#define SDL_INIT_GAMEPAD 0

#endif
