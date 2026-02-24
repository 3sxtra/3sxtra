// tracy_zones.h — Thin C-compatible Tracy profiler wrapper.
// When TRACY_ENABLE is defined, expands to real Tracy zones.
// Otherwise, all macros compile to nothing (zero overhead).
//
// API:
//   TRACE_ZONE_N("name") / TRACE_ZONE_END()  — function-level zone
//   TRACE_SUB_BEGIN("name")  — opens a sub-zone (wraps in { scope)
//   TRACE_SUB_END()          — closes the sub-zone (closes } scope)
//
// Sub-zones use brace scoping so each gets its own ___tracy_ctx variable
// without name collisions.
#pragma once

#ifdef TRACY_ENABLE
#include <tracy/TracyC.h>

// Function-level zone (one per function, uses fixed variable name)
#define TRACE_ZONE() TracyCZone(___tracy_ctx_fn, true)
#define TRACE_ZONE_N(name) TracyCZoneN(___tracy_ctx_fn, name, true)
#define TRACE_ZONE_END() TracyCZoneEnd(___tracy_ctx_fn)

// Sub-zone (opens a brace scope with its own context)
#define TRACE_SUB_BEGIN(name)                                                                                          \
    {                                                                                                                  \
        TracyCZoneN(___tracy_sub_ctx, name, true)
#define TRACE_SUB_END()                                                                                                \
    TracyCZoneEnd(___tracy_sub_ctx);                                                                                   \
    }

#define TRACE_FRAME_MARK() TracyCFrameMark

#else /* !TRACY_ENABLE */

#define TRACE_ZONE() ((void)0)
#define TRACE_ZONE_N(name) ((void)0)
#define TRACE_ZONE_END() ((void)0)
#define TRACE_SUB_BEGIN(name)                                                                                          \
    {                                                                                                                  \
        ((void)0)
#define TRACE_SUB_END()                                                                                                \
    ((void)0);                                                                                                         \
    }
#define TRACE_FRAME_MARK() ((void)0)

#endif
