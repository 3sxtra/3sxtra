---
name: c-cpp-pro
description: "Write idiomatic C and modern C++ for game engine code. Covers C99/C11 patterns, memory management, const correctness, and C++ for wrapper layers (RmlUi). Adapted for SDL2/OpenGL/CPS3."
---

# C/C++ Pro

> Adapted from [antigravity-awesome-skills/cpp-pro](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/cpp-pro) for the 3SX engine, which is primarily C with C++ wrapper layers.

## When to Use

- Writing or reviewing C code in the engine/port layer
- Writing C++ code in the RmlUi wrapper layer
- Making memory management decisions
- Optimizing performance-critical paths
- Setting up CMake build targets

## C Focus Areas (primary language)

### Idiomatic C Patterns

- **C99/C11** standard features (designated initializers, `_Static_assert`, `restrict`)
- `static` linkage for file-local functions and data
- `const` correctness on pointers: `const char *name`, `char *const ptr`
- Enums for named constants, not `#define` magic numbers
- Struct-based APIs with clear ownership semantics

### Memory Management

```c
// Pattern: allocate, use, free — all in same scope when possible
SDL_Surface *surf = SDL_CreateRGBSurface(...);
if (!surf) { SDL_Log("Alloc failed: %s", SDL_GetError()); return; }
// ... use surf ...
SDL_FreeSurface(surf);

// Pattern: init/shutdown pairs for module-level resources
void SDLAppBezel_InitGPU(SDL_GPUDevice *dev) {
    s_pipeline = SDL_CreateGPUGraphicsPipeline(dev, &info);
    s_sampler  = SDL_CreateGPUSampler(dev, &sampler_info);
}
void SDLAppBezel_Shutdown(void) {
    SDL_ReleaseGPUGraphicsPipeline(s_device, s_pipeline);
    SDL_ReleaseGPUSampler(s_device, s_sampler);
    s_pipeline = NULL;
    s_sampler  = NULL;
}
```

### Header Design

```c
// sdl_app_scale.h
#pragma once

typedef enum { SCALE_FIT, SCALE_INTEGER, SCALE_STRETCH, SCALE_COUNT } ScaleMode;

// Public API — only what callers need
const char *scale_mode_name(ScaleMode mode);
ScaleMode   cycle_scale_mode(ScaleMode current);
ScaleMode   config_string_to_scale_mode(const char *str);
SDL_FRect   get_letterbox_rect(int window_w, int window_h);
```

### Error Handling

```c
// Guard clauses with SDL_Log
if (!texture) {
    SDL_Log("ERROR: texture is NULL in render_frame");
    return;
}

// Assert for programmer errors (debug builds only)
assert(renderer != NULL && "renderer must be initialized before calling render");

// Return codes for recoverable errors
int result = save_screenshot(renderer, w, h);
if (result != 0) { SDL_Log("Screenshot save failed"); }
```

## C++ Focus Areas (RmlUi wrapper only)

The RmlUi integration layer uses C++17. Follow these patterns:

- **RAII** for RmlUi documents and contexts
- **`std::string`** at C++ boundary, `const char*` at C boundary
- **No exceptions** across the C/C++ boundary
- **`extern "C"`** for all functions called from C code
- Smart pointers (`std::unique_ptr`) for C++-owned resources

```cpp
// RmlUi wrapper: C++ internally, C API externally
extern "C" {
    void rmlui_load_document(const char *path);
    void rmlui_update(float dt);
    void rmlui_render(void);
}
```

## Build & Tooling

### CMake Conventions

```cmake
# New source files must be added to the target
target_sources(3sxtra PRIVATE
    src/port/sdl/app/sdl_app_scale.c
    src/port/sdl/app/sdl_app_bezel.c
)
```

### Compiler Flags

- `-Wall -Wextra` for all builds
- `-Werror` in CI (clang-tidy via `.\lint.bat`)
- `-O2` for release, `-O0 -g` for debug
- AddressSanitizer/UBSan for debugging memory issues

### Static Analysis

```powershell
.\lint.bat    # clang-tidy + clang-format
```

## Anti-Patterns

| ❌ Don't | ✅ Do |
|----------|-------|
| `extern int foo;` inline in `.c` files | Declare in header, `#include` it |
| `printf()` / `fprintf(stderr, ...)` | `SDL_Log()` |
| `#define SCALE_FIT 0` | `typedef enum { SCALE_FIT, ... } ScaleMode;` |
| Raw `malloc`/`free` when SDL API exists | `SDL_CreateTexture` / `SDL_DestroyTexture` |
| Global non-static state | `static` file-local unless truly shared |
| `void*` for type erasure | Concrete typed pointers |
