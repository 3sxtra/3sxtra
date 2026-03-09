---
name: code-review-checklist
description: "Structured code review for C game engine changes. Covers functionality, safety, performance, tests, and style. Adapted for SDL2/OpenGL/CPS3."
---

# Code Review Checklist

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/code-review-checklist) for the 3SX C game engine.

## When to Use

- Reviewing any code change before committing
- After `/medic` health passes
- After module decomposition or extraction
- Before merging feature branches
- Complement to `/judge` workflow

## Review Process

### Step 1: Understand Context

- [ ] What problem does this change solve?
- [ ] What files are touched? (`git diff --stat`)
- [ ] Is this behavior-preserving or a new feature?
- [ ] Does the PRD specify acceptance criteria?

### Step 2: Functionality

- [ ] Does the code do what the commit message says?
- [ ] Are edge cases handled? (NULL pointers, zero-length, overflow)
- [ ] Are error paths tested and logged?
- [ ] Does it break any existing public API signatures?
- [ ] For emulation: does it preserve game parity?

### Step 3: C Safety & Memory

- [ ] No buffer overflows (bounds-checked array access)
- [ ] No use-after-free (freed pointers set to NULL)
- [ ] No memory leaks (every `malloc`/`SDL_CreateTexture` has a matching free)
- [ ] No uninitialized variables
- [ ] `static` linkage for file-local functions and data
- [ ] No inline `extern` declarations — use proper headers
- [ ] `const` correctness on read-only parameters

### Step 4: Performance

- [ ] No allocations in hot paths (frame loop, render, input poll)
- [ ] No unnecessary `SDL_Log` in frame-critical code
- [ ] No redundant texture creation/destruction per frame
- [ ] GPU resource cleanup handled in shutdown path
- [ ] integer math preferred over floating-point where appropriate

### Step 5: Style & Standards

- [ ] `.\lint.bat` passes (clang-tidy + clang-format)
- [ ] Naming follows `SDLAppModule_Function()` convention
- [ ] File-local state uses `static` and `s_` prefix
- [ ] New files have `@file` / `@brief` doc comments
- [ ] New `.c` files added to `CMakeLists.txt`
- [ ] No magic numbers — use named constants or enums
- [ ] Comments explain *why*, not *what*

### Step 6: Tests

- [ ] `ctest --output-on-failure` passes
- [ ] New public functions have test coverage
- [ ] Existing tests still pass without modification
- [ ] Test names are descriptive (`test_function_does_expected_thing`)

### Step 7: Build Verification

- [ ] `.\recompile.bat` succeeds (all renderers)
- [ ] `.\lint.bat` passes without new warnings
- [ ] `cd build_tests && ctest --output-on-failure` passes

## Common Issues to Watch For

| Issue | Where to Look |
|-------|---------------|
| Inline `extern` instead of header include | Any `.c` file |
| Dead `#include` after code extraction | File that code was moved FROM |
| Missing `CMakeLists.txt` entry | Any new `.c` or test file |
| Leaked GPU resources | Init/shutdown function pairs |
| State shared via global when it should be `static` | New modules |
| `printf`/`fprintf` instead of `SDL_Log` | Any file |

## Review Comment Templates

**Requesting changes:**
> This introduces a raw `extern` on line N. Please move the declaration to the appropriate header and `#include` it instead.

**Asking questions:**
> Why was this changed from `static` to non-static? Is it needed outside this file?

**Praising good code:**
> Clean extraction — the new API surface (`SDLAppBezel_RenderGL/GPU`) is minimal and well-named.
