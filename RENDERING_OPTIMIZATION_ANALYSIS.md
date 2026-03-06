# Character Rendering Pipeline — Optimization Analysis

> **Date**: 2026-03-06  
> **Scope**: Native in-game character sprite rendering (`mtrans.c`, `aboutspr.c`, `sdl_game_renderer*.c`)  
> **Status**: Optimizations #1 (frag shader palette, phase 1+2), #2 (hash cache + persistent atlas), #3 (inlined model transform), #4 (CG tile desc cache), #5 (SIMD transform), #6 (GPU compute LZ77), and #8 (double-buffer sprites) implemented. #7 (instanced rendering) pending.

## Current Pipeline

Every frame, for each visible character/effect WORK object:

```
sort_push_request()                       ← resolve color code, blink, visibility
  → Mtrans_use_trans_mode()               ← dispatch by MTS mode
    → mlt_obj_trans*() / mlt_obj_disp*()  ← tile map walk + LZ decode + cache
      → seqsStoreChip()                   ← fill Sprite2, resolve handles, transform
        → seqsAfterProcess()              ← upload dirty textures, batch flush
          → FlushSprite2Batch()           ← backend submission
```

## Renderer Backends

The game multiplexes through `SDLGameRenderer_*()` dispatch functions, routing to one of three backends:

| Backend | File | Capabilities | Target |
|---------|------|-------------|--------|
| **SDL GPU** | `sdl_game_renderer_gpu.c` | Texture arrays, palette atlas texture, fragment shader palette lookup (vertex `paletteIdx`), compute staging, triple-buffered vertex transfers, Z-sorted indexed draw | Modern desktop/console |
| **OpenGL** | `sdl_game_renderer_gl_*.c` | Render tasks with stable merge sort, per-sprite `SetTexture`/`DrawSprite2`, persistent-mapped VBOs, shader-based rendering | Desktop fallback |
| **SDL 2D** | `sdl_game_renderer_sdl.c` | `SDL_RenderGeometry` batched quads, `SDL_CreateTextureFromSurface` per tex+pal combo, palette cache (16 slots), insertion sort for near-sorted tasks | Low-end / Raspberry Pi |

## MTS Transformation Modes

| Mode | Function | Palette | LZ Decode | Pattern Cache | Description |
|------|----------|---------|-----------|---------------|-------------|
| **17** | `mlt_obj_trans()` | DC (Dreamcast) | ✅ Yes | ✅ Yes | Standard paletted characters |
| **18** | `mlt_obj_trans_cp3()` | CP3 (CPS3) | ✅ Yes | ✅ Yes | CPS3 palette mapped characters |
| **20** | `mlt_obj_trans_rgb()` | None (RGB) | ✅ Yes | ✅ Yes | Direct RGB (Gill, special FX) |
| **33** | `mlt_obj_disp()` | DC | ❌ No | ❌ No | Direct display, pre-decoded tiles |
| **36** | `mlt_obj_disp_rgb()` | None (RGB) | ❌ No | ❌ No | Direct RGB display |

Extended variants (`_ext`) exist for modes 17, 18, 20 — they add a `PatternCollection` cache for better tile reuse across CG frames.

---

## Per-Frame CPU Costs

| # | Step | Function | Per-frame cost | What happens |
|---|------|----------|---------------|--------------|
| 1 | Tile map walk | `mlt_obj_trans()` | ~20–60 tiles/char × N chars | Iterates `TileMapEntry[]` for current `cg_number` |
| 2 | LZ77 decompress | `lz_ext_p6_fx()` | Per cache-miss tile | Byte-by-byte LZ decode of 4bpp pixel data into `mltbuf` |
| 3 | Pattern cache lookup | `get_mltbuf16/32()` | Linear scan per tile | Walks entire `mltcsh16[]` looking for a match |
| 4 | CPU texture upload | `Renderer_UpdateTexture()` | Per dirty tile | Pushes decoded pixels to GPU surface |
| 5 | CPU palette→RGBA | `SetTexture()` in GPU/SDL backends | Per unique tex+pal combo | Converts indexed pixels to RGBA32 via palette loops |
| 6 | Per-chip transform | `njCalcPoints()` | Per tile (×2 points) | 4×4 matrix multiply per chip corner pair |
| 7 | Chip storage | `seqsStoreChip()` | Per tile | Fills `Sprite2`, resolves texture/palette handles |

**Steps 2, 5, and 6 are the most wasteful** — they re-do work the GPU could handle natively.

---

## Optimization Proposals

### 🔴 High Impact

#### 1. Fragment Shader Palette Lookup

**Eliminate step 5 entirely.**

