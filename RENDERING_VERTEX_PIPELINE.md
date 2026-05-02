# Rendering Vertex Pipeline

> **Status**: ✅ Complete — Documentation + all future work investigated and resolved  
> **Created**: 2026-04-25  
> **Last Updated**: 2026-05-02  
> **Scope**: Vertex data flow from game code → backend GPU submission

## Overview

The vertex pipeline has **three layers**, each with its own data types.
Game code produces PS2-era structs; the port layer converts them; backends
submit native GPU vertices. Understanding this flow is essential before
any vertex-format unification work.

```
┌────────────────────────────────────────────────────────────────────┐
│                    GAME CODE (sf33rd/)                             │
│                                                                    │
│  Sprite {v[4], t[4], tex_code}     ← textured quad, 4 corners     │
│  Sprite2 {v[2], t[2], vtx_color,  ← optimized: 2 corners (TL+BR) │
│           tex_code, id, modelX/Y}    + per-char model offset       │
│  Quad {v[4]}                       ← solid-color quad (no texture) │
│  RendererVertex {x,y,z,u,v,color}  ← convenience wrapper          │
│  Vertex {x,y,z,s,t}               ← legacy PS2 struct             │
└────────────────┬───────────────────────────────────────────────────┘
                 │  rendering/renderer.c (Renderer_*)
                 │  Sprite → SDLGameRenderer_DrawSprite()
                 │  RendererVertex → Sprite reconversion (see §3)
                 ▼
┌────────────────────────────────────────────────────────────────────┐
│               PORT LAYER (port/sdl/renderer/)                      │
│                                                                    │
│  renderer.c    ← PS2 shim: SetTexture, 2D prim queue, PPG lookup  │
│  sdl_game_renderer.c  ← vtable dispatch → backend Init/Draw/...   │
└────────────────┬───────────────────────────────────────────────────┘
                 │  g_game_renderer->DrawSprite(sprite, color)
                 │  g_game_renderer->DrawSprite2(sprite2)
                 │  g_game_renderer->FlushSprite2Batch(...)
                 ▼
┌────────────────────────────────────────────────────────────────────┐
│                        BACKENDS                                    │
│                                                                    │
│  GL:    SDL_Vertex {pos, color, tex_coord}                         │
│         + batch_layers[]      (float per-vertex: texture array)    │
│         + batch_pal_indices[] (float per-vertex: palette row)      │
│         + batch_z[]           (float per-vertex: depth)            │
│         = 4 separate attribute streams, indexed draw               │
│                                                                    │
│  GPU:   GPUVertex {x,y, r,g,b,a, u,v, layer, paletteIdx}          │
│         = 1 interleaved stream (40 bytes), indexed draw            │
│                                                                    │
│  SDL2D: SDL_Vertex {pos, color, tex_coord}                         │
│         → SDL_RenderGeometry() call per quad                       │
│                                                                    │
│  Classic: SDL_RenderTexture() (no vertex assembly)                 │
└────────────────────────────────────────────────────────────────────┘
```

---

## §1 — Game-Side Types

### Sprite (primitives.h:10)
```c
typedef struct Sprite {
    Vec3 v[4];           // 4 corner positions (x,y,z)
    TexCoord t[4];       // 4 tex coords (s,t)
    unsigned int tex_code; // combined handle: low 16 = texture, high 16 = palette
} Sprite;
```
**Size**: 4×12 + 4×8 + 4 = **84 bytes**  
**Used by**: Most game drawing — characters, effects, UI sprites.

### Sprite2 (primitives.h:16)
```c
typedef struct Sprite2 {
    Vec3 v[2];              // 2 corners (top-left + bottom-right)
    TexCoord t[2];          // 2 tex coords (TL + BR)
    unsigned int vertex_color;
    unsigned int tex_code;
    unsigned int id;        // unique sprite ID for sorting
    float modelX, modelY;  // per-sprite model-space offset
} Sprite2;
```
**Size**: 2×12 + 2×8 + 4 + 4 + 4 + 8 = **60 bytes**  
**Used by**: Batched character sprite system (`FlushSprite2Batch`). More compact than Sprite — backends expand 2 corners → 4 vertices.

### Quad (primitives.h:6)
```c
typedef struct Quad { Vec3 v[4]; } Quad;
```
**Size**: **48 bytes**  
**Used by**: Solid-color rectangles (shadows, fade overlays, 2D prim queue).

### RendererVertex (structs.h:1454)
```c
typedef struct {
    f32 x, y, z, u, v;
    u32 color;
} RendererVertex;
```
**Size**: **24 bytes**  
**Used by**: `DrawTexturedQuadVtx`, `DrawSpriteVtx`, `DrawSolidQuadVtx` — convenience functions for callers that don't have Sprite structs. These are **reconverted back to Sprite** in the port layer (see §3).

