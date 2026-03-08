# Renderer Benchmarks — Classic vs SDL2D

**Date:** 2026-03-08
**Renderers:** `--renderer classic` (simple reference) vs `--renderer sdl2d` (optimized)
**Platforms:** Desktop (Windows) and Raspberry Pi 4

---

## Summary

| Platform | Classic | SDL2D (SwFrame ON) | SDL2D (SwFrame OFF) |
|----------|---------|---------------------|---------------------|
| **Desktop** | **795µs / 1,257 FPS** | 1,630µs / 613 FPS | 1,030µs / ~970 FPS |
| **Pi4** | **~4.7ms (60fps ✅)** | ~11ms+ (struggling) | not tested |

**Classic wins on both platforms.** SDL2D's SwFrame software rasterization is the primary bottleneck on both — consuming 39% of desktop time and 57% of Pi4's 16.7ms budget.

---

## Desktop Results

### Classic (Winner — 795µs mean, 1,257 FPS)

Session: 57.17s, 63,538 frames · Median **579µs (1,727 FPS)**

| Zone | Total (s) | Calls | Per-frame (µs) | % |
|------|-----------|-------|-----------------|---|
| RenderPresent | 18.57 | 63,538 | 292 | 32.5% |
| SetTexture | 11.78 | 17,897,335 | 185 | 20.6% |
| FramePacing | 4.61 | 63,538 | 73 | 8.1% |
| cpLoopTask | 3.26 | 316,071 | 51 | 5.7% |
| BatchRender | 3.03 | 63,207 | 48 | 5.3% |
| EndFrame | 2.54 | 63,538 | 40 | 4.4% |
| Sort (qsort) | 2.27 | 63,207 | 36 | 4.0% |

- SetTexture: 17.9M calls (282/frame), 659ns each — mostly Tracy overhead
- Cache hit rate: 99.95% (3,026 misses / 17.9M lookups)

### SDL2D + SwFrame (Slowest — 1,630µs mean, 613 FPS)

Session: 1:39.1, 44,572 frames · Median **1.00ms (997 FPS)**

| Zone | Total (s) | Calls | Per-frame (µs) | % |
|------|-----------|-------|-----------------|---|
| **SwFrame** | **38.75** | **44,322** | **874** | **39.1%** |
| RenderPresent | 20.92 | 44,572 | 469 | 21.1% |
| FramePacing | 9.15 | 44,572 | 205 | 9.2% |

### SDL2D − SwFrame (Middle — ~1,030µs, ~970 FPS)

Session: 33,803 frames

| Zone | Total (s) | Calls | Per-frame (µs) |
|------|-----------|-------|-----------------|
| RenderFrame | 17.73 | 33,803 | 525 |
| RenderPresent | 11.98 | 33,803 | 354 |
| SwFrame (disabled) | 0.002 | 33,608 | 0.06 |

Disabling SwFrame: 613 → ~970 FPS (+58%). Still slower than Classic due to SDL2D's heavier hardware path (adaptive sort, texture sub-sort, deferred texture resolution, rect/non-rect classification).

---

## Pi4 Results

### Classic (Winner — ~4.7ms, 60fps ✅)

Session: 29,384 frames

| Zone | Total (s) | Calls | Per-frame (µs) | % budget |
|------|-----------|-------|-----------------|----------|
| RenderPresent | 53.17 | 29,384 | 1,810 | 10.8% |
| EndFrame | 38.94 | 29,384 | 1,325 | 7.9% |
| SetTexture | 21.34 | 5,925,577 | 726 | 4.3% |
| cpLoopTask | 7.33 | 142,956 | 250 | 1.5% |
| BeginFrame | 5.62 | 29,384 | 191 | 1.1% |
| BatchRender | 4.61 | 29,193 | 157 | 0.9% |
| Sort (qsort) | 1.30 | 29,193 | 44 | 0.3% |
| FlushBatch | 0.90 | 35,551 | 30 | 0.2% |

- SetTexture: 5.9M calls, 3.6µs/call. Cache misses: 1,407 (99.98% hit rate)
- FlushBatch: 30µs/frame — 227× faster than old SDL2D's 6,830µs/frame

### SDL2D + SwFrame (Slowest — ~11ms+, struggling for 60fps)

Session: 4,971 frames

| Zone | Total (s) | Calls | Per-frame (µs) | % budget |
|------|-----------|-------|-----------------|----------|
| **SwFrame** | **47.17** | **4,918** | **9,594** | **57.4%** |
| RenderPresent | 4.57 | 4,971 | 919 | 5.5% |
| cpLoopTask | 1.42 | 20,903 | 286 | 1.7% |

Pi4's ARM CPU is too slow for per-pixel software blending at 384×224.

---

## Experiments: What We Tried

### 1. Original Classic Renderer (user's pre-optimization code)
Adapted the original SDL renderer to the Classic API. Key differences: texture stack, reverse sort tie-breaking, destroy+recreate unlock pattern, RENDER_TASK_MAX=1024.

**Result: ~1,050µs on desktop (32% slower)**
- CreateTexture calls: 61,760 (vs 3,026) — aggressive cache invalidation via destroy+recreate
- SetTexture per-call: 856ns (vs 659ns) — texture stack overhead
- EndFrame: 16µs (vs 8.2µs) — zeroing 1024 structs + stack cleanup

### 2. Reverse Sort Tie-Breaking
Tested the original's reverse `original_index` tie-breaking (later submission draws first) vs forward (earlier draws first).

**Result: ~960µs on desktop (20% slower than forward)**
- No visual differences — sort direction doesn't affect this game
- Forward order produces better texture grouping → fewer batch breaks

### 3. SDL2D Optimizations Ported to Classic (insertion sort + texture sub-sort)
Ported adaptive insertion sort and texture sub-sort from SDL2D to Classic's AoS layout.

**Result: ~795µs desktop, ~225µs render cost on Pi4 (net worse)**
- AoS insertion sort copies 96-byte structs per swap (vs 4-byte indices in SoA) — kills cache advantage
- Texture sub-sort adds 4.6µs (desktop) / 23µs (Pi4) overhead but doesn't reduce BatchRender
- Sort saved only 3µs on Pi4 — not enough to offset overhead

**Lesson: SoA optimizations don't translate to AoS.** Struct copying overhead eats algorithmic gains.

---

## Architecture Comparison

| Feature | Classic | SDL2D |
|---------|---------|-------|
| Texture cache | Flat 2D array `[tex][pal]` | Multi-slot LRU per texture |
| Cache hit rate | 99.98% | Lower (LRU eviction) |
| Sorting | AoS qsort | SoA adaptive insertion + radix |
| Texture sub-sort | None (not worth it in AoS) | Within-Z-group by texture handle |
| Rect fast path | No | Yes (`SDL_RenderTexture`) |
| Software frame | No | Yes (harmful on both platforms) |
| Task storage | AoS `RenderTask` (96 bytes) | SoA parallel arrays |
| Complexity | ~672 lines | ~2,289 lines |

## Conclusion

Classic's flat texture cache with near-perfect hit rates (99.98%) is the decisive advantage. It eliminates expensive palette→RGBA blit + GPU upload on virtually all `SetTexture` calls. SDL2D's SwFrame, designed to reduce draw-call overhead, introduces CPU rasterization costs that exceed the overhead it replaces on both platforms. SDL2D's SoA sorting optimizations are effective but only in SoA layout — they regress when ported to AoS.

**The optimal Classic renderer is the simplest one:** flat cache, plain qsort, forward index tie-breaking, no texture stack, no software frame.