The GPU backend currently does CPU-side palette conversion in `SDLGameRendererGPU_SetTexture()` — 3 nested loops turning indexed pixels → RGBA32 before upload. Instead:

- Upload raw 4bpp/8bpp indexed pixels as `R8` to the texture array
- Pass the palette row index per-vertex (already in `GPUVertex.paletteIdx`)
- Fragment shader: `texture(palette_atlas, vec2(index / 256.0, palIdx))`

**Savings**: Eliminates entire CPU palette conversion. Phase 2 (R8 format) will also reduce upload volume by **4×** (1 byte vs 4 bytes per pixel).

> [!NOTE]
> **Phase 1: ✅ IMPLEMENTED** — Fragment shader reads palette index from R channel of RGBA8 texture array and looks up color from palette atlas. `SetTexture()` writes `(idx, 0, 0, 0xFF)` for PSMT4/PSMT8 instead of CPU palette loops. PSMCT16/32-bit direct paths are unchanged. `FlushSprite2Batch` paletteIdx bug also fixed.
>
> **Phase 2: ✅ IMPLEMENTED** — Dual texture array architecture: `indexed_texture_array` (R8_UNORM, 1 byte/pixel) for PSMT4/PSMT8 and `direct_texture_array` (RGBA8) for PSMCT16/32-bit (Gill, special FX). Upload volume reduced **4×** for indexed textures. PSMT8 path is now a zero-conversion `memcpy`. PSMT4 uses SIMD nibble-to-byte unpack (no u32 expansion). Fragment shader uses 3 samplers: `IdxArray` (R8), `PaletteTex`, `DirArray` (RGBA8). Layer encoding uses sign-convention macros (`TEX_LAYER_IDX`/`TEX_LAYER_DIR`) for correct routing.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Primary target** | Already has palette atlas texture and `paletteIdx` in vertex format — just needs the fragment shader change and R8 upload path |
| **OpenGL** | ✅ **Yes** | Needs shader modification + palette texture setup, but GL has full shader capabilities |
| **SDL 2D** | ❌ **No** | No shader access — `SDL_RenderGeometry` is fixed-function. Must continue CPU palette→RGBA via `SDL_CreateTextureFromSurface` |

---

#### 2. Persistent Tile Atlas with Hash Map

> [!NOTE]
> **Hash cache portion: ✅ IMPLEMENTED** — `get_mltbuf16()` and `get_mltbuf32()` in `mtrans.c` now use O(1) hash-based lookup via `MltHashEntry` tables with Knuth multiplicative hashing and linear probing. Hash tables are cleared in `mlt_obj_trans_init()` and invalidated on eviction in `mlt_obj_trans_update()`.
>
> **Persistent tile cache: ✅ IMPLEMENTED** — Two changes make the cache persistent: (1) **Boost-on-hit**: cache hits now set `time = base_lifetime × 4` (`TILE_CACHE_BOOST`), so frequently-used tiles survive 4× longer between uses. (2) **LRU eviction fallback**: when the cache is full (no free slots), the slot with the lowest `time` is evicted instead of silently dropping the decode. Its hash entry is properly invalidated before the new tile overwrites it. Net effect: after warmup, most tile decodes are eliminated — tiles persist across animation cycles.

**Eliminate steps 2 + 4 on cache hits. Fix step 3.**

Currently `get_mltbuf16/32()` ~~uses time-eviction — tiles fall out and get re-decoded. The linear search is O(n).~~ uses hash-based O(1) lookup and persistent retention via boost-on-hit + LRU eviction (both implemented).

**Savings**: After warmup, most frames have **zero** LZ77 decode calls. O(1) vs O(n) per cache lookup.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Primary target** | Texture array already exists — extend it to act as a persistent atlas. Hash map replaces `get_mltbuf16/32()` in shared `mtrans.c` code |
| **OpenGL** | ✅ **Yes** | Can use GL texture atlas (already has per-texture caching). Hash map is backend-agnostic (lives in `mtrans.c`) |
| **SDL 2D** | ✅ **Yes** | The cache + hash map logic is in `mtrans.c` (shared). SDL 2D benefits from fewer `SDL_CreateTextureFromSurface` calls on cache misses |

---

#### 3. Vertex Shader Model Transform

**Eliminate step 6.**

Currently `njCalcPoints()` multiplies 2 points per chip through a 4×4 matrix on the CPU. Instead:

- Pass the character's model matrix as a per-draw-call vertex uniform
- Vertex shader applies `gl_Position = projMatrix * modelMatrix * vec4(pos, 1.0)`
- Store raw untransformed positions in `Sprite2`

**Savings**: Eliminates N × 2 CPU matrix multiplies per character. GPU does this for free.