### Vertex (structs.h:1463) — Legacy
```c
typedef struct { f32 x, y, z, s, t; } Vertex;
```
**Size**: **20 bytes**  
**Used by**: `ppgWriteQuadWithST_B2()` PPG draw path (PS2 legacy). Only used as a temporary in `renderer.c` for the PPG fallback path.

---

## §2 — Backend Vertex Types

### GL: SDL_Vertex + Attribute Streams

The GL backend uses SDL3's `SDL_Vertex` for position/color/texcoord, plus three parallel float arrays for metadata:

```
Stream 0: SDL_Vertex batch_vertices[N×4]
  ├── .position  {x, y}        (float×2)
  ├── .color     {r, g, b, a}  (float×4)
  └── .tex_coord {x, y}        (float×2)

Stream 1: float batch_layers[N×4]      — texture array layer index
Stream 2: float batch_pal_indices[N×4]  — palette TBO slot (or -1)
Stream 3: float batch_z[N×4]            — depth for sorting
```

**Why split streams?** The GL shader uses `gl_VertexID` to index into separate `samplerBuffer` objects for palette lookup. Interleaving would require a custom vertex format change in the VAO setup and wouldn't benefit cache performance at this batch size (~500 quads/frame).

**Batching**: Up to `RENDER_TASK_MAX` (8192) quads per frame. Pre-computed index buffer (6 indices per quad: `0,1,2, 2,1,3`). Sorted by Z-depth, drawn in batches by texture/blend state.

### GPU: GPUVertex (Interleaved)

```c
typedef struct GPUVertex {
    float x, y;          // position (2D)
    float r, g, b, a;    // vertex color
    float u, v;          // texture coordinates
    float layer;         // texture array layer
    float paletteIdx;    // palette row in atlas, or -1.0
} GPUVertex;             // 40 bytes, 10 floats
```

**Why interleaved?** The GPU backend uses SDL_GPU which prefers a single vertex buffer with a declared vertex layout. All per-vertex data is packed together. The shader reads palette and layer inline.

**Batching**: Up to `MAX_VERTICES` (65536) vertices = 16384 quads. Z-sorting uses separate `QuadSortKey` array per pass.

**Per-Pass Queue Buckets (FrameGraph Phase 1):** Quads are routed into 3 pass buckets:
- Pass 0: CPS3 Canvas (game sprites)
- Pass 1: HD Backgrounds (overlays with z ≥ 0.1)
- Pass 2: HD Foregrounds (overlays with z < 0.1)

Each pass has its own `pass_sort_keys[p][MAX_QUADS]` and `pass_quad_count[p]` arrays. The `QuadSortKey` struct contains:
```c
typedef struct {
    float z;                // Z-depth for sorting
    int original_index;     // quad index in per-pass submission order
    int global_quad_index;  // global vertex array index (vertex_count / 4)
    RendererBlendMode blend_mode;
} QuadSortKey;
```
`global_quad_index` maps each per-pass sort key back to the unified vertex buffer.

### SDL2D: SDL_Vertex (Per-Quad Draw)

Uses SDL3's `SDL_RenderGeometry()` with 4 vertices + 6 indices per call. No batching — each quad is a separate draw call. Simple but sufficient for the fallback renderer.

### Classic: No Vertices

Uses `SDL_RenderTexture()` directly from Sprite rects. No vertex assembly at all.

---

## §3 — The RendererVertex Reconversion Problem

There is a round-trip conversion happening for the `*Vtx` entry points:

```
Caller has RendererVertex[4]
    │
    │  Renderer_DrawSpriteVtx() — rendering/renderer.c:191
    │    → copies 4× RendererVertex fields → Sprite struct
    ▼
Sprite
    │
    │  PortRenderer_DrawSprite() — port/sdl/renderer/renderer.c:146
    │    → may reconvert Sprite → Vertex[4] for PPG path
    │    → otherwise passes Sprite directly to backend
    ▼
Backend converts Sprite → native vertex (SDL_Vertex / GPUVertex)
```

**Observation**: For the `*Vtx` path, the data travels:
1. Caller packs `RendererVertex` (24B × 4 = 96B)
2. Port layer unpacks to `Sprite` (84B)
3. Backend unpacks `Sprite` to native vertex (varies)

This is **three copy operations** for the same data. However:
- The `*Vtx` path is a minority of calls (< 5% of draw calls)
- The main path (`Renderer_DrawSprite` → `SDLGameRenderer_DrawSprite`) passes `Sprite*` directly — only one copy (backend unpacking)
- The PPG fallback in `renderer.c` needs the `Vertex` format anyway

**Recommendation**: Keep both paths. The `*Vtx` convenience wrappers serve a real purpose (callers that construct vertices procedurally, not from game-data Sprite structs). The overhead is negligible at current draw volumes.

---

## §4 — Texture Handle Encoding

The `tex_code` field in Sprite/Sprite2 encodes both texture and palette:

```
tex_code bits:
  [15:0]  = texture handle (1-based, 0 = invalid)
  [31:16] = palette handle (0 = no palette / direct RGBA)
```

The port layer resolves this in `Renderer_SetTexture()`:
- PPG index lookup via `s_CurrentTexture->handle[ix]`
- Combined handle construction: `texHandle | (palHandle << 16)`
- Stored as `tex_code` in Sprite, passed to backend

Each backend then splits `tex_code` to look up its texture array layer and palette slot/row.

---

## §5 — Per-Backend Conversion Details

### GL Backend (sdl_game_renderer_gl_draw.c)

For each `DrawSprite(sprite, color)`:
1. Push 4 `SDL_Vertex` entries to `batch_vertices[]`
   - `position.x/y` from `sprite->v[i].x/y`
   - `tex_coord.x/y` from `sprite->t[i].s/t` (scaled by texture dimensions)
   - `color` from packed u32 → normalized RGBA
2. Push 4 floats each to `batch_layers[]`, `batch_pal_indices[]`, `batch_z[]`
3. Create `RenderTask` with texture handle, blend mode, Z-depth
4. At frame end: sort `RenderTask[]` by Z, batch by texture/blend, `glDrawElements()`

### GPU Backend (sdl_game_renderer_gpu.c)

For each `DrawSprite(sprite, color)`:
1. Map transfer buffer, write 4 `GPUVertex` entries
   - Position, color, texcoords packed inline
   - `layer` = texture array layer
   - `paletteIdx` = palette atlas row
2. Create `QuadSortKey` for Z-sorting (routed to pass bucket 0 for canvas sprites)
3. At frame end: sort keys per pass, build index buffer using `global_quad_index`, upload, `SDL_DrawGPUIndexedPrimitives()`

### SDL2D Backend

For each `DrawSprite(sprite, color)`:
1. Build 4 `SDL_Vertex` + 6 indices
2. Call `SDL_RenderGeometry()` immediately (no batching)

---

## §6 — Data Flow Summary Table

| Entry Point | Game Type | Port Copy? | Backend Type | Copies |
|------------|-----------|-----------|--------------|--------|
| `Renderer_DrawSprite()` | Sprite* | No | SDL_Vertex/GPUVertex | 1 |
| `Renderer_DrawSprite2()` | Sprite2* | No | (expanded to 4 verts) | 1 |
| `Renderer_DrawTexturedQuad()` | Sprite* | No | SDL_Vertex/GPUVertex | 1 |
| `Renderer_DrawSolidQuad()` | Quad* | No | SDL_Vertex/GPUVertex | 1 |
| `Renderer_DrawSpriteVtx()` | RendererVertex* | → Sprite | SDL_Vertex/GPUVertex | 2 |
| `Renderer_DrawTexturedQuadVtx()` | RendererVertex* | → Sprite | SDL_Vertex/GPUVertex | 2 |
| `Renderer_DrawSolidQuadVtx()` | RendererVertex* | → Quad | SDL_Vertex/GPUVertex | 2 |
| `Renderer_FlushSprite2Batch()` | Sprite2[] | No | bulk expand | 1 |
| `Renderer_Queue2DPrimitive()` | float[] | → Quad | SDL_Vertex/GPUVertex | 2 |

---

## §7 — Future Work (Investigated — All Resolved)

1. **Remove RendererVertex middleman** — ✅ **Closed (won't do)**. `scrscrntex` is a global `RendererVertex[4]` shared across 8 UI files. Migrating 37 call sites would require renaming hundreds of `.x/.y/.z/.u/.v` field accesses to `.v[i].x/.t[i].s`. The `*Vtx` path handles <5% of draw calls; overhead is negligible. `training_hud.c` was migrated to `Quad + DrawSolidQuad` as an isolated case (2026-05-02).

2. **Unify GL and GPU vertex layout** — ✅ **Closed (won't do)**. GL uses split attribute streams (SDL_Vertex + 3 float arrays) optimized for its VAO setup and `samplerBuffer` palette lookup. GPU uses interleaved `GPUVertex` (40B) optimized for SDL_GPU's single-buffer layout. Both backends are already heavily optimized with SIMD. A shared struct would require changing GL VAO setup, shader attribute bindings, and persistent mapping logic for marginal benefit (2026-05-02).

3. **Sprite2 batch optimization** — ✅ **Closed (not needed)**. The `active_layers` check in `FlushSprite2Batch` appears to iterate all 8192 chips, but in practice only `sprTotal` (~100-300) are submitted. More importantly, the `active_layers[id]` check is **not redundant**: `seqs_w.up[id]` can be reset to 0 by `ppgRenewTexChunkSeqs()` during texture upload if a texture fails validation (mtrans.c:2069-2072). Removing the check would cause rendering of sprites with invalid texture data (2026-05-02).

4. **Zero-copy path** — Unchanged. CPS3 emulation data (tex_code, palette indirection) makes this impractical without significant game-code changes.
