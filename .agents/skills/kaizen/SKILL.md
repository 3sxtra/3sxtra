---
name: kaizen
description: "Continuous improvement for C game engine code: small incremental changes, error-proofing, standardized patterns, and YAGNI. Adapted for medic/health passes."
---

# Kaizen: Continuous Improvement

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/kaizen) for the 3SX C game engine.

## Overview

Small improvements, continuously. Error-proof by design. Follow what works. Build only what's needed.

**Core principle:** Many small improvements beat one big change. Prevent errors at design time, not with fixes.

## When to Use

**Always applied for:**
- Code implementation and refactoring
- `/medic` health passes and PRD-driven tasks
- Architecture and design decisions
- Error handling and validation
- File decomposition (extracting modules from god files)

## The Four Pillars

### 1. Continuous Improvement (Kaizen)

**Incremental over revolutionary:**
- Make smallest viable change that improves quality
- One improvement at a time
- Verify each change before next (`.\recompile.bat`, `.\lint.bat`, `ctest`)
- Build momentum through small wins

**Always leave code better:**
- Fix small issues as you encounter them
- Replace inline `extern` declarations with proper `#include`
- Remove dead `#include` lines when you spot them
- Update stale comments, remove `// ... existing ...` markers

**Iterative refinement (the medic loop):**
1. First version: make it work
2. Second pass: make it clear (extract, rename, document)
3. Third pass: make it efficient (profile first!)
4. Don't try all three at once

**In practice for 3SX:**
```c
// Iteration 1: Make it work — inline in sdl_app.c
static void save_screenshot(SDL_Renderer *r, int w, int h) { ... }

// Iteration 2: Make it clear — extract to sdl_app_screenshot.c
void SDLAppScreenshot_RequestCapture(void) { ... }
void SDLAppScreenshot_ProcessPending(SDL_Renderer *r, int w, int h) { ... }

// Iteration 3: Make it robust — add error handling
void SDLAppScreenshot_ProcessPending(...) {
    if (!s_pending) return;
    SDL_Surface *surf = SDL_RenderReadPixels(...);
    if (!surf) { SDL_Log("Screenshot failed: %s", SDL_GetError()); return; }
    ...
}
```

### 2. Poka-Yoke (Error Proofing)

Design systems that prevent errors at compile time, not runtime.

**In C, error-proof with:**
- `const` correctness on pointers and parameters
- `static` linkage for file-local state (don't expose what isn't needed)
- `assert()` for preconditions in debug builds
- Enums over magic numbers: `ScaleMode` not `int mode = 2`
- Header guards: one include, one declaration site
- `_Static_assert` for struct layout assumptions

**Defense in layers:**
1. Compiler warnings (`-Wall -Wextra -Werror` via clang-tidy)
2. Static analysis (`.\lint.bat`)
3. Guards (precondition asserts)
4. Runtime checks (SDL_Log + early return)

**Make invalid states impossible:**
```c
// Bad: raw int, any value allowed
int scale_mode = 3;  // what does 3 mean?

// Good: enum, only valid values compile
typedef enum { SCALE_FIT, SCALE_INTEGER, SCALE_STRETCH, SCALE_COUNT } ScaleMode;
ScaleMode scale_mode = SCALE_FIT;
```

### 3. Standardized Work

Follow established patterns. Document what works.

**3SX conventions:**
- Public API: `SDLAppModule_FunctionName()` prefix pattern
- File-local state: `static` variables, prefixed `s_`
- New files: must be added to `CMakeLists.txt`
- Headers: `#pragma once` or include guards
- Doc comments: `@file` / `@brief` on new files
- Error logging: `SDL_Log()` (not printf, not fprintf)

**Before adding new patterns:**
- Search codebase for how similar problems are solved
- Match existing file structure and naming
- Follow same error handling approach
- Check `PRD.md` for conventions

### 4. Just-In-Time (JIT)

Build what's needed now. No more, no less.

**YAGNI for 3SX:**
- Don't add renderer backends nobody asked for
- Don't abstract until 3+ similar cases exist (Rule of Three)
- Don't optimize without profiling first (use `/bolt`)
- Don't add config options "just in case"

**When to add complexity:**
- Current requirement demands it
- Pain points identified through use
- Measured performance issues (frame time > budget)
- Multiple use cases have emerged

## Red Flags

| Violation | Symptom |
|-----------|---------|
| **Anti-Kaizen** | "I'll refactor it later" / big bang rewrites |
| **Anti-Poka-Yoke** | Magic numbers, no validation, inline externs |
| **Anti-Standard** | Breaking naming conventions, ignoring lint |
| **Anti-JIT** | Building frameworks before using them, premature optimization |

## Remember

**Kaizen is about:** Small improvements continuously, preventing errors by design, following proven patterns, building only what's needed.

**Not about:** Perfection on first try, massive refactoring projects, clever abstractions, premature optimization.

**Mindset:** Good enough today, better tomorrow. Repeat.