> [!NOTE]
> **✅ IMPLEMENTED** — Replaced per-chip `njCalcPoints()` → `njCalcPoint()` ×2 function call chain with an inlined affine transform using pre-cached matrix elements. `mlt_obj_matrix()` now extracts the 9 relevant matrix coefficients (X/Y/Z rows) into statics after setup. `seqsStoreChip()` uses these directly: since input `z==0`, the z-row multiply is eliminated (saves 4 mults/chip). The per-chip Z increment (`appRenewTempPriority_1_Chip`) is also inlined as a single float add. `Sprite2` gains `modelX`/`modelY` fields for future GPU-side transform work. Net: eliminates function call overhead (NULL checks, loop, indirect calls), z-row mults, and reduces ~20 FLOPs + overhead → ~12 FLOPs flat per chip.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Yes** | Inlined transform is in shared `mtrans.c` — benefits all backends equally. Future phase could move offset to vertex shader |
| **OpenGL** | ✅ **Yes** | Same — shared code optimization |
| **SDL 2D** | ✅ **Yes** | Same — no shader dependency, pure CPU optimization |

---

### 🟡 Medium Impact

#### 4. SIMD Batch Transforms (alternative to #3)

If moving transforms fully to the vertex shader is too invasive, or to accelerate the SDL 2D backend, batch all chip corners into a contiguous buffer and transform with SSE/NEON intrinsics:

```c
// Instead of per-chip njCalcPoints():
njCalcPointsBatch(matrix, corners, count);  // SSE4: 4 transforms per instruction
```

> [!NOTE]
> **✅ IMPLEMENTED** — Combined with Opt3's inlined transform. Uses SIMDe (`simde/x86/sse.h` + `simde/x86/fma.h`) for portable SIMD: both points' X/Y are packed into a single `__m128` and transformed with two `_mm_fmadd_ps` operations (fused multiply-add). This processes 4 multiply-adds in parallel instead of 4 sequential scalar FMAs. Z stays scalar (2 FMAs — not worth vectorizing). SIMDe auto-translates to NEON on ARM/RPi4.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Yes** | Shared `mtrans.c` optimization — benefits all backends |
| **OpenGL** | ✅ **Yes** | Same |
| **SDL 2D** | ✅ **Yes** | Primary beneficiary — only way to accelerate transforms on this backend |

---

#### 5. CG Frame → Pre-built Tile Descriptor Cache

For each `cg_number`, the tile layout is deterministic (tile codes, relative positions, sizes). Pre-compute:

- List of tiles, UVs, and relative offsets per CG at load time
- At render time, apply flip + world position to the pre-built vertex array
- "Sprite sheet instancing" — CG defines the mesh, WORK defines the instance

**Savings**: Eliminates the per-frame tile map walk loop in `mlt_obj_trans()`.

> [!NOTE]
> **✅ IMPLEMENTED** — `CGTileDesc` struct caches per-tile cumulative X/Y offsets, TEX dimensions (`dw`, `dh`, `wh`), decoded tile sizes, and compressed data pointers. `CGTileCacheEntry` hash table (1024 slots, Knuth multiplicative hash with linear probing) stores pre-built arrays per `cg_number`. Three non-ext variants refactored: `mlt_obj_trans()`, `mlt_obj_trans_cp3()`, `mlt_obj_trans_rgb()`. The `_ext` variants (which already have `PatternCollection` caching) are left unchanged for now. Cache invalidated in `mlt_obj_trans_init()` when texture groups are reloaded.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Yes** | Shared `mtrans.c` optimization — benefits all backends equally |
| **OpenGL** | ✅ **Yes** | Same — tile map walk is backend-agnostic |
| **SDL 2D** | ✅ **Yes** | Same — and has biggest relative impact on low-end devices |

---

#### 6. GPU Compute Shader for LZ77 Decompression

Move `lz_ext_p6_fx()` / `lz_ext_p6_cx()` to a compute shader:

- Upload compressed tile data to a storage buffer
- Compute shader decompresses directly into the texture atlas
- The 4-case LZ format maps well to GPU compute

**Savings**: Frees CPU for game logic. Valuable on integrated GPUs where CPU↔GPU memory bandwidth is shared.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Primary target** | SDL GPU API has native compute shader support. Staging buffer infrastructure already exists (`s_compute_staging_buffer`) |
| **OpenGL** | ◐ **Possible** | Requires GL 4.3+ compute shaders. Not available on all targets (e.g., macOS, older hardware). Could use SSBO approach |
| **SDL 2D** | ❌ **No** | No GPU compute — must keep CPU decompression |

