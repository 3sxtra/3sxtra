---
name: memory-safety-patterns
description: "Memory-safe C programming: init/shutdown pairs, guard clauses, leak prevention, and resource lifecycle management. Adapted for SDL2/GPU/CPS3 emulation."
---

# Memory Safety Patterns

> Adapted from [antigravity-awesome-skills/memory-safety-patterns](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/memory-safety-patterns) for the 3SX C game engine.

## When to Use

- Allocating or freeing SDL resources (textures, surfaces, GPU pipelines)
- Managing CPS3 emulation state and memory regions
- Writing init/shutdown pairs for new modules
- Debugging crashes (segfault, use-after-free, double-free)
- Reviewing code for resource leaks

## Core Patterns

### 1. Init/Shutdown Pairs

Every module that allocates resources must have a matching shutdown:

```c
// sdl_app_bezel.c — resource lifecycle
static SDL_GPUGraphicsPipeline *s_pipeline = NULL;
static SDL_GPUSampler          *s_sampler  = NULL;
static SDL_GPUBuffer           *s_vbo      = NULL;

void SDLAppBezel_InitGPU(SDL_GPUDevice *dev) {
    s_pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    s_sampler  = SDL_CreateGPUSampler(dev, &sampler_info);
    s_vbo      = SDL_CreateGPUBuffer(dev, &buf_info);
}

void SDLAppBezel_Shutdown(void) {
    if (s_pipeline) { SDL_ReleaseGPUGraphicsPipeline(s_device, s_pipeline); s_pipeline = NULL; }
    if (s_sampler)  { SDL_ReleaseGPUSampler(s_device, s_sampler);           s_sampler  = NULL; }
    if (s_vbo)      { SDL_ReleaseGPUBuffer(s_device, s_vbo);               s_vbo      = NULL; }
}
```

**Rules:**
- NULL-check before release (idempotent shutdown)
- Set pointer to NULL after release (prevent use-after-free)
- Call shutdown in reverse order of init
- Shutdown must be safe to call multiple times

### 2. Guard Clauses

Validate inputs at function entry:

```c
void render_bezel(SDL_Renderer *renderer, const BezelConfig *config) {
    if (!renderer) { SDL_Log("ERROR: NULL renderer in render_bezel"); return; }
    if (!config)   { SDL_Log("ERROR: NULL config in render_bezel");   return; }
    if (config->width <= 0) { return; }  // nothing to render
    // ... safe to proceed
}
```

### 3. Scoped Resource Cleanup

For resources with function-local lifetime, use the goto-cleanup pattern:

```c
int save_screenshot(SDL_Renderer *renderer, int w, int h) {
    int result = -1;
    SDL_Surface *surf = NULL;
    FILE *fp = NULL;

    surf = SDL_CreateRGBSurface(0, w, h, 32, ...);
    if (!surf) { SDL_Log("Surface alloc failed"); goto cleanup; }

    if (SDL_RenderReadPixels(renderer, NULL, surf->format->format, surf->pixels, surf->pitch) != 0) {
        SDL_Log("ReadPixels failed: %s", SDL_GetError());
        goto cleanup;
    }

    fp = fopen("screenshot.bmp", "wb");
    if (!fp) { SDL_Log("File open failed"); goto cleanup; }

    SDL_SaveBMP_RW(surf, SDL_RWFromFP(fp, 0), 1);
    result = 0;  // success

cleanup:
    if (fp)   fclose(fp);
    if (surf) SDL_FreeSurface(surf);
    return result;
}
```

### 4. Ownership Rules

| Pattern | Owner | Cleanup |
|---------|-------|---------|
| Module `static` state | The module's Init/Shutdown | Shutdown function |
| Function-local allocations | The function | goto-cleanup or early return |
| Caller-allocated buffers | The caller | Caller frees after use |
| SDL-managed handles | SDL | Paired SDL_Destroy/Release call |

**Never** transfer ownership implicitly. If a function takes ownership, document it:
```c
// Takes ownership of `surface` — caller must NOT free it after this call
void set_bezel_texture(SDL_Surface *surface);
```

### 5. Common Leak Sources in 3SX

| Resource | Allocator | Deallocator |
|----------|-----------|-------------|
| `SDL_Surface` | `SDL_CreateRGBSurface` | `SDL_FreeSurface` |
| `SDL_Texture` | `SDL_CreateTexture` | `SDL_DestroyTexture` |
| `SDL_GPUGraphicsPipeline` | `SDL_CreateGPUGraphicsPipeline` | `SDL_ReleaseGPUGraphicsPipeline` |
| `SDL_GPUBuffer` | `SDL_CreateGPUBuffer` | `SDL_ReleaseGPUBuffer` |
| `SDL_GPUSampler` | `SDL_CreateGPUSampler` | `SDL_ReleaseGPUSampler` |
| `SDL_GPUTexture` | `SDL_CreateGPUTexture` | `SDL_ReleaseGPUTexture` |
| `malloc`/`calloc` | stdlib | `free` |

### 6. Debugging Memory Issues

**Tools:**
- AddressSanitizer (`-fsanitize=address`) — catches use-after-free, buffer overflow
- UndefinedBehaviorSanitizer (`-fsanitize=undefined`) — catches UB
- SDL_Log at alloc/free sites for manual tracking
- Valgrind (Linux builds)

**Symptoms → Likely Cause:**

| Symptom | Likely Cause |
|---------|-------------|
| Crash in `SDL_RenderPresent` | Destroyed texture still referenced |
| Corruption after resize | Stale pointer to reallocated buffer |
| Leak on exit | Missing shutdown call or missing resource in shutdown |
| Random crash on level change | Static state not reset between games |

## Anti-Patterns

| ❌ Don't | ✅ Do |
|----------|-------|
| Free without NULL-check | `if (ptr) { free(ptr); ptr = NULL; }` |
| Return allocated memory without documenting ownership | Document who frees |
| Allocate in a loop without freeing | Allocate once, reuse |
| Use pointer after free | Set to NULL immediately after free |
| Rely on process exit for cleanup | Explicit shutdown in all paths |
