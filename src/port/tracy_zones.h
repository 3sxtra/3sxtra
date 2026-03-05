// tracy_zones.h — Thin C-compatible Tracy profiler wrapper.
// When TRACY_ENABLE is defined, expands to real Tracy zones.
// Otherwise, all macros compile to nothing (zero overhead).
//
// API:
//   TRACE_ZONE_N("name") / TRACE_ZONE_END()  — function-level zone
//   TRACE_SUB_BEGIN("name")  — opens a sub-zone (wraps in { scope)
//   TRACE_SUB_END()          — closes the sub-zone (closes } scope)
//   TRACE_SUB_DYN_BEGIN(ptr, len)  — sub-zone with runtime-determined name
//
// Lock tracking:
//   TRACE_LOCK_ANNOUNCE(ctx) / TRACE_LOCK_TERMINATE(ctx)
//   TRACE_LOCK_BEFORE(ctx) / TRACE_LOCK_AFTER(ctx) / TRACE_LOCK_UNLOCK(ctx)
//   TRACE_LOCK_NAME(ctx, name, len)
//
// Plots, messages, thread naming, zone colors/values:
//   TRACE_PLOT_INT(name, val) / TRACE_PLOT_FLOAT(name, val)
//   TRACE_MSG(txt) / TRACE_MSG_COLOR(txt, color)
//   TRACE_THREAD_NAME(name)
//   TRACE_ZONE_NC(name, color)
//   TRACE_ZONE_VALUE(val) / TRACE_SUB_VALUE(val)
#pragma once

// ⚡ Subsystem color constants for zone categorization
#define TRACE_COLOR_GAME     0xE06060  // Red — game logic / task scheduler
#define TRACE_COLOR_RENDER   0x60E060  // Green — rendering pipeline
#define TRACE_COLOR_SOUND    0x6060E0  // Blue — audio / SPU
#define TRACE_COLOR_UI       0xE060E0  // Magenta — UI / menu bridge
#define TRACE_COLOR_NET      0xE0E060  // Yellow — netplay
#define TRACE_COLOR_IO       0x60E0E0  // Cyan — I/O / file loading

#ifdef TRACY_ENABLE
#include <tracy/TracyC.h>

// ── Function-level zone (one per function, uses fixed variable name) ──
#define TRACE_ZONE() TracyCZone(___tracy_ctx_fn, true)
#define TRACE_ZONE_N(name) TracyCZoneN(___tracy_ctx_fn, name, true)
#define TRACE_ZONE_NC(name, color) TracyCZoneNC(___tracy_ctx_fn, name, color, true)
#define TRACE_ZONE_END() TracyCZoneEnd(___tracy_ctx_fn)
#define TRACE_ZONE_VALUE(val) TracyCZoneValue(___tracy_ctx_fn, val)

// ── Sub-zone (opens a brace scope with its own context) ──
#define TRACE_SUB_BEGIN(name)                                                                                          \
    {                                                                                                                  \
        TracyCZoneN(___tracy_sub_ctx, name, true)
#define TRACE_SUB_END()                                                                                                \
    TracyCZoneEnd(___tracy_sub_ctx);                                                                                   \
    }
#define TRACE_SUB_VALUE(val) TracyCZoneValue(___tracy_sub_ctx, val)

// ── Dynamic-name sub-zone — for runtime-determined zone names ──
#define TRACE_SUB_DYN_BEGIN(name_ptr, name_len)                                                                        \
    {                                                                                                                  \
        TracyCZone(___tracy_sub_ctx, true);                                                                            \
        TracyCZoneName(___tracy_sub_ctx, name_ptr, name_len)

// ── Frame marks ──
#define TRACE_FRAME_MARK() TracyCFrameMark

// ── Lock contention tracking ──
#define TRACE_LOCK_ANNOUNCE(lock)    TracyCLockAnnounce(lock)
#define TRACE_LOCK_TERMINATE(lock)   TracyCLockTerminate(lock)
#define TRACE_LOCK_BEFORE(lock)      TracyCLockBeforeLock(lock)
#define TRACE_LOCK_AFTER(lock)       TracyCLockAfterLock(lock)
#define TRACE_LOCK_UNLOCK(lock)      TracyCLockAfterUnlock(lock)
#define TRACE_LOCK_NAME(lock, n, l)  TracyCLockCustomName(lock, n, l)

// ── Plots — time-series graphs on the timeline ──
#define TRACE_PLOT_INT(name, val)    TracyCPlotI(name, val)
#define TRACE_PLOT_FLOAT(name, val)  TracyCPlotF(name, val)
#define TRACE_PLOT_CONFIG(name, type, step, fill, color) \
    TracyCPlotConfig(name, type, step, fill, color)

// ── Messages — event markers on the timeline ──
#define TRACE_MSG(txt)               TracyCMessageL(txt)
#define TRACE_MSG_COLOR(txt, color)  TracyCMessageLC(txt, color)

// ── Thread naming ──
#define TRACE_THREAD_NAME(name)      TracyCSetThreadName(name)

// ── Connection check ──
#define TRACE_IS_CONNECTED()         TracyCIsConnected

#else /* !TRACY_ENABLE */

#define TRACE_ZONE() ((void)0)
#define TRACE_ZONE_N(name) ((void)0)
#define TRACE_ZONE_NC(name, color) ((void)0)
#define TRACE_ZONE_END() ((void)0)
#define TRACE_ZONE_VALUE(val) ((void)0)

#define TRACE_SUB_BEGIN(name)                                                                                          \
    {                                                                                                                  \
        ((void)0)
#define TRACE_SUB_END()                                                                                                \
    ((void)0);                                                                                                         \
    }
#define TRACE_SUB_VALUE(val) ((void)0)

#define TRACE_SUB_DYN_BEGIN(name_ptr, name_len)                                                                        \
    {                                                                                                                  \
        ((void)0)

#define TRACE_FRAME_MARK() ((void)0)

#define TRACE_LOCK_ANNOUNCE(lock)    ((void)0)
#define TRACE_LOCK_TERMINATE(lock)   ((void)0)
#define TRACE_LOCK_BEFORE(lock)      ((void)0)
#define TRACE_LOCK_AFTER(lock)       ((void)0)
#define TRACE_LOCK_UNLOCK(lock)      ((void)0)
#define TRACE_LOCK_NAME(lock, n, l)  ((void)0)

#define TRACE_PLOT_INT(name, val)    ((void)0)
#define TRACE_PLOT_FLOAT(name, val)  ((void)0)
#define TRACE_PLOT_CONFIG(name, type, step, fill, color) ((void)0)

#define TRACE_MSG(txt)               ((void)0)
#define TRACE_MSG_COLOR(txt, color)  ((void)0)

#define TRACE_THREAD_NAME(name)      ((void)0)

#define TRACE_IS_CONNECTED()         0

#endif