> [!NOTE]
> **✅ IMPLEMENTED** — GLSL compute shader (`lz77_decode.gpu.comp`) decompresses LZ77 tiles directly into the R8_UNORM indexed texture array via `imageStore`. One workgroup per tile, serial decoder matching the CPU's 4-case format (literal, short backref, long backref, packed nibbles). Includes `dctex_linear` swizzle LUT as a readonly storage buffer. Jobs queued per-frame via `Renderer_LZ77Enqueue()` with CPU fallback when staging is full or compute unavailable. Dispatch runs between copy and render passes. Swizzle table uploaded once on first use.

---

### 🟢 Lower Impact

#### 7. Instanced Rendering

Instead of 4 vertices per tile quad, use GPU instancing:

- Upload one unit quad (4 verts) once
- Per-instance data: position, UV rect, color, palette index, atlas layer
- Single `DrawInstanced(quad, N)` call

**Savings**: Vertex buffer shrinks from `4 × N vertices` to `N instance records`.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Yes** | SDL GPU API supports instanced drawing natively |
| **OpenGL** | ✅ **Yes** | `glDrawArraysInstanced` available since GL 3.3 |
| **SDL 2D** | ❌ **No** | `SDL_RenderGeometry` does not support instancing |

---

#### 8. Double-Buffer Sprite2 Array

Write next frame's `seqs_w.chip[]` while the GPU consumes the current frame. Currently `seqsBeforeProcess()` zeros and overwrites immediately.

**Savings**: Slight latency improvement on CPU-bound frames.

> [!NOTE]
> **✅ IMPLEMENTED** — `SpriteChipSet` now holds two `Sprite2` buffers (`chip_buf0`, `chip_buf1`). `seqsBeforeProcess()` swaps the active pointer each frame with `buf_index ^= 1`. `seqsGetUseMemorySize()` doubled to `0x1A000` to accommodate both buffers. Cost: one pointer swap + XOR per frame.

**Backend applicability**:

| Backend | Applicable | Notes |
|---------|-----------|-------|
| **SDL GPU** | ✅ **Yes** | Already triple-buffers vertex transfers — extending to Sprite2 array is natural |
| **OpenGL** | ✅ **Yes** | Uses persistent-mapped VBOs — can double-buffer the source array |
| **SDL 2D** | ✅ **Yes** | Benefits any backend — logic is in shared `mtrans.c` |

---

## Recommended Priority Order

| Priority | Optimization | Effort | Impact | Status |
|----------|-------------|--------|--------|--------|
| **1** | Fragment shader palette lookup | Medium | High — eliminates CPU palette conversion, 4× less upload | **✅ Done** (Phase 1: CPU elim · Phase 2: R8 dual arrays) |
| **2** | Persistent tile atlas + hash cache | Medium | High — eliminates repeated LZ77 decode | **✅ Done** (hash cache + boost-on-hit + LRU eviction) |
| **3** | Vertex shader model transform | Low | Medium — eliminates CPU matrix math | **✅ Done** (inlined transform) |
| **4** | CG frame pre-built tile desc cache | Medium | Medium — eliminates tile map walk | **✅ Done** (non-ext variants) |
| **5** | SIMD batch transforms | Low | Medium — fallback if #3 too invasive | **✅ Done** (SIMDe FMA) |
| **6** | GPU compute LZ77 | High | Medium — frees CPU, complex to implement | **✅ Done** (compute shader + CPU fallback) |
| **7** | Instanced rendering | Medium | Low — reduces vertex buffer size | Pending |
| **8** | Double-buffer sprite array | Low | Low — minor latency win | **✅ Done** |

## Benefit Summary by Renderer Backend

| Optimization | SDL GPU | OpenGL | SDL 2D |
|-------------|:---:|:---:|:---:|
| 1. Frag shader palette | ✅ Primary | ✅ Yes | ❌ No shaders |
| 2. Persistent atlas + hash | ✅ Yes | ✅ Yes | ✅ Yes (shared) |
| 3. Inlined model xform | ✅ Yes (shared) | ✅ Yes (shared) | ✅ Yes (shared) |
| 4. SIMD transforms | ✅ Yes (shared) | ✅ Yes (shared) | ✅ Yes (shared) |
| 5. CG vertex cache | ✅ Yes | ✅ Yes | ✅ Yes (shared) |
| 6. GPU compute LZ77 | ✅ Primary | ◐ GL 4.3+ | ❌ No compute |
| 7. Instanced rendering | ✅ Yes | ✅ Yes | ❌ No instancing |
| 8. Double-buffer sprites | ✅ Yes (shared) | ✅ Yes (shared) | ✅ Yes (shared) |

> ✅ = Full benefit · ◐ = Partial/conditional · ❌ = Not applicable
