# Rendering Abstraction Plan — bgfx-Inspired Improvements

> **Status**: Rounds 1 & 2 Complete — Round 3 Pending  
> **Created**: 2026-04-19  
> **Last Updated**: 2026-04-25  
> **Inspiration**: [bgfx](https://github.com/bkaradzic/bgfx) architecture patterns  
> **Scope**: 3sxtra (NOT 3sxtra-arcade)

## Context

3sxtra's rendering layer supports 4 backends (OpenGL, SDL_GPU, SDL2D, SDL2D Classic).
~~The dispatch mechanism is a 300-line if/else chain in `sdl_game_renderer.c` that calls
`SDLApp_GetRenderer()` 18+ times per frame.~~ **DONE** — replaced with vtable dispatch.
bgfx solves this with typed handles, a virtual renderer interface, runtime caps, view
tables, and unified vertex layouts.

This plan cherry-picks those patterns across 3 implementation rounds.

---

## Architecture Overview (Current State)

```
Game Code (sf33rd/)
    │
    ▼
rendering/game_renderer.h          ← Public API: Renderer_CreateTexture(), Renderer_DrawSprite(), etc.
    │
    ▼
rendering/renderer.c               ← Thin shim: converts Sprite/Quad → RendererVertex, calls SDLGameRenderer_*()
    │
    ▼
port/sdl/renderer/sdl_game_renderer.c  ← ✅ SOLVED: vtable dispatch via g_game_renderer
    │
    ├──► sdl_game_renderer_gl_*.c       (OpenGL backend — ~90k lines total)
    ├──► sdl_game_renderer_gpu_*.c      (SDL_GPU backend — ~100k lines)
    ├──► sdl_game_renderer_sdl.c        (SDL2D backend — ~84k lines)
    └──► sdl_game_renderer_classic.c    (SDL2D Classic — ~43k lines)
```

### Key Files

| File | Role | Size |
|------|------|------|
| `src/include/rendering/game_renderer.h` | Public renderer API (game-facing) | 20 lines |
| `src/include/rendering/primitives.h` | Sprite, Sprite2, Quad structs | 27 lines |
| `src/include/port/rendering/renderer.h` | Port-layer renderer API (RendererVertex, blend modes) | 72 lines |
| `src/include/port/sdl/renderer/sdl_game_renderer.h` | SDL renderer public API | 57 lines |
| `src/port/sdl/renderer/sdl_game_renderer_internal.h` | Per-backend function declarations (148 lines — IS the vtable, just not stored as one) | 148 lines |
| `src/port/sdl/renderer/sdl_game_renderer.c` | if/else dispatch (THE PROBLEM FILE) | 302 lines |
| `src/include/port/renderer_plugin.h` | HD plugin vtable (renderer_export_t) — ALREADY uses the pattern we want | 125 lines |
| `src/port/renderer_plugin.c` | DLL plugin loader | 133 lines |
| `src/include/structs.h:1454-1461` | RendererVertex: `{x,y,z,u,v,color}` | 8 lines |
| `src/port/sdl/renderer/sdl_game_renderer_gl_internal.h` | GL backend state: `GLRendererState`, `RenderTask`, uses `SDL_Vertex` | 171 lines |
| `src/port/sdl/renderer/sdl_game_renderer_gpu_internal.h` | GPU backend state: `GPUVertex {x,y,r,g,b,a,u,v,layer,paletteIdx}` | 228 lines |
| `src/port/sdl/app/sdl_app.h:17-22` | `RendererBackend` enum: OPENGL, SDLGPU, SDL2D, SDL2D_CLASSIC | 6 lines |

### Existing Pattern to Follow

`renderer_plugin.h` already implements the exact vtable pattern we want for the base renderer:
```c
typedef struct renderer_export_t {
    int api_version;
    bool (*Init)(int argc, const char** argv);
    void (*Shutdown)(void);
    bool (*TryRenderSprite)(...);
    // ...
} renderer_export_t;
extern renderer_export_t* g_renderer_plugin;
```

---

## Round 1: Vtable Dispatch + RendererCaps ✅

**Goal**: Eliminate the if/else dispatch chain. Add runtime capability detection.

### Change 1A: GameRendererVtable ✅ COMPLETE

**New file**: `src/include/port/game_renderer_vtable.h`

Define a struct with function pointers matching every function in `sdl_game_renderer_internal.h`.
The vtable must cover these 19 functions (extracted from the current if/else chain):

```c
typedef struct GameRendererVtable {
    // Lifecycle (6)
    void (*Init)(void);
    void (*Shutdown)(void);
    void (*BeginFrame)(void);
    void (*RenderFrame)(void);
    void (*RenderHDPass)(int vp_x, int vp_y, int vp_w, int vp_h, bool bg_only);
    void (*EndFrame)(void);

    // Texture management (7)
    void (*CreateTexture)(unsigned int th);
    void (*DestroyTexture)(unsigned int texture_handle);
    void (*UnlockTexture)(unsigned int th);
    void (*CreatePalette)(unsigned int ph);
    void (*DestroyPalette)(unsigned int palette_handle);
    void (*UnlockPalette)(unsigned int ph);
    void (*SetTexture)(unsigned int th);

    // State (1)
    void (*SetBlendMode)(RendererBlendMode mode);

    // Drawing (5)
    void (*DrawTexturedQuad)(const Sprite* sprite, unsigned int color);
    void (*DrawSolidQuad)(const Quad* vertices, unsigned int color);
    void (*DrawSprite)(const Sprite* sprite, unsigned int color);
    void (*DrawSprite2)(const Sprite2* sprite2);
    void (*FlushSprite2Batch)(Sprite2* chips, const unsigned char* active_layers, int count);

    // Debug (2)
    unsigned int (*GetCachedGLTexture)(unsigned int texture_handle, unsigned int palette_handle);
    void (*DumpTextures)(void);
} GameRendererVtable;

extern const GameRendererVtable* g_renderer;
void GameRendererVtable_Init(void);
```

**Modify file**: `src/port/sdl/renderer/sdl_game_renderer.c`

Replace the 300-line if/else chain with:

1. Four static const vtable instances:
```c
static const GameRendererVtable s_vtable_gl = {
    .Init = SDLGameRendererGL_Init,
    .Shutdown = SDLGameRendererGL_Shutdown,
    .BeginFrame = SDLGameRendererGL_BeginFrame,
    // ... all 19 fields
};
// Repeat for GPU, SDL, Classic
```

2. `GameRendererVtable_Init()` picks one based on `SDLApp_GetRenderer()`.
3. Each `SDLGameRenderer_*()` function becomes a one-liner: `g_renderer->Xxx(args)`.

**Special cases (resolved)**:
- `SDLGameRenderer_DumpTextures()` — ✅ GL's `DumpTextures` now calls `DumpPaletteStats()` internally. Pure one-liner dispatch.
- `Renderer_LZ77Available()` / `Renderer_LZ77Enqueue()` — ✅ Kept as standalone (Option B).

**Implementation notes**:
- Global pointer: `const GameRendererVtable* g_game_renderer` (in `sdl_game_renderer.c`)
- 21-slot vtable (19 base + 3 overlay slots added in Round 2)
- `SDLApp_GetRenderer()` calls in dispatch file reduced from 18+ to 3 (1 vtable init + 2 LZ77 standalone)

### Change 1B: RendererCaps ✅ COMPLETE

**Header**: `src/include/port/renderer_caps.h`

**Implementation**: Inlined in `sdl_game_renderer.c` (avoids adding a new .c file to the build).

```c
typedef struct RendererCaps {
    bool is_gles;
    bool has_get_tex_level_param;  // Added: glGetTexLevelParameteriv availability
    bool has_persistent_mapping;
    bool has_texture_arrays;
    bool has_compute_shaders;
    bool has_pbo;
    uint32_t max_texture_size;
    uint32_t max_array_layers;
} RendererCaps;
```

**Implementation notes**:
- `RendererCaps_Detect()` called during `SDLGameRenderer_Init()`, after vtable selection, before backend Init.
- For GL: probes `GLAD_GL_ARB_*` extension booleans, `glGetIntegerv()` limits, and `GL_COMPAT_ES` for GLES detection.
- For GPU: sets known GPU defaults (compute=true, texture_arrays=true).
- For SDL2D: minimal defaults.
- Added `has_get_tex_level_param` field — needed for `sdl_texture_util.cpp` GLES workaround.

**Files updated** (replaced `#ifdef __ANDROID__` with runtime caps):
- `sdl_text_renderer_gl.c` — ✅ shader path selection uses `g_renderer_caps.is_gles` 
- `sdl_texture_util.cpp` — ✅ all 5 `#ifdef __ANDROID__` blocks replaced with `g_renderer_caps.has_get_tex_level_param`

### Change 1C: TextRendererVtable ✅ COMPLETE (added post-plan)

**New header**: `src/include/port/text_renderer_vtable.h`

9-slot vtable (8 common + `DrawDebugChars` GL-only, NULL for other backends).
3 static const instances (GL, GPU, SDL2D). `DrawDebugBuffer` uses NULL-check on `DrawDebugChars` for batched GL path vs per-char fallback.

`SDLApp_GetRenderer()` calls in `sdl_text_renderer.c` reduced from 10 to 1.

### Round 1 Verification

- ✅ Build passes (Release, incremental via recompile.bat).
- Runtime testing pending.

---

## Round 2: Overlay API Unification ✅

**Goal**: Bring the `DrawOverlaySprite*` functions under the vtable with a backend-agnostic handle.

### The Problem

Overlay sprite functions have **different signatures per backend**:

```c
// GL: takes GLuint texture ID
void SDLGameRendererGL_DrawOverlaySprite(unsigned int gl_texture_id, float x, float y, float w, float h, float z);
void SDLGameRendererGL_DrawOverlaySpriteEx(unsigned int gl_texture_id, float x, float y, float w, float h, float z, int flip_x, int flip_y);
void SDLGameRendererGL_DrawOverlaySubSprite(unsigned int gl_texture_id, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float z);

// GPU: takes raw pixel pointer + dimensions (uploads each frame)
void SDLGameRendererGPU_DrawOverlaySprite(const uint32_t* pixels, int tex_w, int tex_h, float x, float y, float w, float h, float z);
void SDLGameRendererGPU_DrawOverlaySpriteEx(const uint32_t* pixels, int tex_w, int tex_h, float x, float y, float w, float h, float z, int flip_x, int flip_y);

// SDL2D: takes SDL_Texture*
void SDLGameRendererSDL_DrawOverlaySprite(SDL_Texture* texture, float x, float y, float w, float h, float z);

// GPU also has deferred blit variants:
void SDLGameRendererGPU_QueueDeferredBlit(SDL_GPUTexture* texture, int tex_w, int tex_h, ...);
```

### Solution: Opaque Overlay Handle

Introduce an `OverlayTexture` type that wraps the backend's native handle:

**New file**: `src/include/port/overlay_texture.h`

```c
typedef struct OverlayTexture {
    void* native_handle;  // GLuint (cast to void*), SDL_Texture*, or SDL_GPUTexture*
    const uint32_t* cpu_pixels;  // For GPU backend (uploads pixels each frame)
    int width;
    int height;
} OverlayTexture;
```

**Add to vtable** (extend `GameRendererVtable`):
```c
    // Overlay drawing (added in Round 2)
    void (*DrawOverlaySprite)(OverlayTexture* tex, float x, float y, float w, float h, float z);
    void (*DrawOverlaySpriteEx)(OverlayTexture* tex, float x, float y, float w, float h, float z, int flip_x, int flip_y);
    void (*DrawOverlaySubSprite)(OverlayTexture* tex, float x, float y, float w, float h, float u0, float v0, float u1, float v1, float z);
```

**Files to update**:
- `src/port/sdl/renderer/sprite_override.c` — currently dispatches overlays manually
- `src/port/sdl/renderer/sdl_texture_util.cpp` — creates and manages overlay textures
- `src/port/sdl/renderer/sdl_game_renderer_internal.h` — add unified declarations
- Each backend's overlay implementation — wrap native handle extraction

**Callers** (places that currently call DrawOverlaySprite with backend-specific types):
- Search for `DrawOverlaySprite` in: `sprite_override.c`, `sdl_texture_util.cpp`, `stage_bg/` files, `appear/` files

### Round 2 Verification

- ✅ HD sprite overlays render correctly on GL and GPU backends.
- ✅ Background tile overrides render correctly.
- ✅ Renderer plugin system (DLL) still works.

**Implementation notes**:
- `OverlayTexture` was simplified to `typedef void* OverlayTexture` — the proposed struct was unnecessary since each backend knows how to interpret its own handle.
- 3 overlay function pointers added to `GameRendererVtable`: `DrawOverlayQuad`, `DrawOverlayQuadEx`, `DrawOverlaySubQuadEx`.

---

## Round 3: Unified Vertex Format + Render Pass Table

### Change 3A: Vertex Format Documentation & Alignment

**Goal**: Document and optionally unify the vertex format pipeline.

#### Current Vertex Types

```
Game code produces:
    RendererVertex {float x, y, z, u, v; uint32_t color}  (structs.h:1454)
    Vertex {float x, y, z, s, t}                           (structs.h:1463, legacy)
    Sprite {Vec3 v[4]; TexCoord t[4]; uint tex_code}       (primitives.h:10)
    Sprite2 {Vec3 v[2]; TexCoord t[2]; uint vtx_color, tex_code, id; float modelX,Y}  (primitives.h:16)

Backends consume:
    GL backend:  SDL_Vertex (from SDL3) + batch_layers[] + batch_pal_indices[] + batch_z[] as separate attribute streams
    GPU backend: GPUVertex {float x,y, r,g,b,a, u,v, layer, paletteIdx}  (10 floats, 40 bytes)
    SDL2D:       SDL_Vertex (position + color + tex_coord, from SDL3)
    Classic:     SDL_Vertex
```

#### Conversion Pipeline

```
Sprite/Sprite2/Quad
    │  (rendering/renderer.c — converts to RendererVertex)
    ▼
RendererVertex {x,y,z,u,v,color}
    │  (port/sdl/renderer/renderer.c — calls SDLGameRenderer_DrawSprite)
    │  (NOTE: this step unpacks RendererVertex BACK to Sprite, which is wasteful)
    ▼
SDLGameRenderer_DrawSprite(Sprite*, color)
    │  (each backend converts Sprite → its native vertex format)
    ▼
GL: SDL_Vertex + attribute arrays
GPU: GPUVertex
```

> **Key insight**: `rendering/renderer.c` converts `Sprite` → `RendererVertex` → then
> `port/sdl/renderer/renderer.c` converts `RendererVertex` → `Sprite` → passes to backend.
> This double-conversion is unnecessary. The Sprite/Sprite2 types could go directly to backends.

#### Proposed Changes

1. **Document** the vertex pipeline in a `RENDERING_VERTEX_PIPELINE.md` or as comments.

2. **Consider removing the RendererVertex middleman** for the main path:
   - `game_renderer.h` already passes `Sprite*` directly for most functions
   - The `RendererVertex` path (`DrawTexturedQuadVtx`, `DrawSpriteVtx`, `DrawSolidQuadVtx`) exists for a few callers that don't have Sprite structs
   - Keep both paths but mark `RendererVertex` variants as "convenience wrappers"

3. **Align GPU vertex with GL vertex** where possible:
   - Both need: position (x,y), depth (z), tex coords (u,v), color (r,g,b,a), layer index, palette index
   - GL uses separate attribute arrays for layer/palette; GPU packs them interleaved
   - A shared `RendererGPUVertex` struct definition would help documentation, but the actual memory layout may need to differ for performance

#### Files Involved
- `src/include/structs.h` (RendererVertex definition at line 1454)
- `src/include/rendering/primitives.h` (Sprite, Sprite2, Quad)
- `src/rendering/renderer.c` (Sprite→RendererVertex conversion)
- `src/port/sdl/renderer/renderer.c` (RendererVertex→Sprite reconversion)
- `src/port/sdl/renderer/sdl_game_renderer_gl_internal.h` (RenderTask, SDL_Vertex usage)
- `src/port/sdl/renderer/sdl_game_renderer_gpu_internal.h` (GPUVertex)
- `src/port/sdl/renderer/sdl_game_renderer_gl_draw.c` (GL vertex assembly)
- `src/port/sdl/renderer/sdl_game_renderer_gpu.c` (GPU vertex assembly)

### Change 3B: Render Pass Table

**Goal**: Declare render passes as data instead of imperative FBO/render-target switching.

#### Current Pass Structure

**GL backend** (`sdl_game_renderer_gl_draw.c` RenderFrame):
```
Pass 0: CPS3 Canvas (384×224 FBO)
  - Bind cps3_canvas_fbo
  - Clear color+depth
  - Sort quads by Z
  - Batch draw: texture array path (indexed textures) + legacy path (direct RGBA)
  - Handle blend mode changes (normal → additive → multiply)

Pass 1: Screen Upscale
  - Bind screen FBO (or default FBO)
  - Draw fullscreen quad with canvas texture
  - Apply post-processing shader (CRT, scanlines, etc.)

Pass 2: HD Overlay (optional, via RenderHDPass)
  - Bind same target as Pass 1
  - Draw HD sprite/tile overrides on top
```

**GPU backend** (`sdl_game_renderer_gpu.c` RenderFrame):
```
Pass 0: Texture/Palette Uploads
  - Upload dirty textures to texture array via copy pass
  - Upload dirty palettes to palette atlas

Pass 1: CPS3 Canvas (384×224 render target)
  - Begin render pass on canvas texture
  - Sort quads by Z
  - Batch draw with palette shader

Pass 2: Screen Blit
  - Begin render pass on swapchain texture
  - Draw canvas to screen
  - Draw deferred HD blits

Pass 3: Present
  - Submit command buffer
```

#### Proposed Pass Table

```c
typedef struct RenderPass {
    const char* name;           // "CPS3 Canvas", "Screen Upscale", "HD Overlay"
    uint16_t width, height;     // Render target dimensions (0 = backbuffer size)
    bool clear_color;
    bool clear_depth;
    uint32_t clear_color_value; // RGBA
    float clear_depth_value;
    // Backend fills in native handles:
    void* framebuffer;          // GLuint FBO / SDL_GPUTexture* / NULL for backbuffer
} RenderPass;

typedef struct RenderPassTable {
    RenderPass passes[8];       // Max 8 passes
    int count;
} RenderPassTable;
```

**Where this helps**:
- Adding a CRT shader pass = add one entry to the table
- Adding scanline overlay = add one entry
- Adding split-screen = duplicate canvas pass with different viewport
- Per-pass GPU profiling becomes trivial (iterate passes, time each)

**Where this is complex**:
- GL and GPU backends have very different resource upload strategies (GL uses PBO, GPU uses transfer buffers)
- The "upload pass" in GPU backend isn't really a render pass — it's a copy pass
- Shader binding is per-draw-call, not per-pass

#### Recommendation

Start with a **read-only pass descriptor** that documents the pass structure without controlling it:

```c
// Filled by each backend during Init, read by profiler/debug tools
extern RenderPassTable g_render_passes;
```

Then gradually move pass setup (FBO bind, clear, viewport) into a shared `RenderPass_Begin(pass_index)` / `RenderPass_End(pass_index)` API as backends converge.

#### Files Involved
- `src/port/sdl/renderer/sdl_game_renderer_gl_draw.c` (GL render loop)
- `src/port/sdl/renderer/sdl_game_renderer_gpu.c` (GPU render loop, GPU setup)
- `src/port/sdl/renderer/sdl_game_renderer_sdl.c` (SDL2D render loop)
- `src/port/sdl/renderer/sdl_game_renderer_classic.c` (Classic render loop)
- New: `src/include/port/render_pass.h`

### Round 3 Verification

- Document vertex pipeline (no code changes needed for documentation)
- If removing RendererVertex middleman: verify all callers of `DrawTexturedQuadVtx`, `DrawSpriteVtx`, `DrawSolidQuadVtx` still work
- If adding pass table: verify per-pass profiling data is correct
- Build + visual test all 4 backends

---

## Reference: bgfx Patterns Applied

| bgfx Pattern | 3sxtra Round | Adaptation |
|--------------|-------------|------------|
| `RendererContextI` (virtual dispatch) | Round 1 | `GameRendererVtable` function-pointer struct |
| `bgfx::Caps` (runtime capabilities) | Round 1 | `RendererCaps` struct |
| `BGFX_HANDLE(TextureHandle)` (typed handles) | Round 2 | `OverlayTexture` opaque handle for overlay system |
| `bgfx::VertexLayout` (vertex format) | Round 3 | Vertex pipeline documentation + optional alignment |
| `bgfx::setViewRect/setViewFrameBuffer` (view system) | Round 3 | `RenderPassTable` descriptor |
| `bgfx::Encoder` (command buffer) | **Not planned** | Overkill for ~500 quads/frame |
| Multi-threaded submission | **Not planned** | Workload too small to benefit |

---

## Dependency Graph

```
Round 1A (Vtable) ──────────► Round 2 (Overlay Unification)
                                    │
Round 1B (Caps) ◄── independent ──► │
                                    │
                              Round 3A (Vertex Format)
                              Round 3B (Render Pass Table) ◄── independent
```

Round 1A and 1B are independent of each other but both should complete before Round 2.
Round 3A and 3B are independent of each other and can be done in any order after Round 2.

---

## Open Questions (Resolved)

### Round 1 (Resolved)
1. `g_game_renderer` is `const GameRendererVtable*` — prevents mutation. ✅
2. LZ77: standalone with backend check (Option B). ✅
3. `DumpPaletteStats()`: folded into GL's `DumpTextures`. ✅

### Round 2 (Resolved)
4. `OverlayTexture` simplified to `void*` — just references native handle, no RAII. ✅
5. Plugin DLL not affected — overlay system is internal to the port layer. ✅

### Round 3 (Open)
6. CRT/scanline/post-processing passes — **deferred** until pass table is needed.
7. `RendererVertex` → `Sprite` double-conversion — **document first**, optimize later.

---

## Remaining `SDLApp_GetRenderer()` Dispatch Calls

| File | Count | Classification |
|------|-------|---------------|
| `sdl_game_renderer.c` | 3 | ✅ 1 vtable init + 2 LZ77 standalone |
| `sdl_texture_util.cpp` | 1 | ✅ vtable init only (was 9 — solved by `TextureUtilVtable`) |
| `sdl_text_renderer.c` | 1 | ✅ vtable init only |
| `sdl_app_shader_config.c` | 3 | ✅ shader pipeline config (app layer) |
| `sdl_bezel.c` | 2 | ⚠️ `SetTextureNearest()` — per-backend texture filtering |
| `librashader_manager.c` | 1 | ✅ one-time init |
| `sdl_app_input.c` | 1 | ✅ feature guard |
| `rmlui_wrapper.cpp` | 1 | ✅ one-time init |

**Next candidate**: `sdl_bezel.c` — 2 calls in `SetTextureNearest()` for per-backend filtering. Low priority (runs once per bezel load, not per-frame). Could add `SetFilterMode` to `TextureUtilVtable`.
