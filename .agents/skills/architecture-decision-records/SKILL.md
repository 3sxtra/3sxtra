---
name: architecture-decision-records
description: "Document significant architecture decisions with context, alternatives, and consequences. Adapted for C game engine evolution and port layer design."
---

# Architecture Decision Records (ADRs)

> Adapted from [antigravity-awesome-skills/architecture-decision-records](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/architecture-decision-records) for the 3SX C game engine.

## When to Use

- Choosing between renderer backends (GL vs GPU vs SDL)
- Adding new subsystems (netplay, RmlUi, replay)
- Changing module boundaries (decomposing god files)
- Selecting third-party dependencies
- Modifying the build system or toolchain
- Any decision that would be hard to reverse later

## When NOT to Use

- Routine refactoring within established patterns
- Bug fixes that follow existing conventions
- Style/formatting choices (handled by clang-format)

## ADR Template

```markdown
# ADR-NNN: <Title>

**Date:** YYYY-MM-DD
**Status:** Proposed | Accepted | Deprecated | Superseded by ADR-NNN
**Context:**

<What is the issue? What forces are at play? 2-4 sentences.>

**Decision:**

<What we decided and why. Be specific about what we will do.>

**Alternatives Considered:**

1. <Alternative A> — <why rejected>
2. <Alternative B> — <why rejected>

**Consequences:**

- <Positive consequence>
- <Negative consequence / trade-off>
- <Follow-up work needed>
```

## Example: ADR for 3SX

```markdown
# ADR-001: Extract sdl_app.c into sub-modules

**Date:** 2026-03-09
**Status:** Accepted
**Context:**

`sdl_app.c` has grown to 2400+ lines containing scale-mode logic,
bezel rendering, screenshot capture, and debug HUD — all unrelated
to its core responsibility of init/quit/frame-loop orchestration.
This makes the file difficult to navigate and increases merge conflicts.

**Decision:**

Decompose into `sdl_app_scale.c`, `sdl_app_bezel.c`,
`sdl_app_screenshot.c`, and `sdl_app_debug_hud.c`. Each module
exposes a minimal public API via its own header. All `static` state
moves with its functions. sdl_app.c retains only orchestration.

**Alternatives Considered:**

1. Single header refactor (keep one file, add sections) — rejected
   because it doesn't reduce file size or improve modularity.
2. Move to C++ classes — rejected because the codebase is C and
   the overhead of a language switch is unjustified.

**Consequences:**

- sdl_app.c drops to ~1500 lines (positive)
- 8 new files to maintain (minor overhead)
- All public APIs use `SDLAppModule_Function()` naming (consistency)
- Must update CMakeLists.txt for each new .c file
```

## Directory Structure

Store ADRs in the project docs directory:

```
docs/
  adr/
    0001-extract-sdl-app-submodules.md
    0002-rmlui-integration-approach.md
    0003-gpu-renderer-backend.md
    README.md   # index of all ADRs
```

## ADR Index (README.md)

```markdown
# Architecture Decision Records

| ADR | Title | Status | Date |
|-----|-------|--------|------|
| 001 | Extract sdl_app.c into sub-modules | Accepted | 2026-03-09 |
| 002 | RmlUi integration approach | Accepted | 2026-02-15 |
| 003 | GPU renderer backend | Proposed | 2026-03-01 |
```

## Best Practices

| ✅ Do | ❌ Don't |
|-------|----------|
| Write ADR BEFORE implementing | Document decisions after the fact |
| Keep it short (1 page max) | Write novels |
| List concrete alternatives | Only describe the chosen option |
| State trade-offs honestly | Pretend there are no downsides |
| Update status when superseded | Delete old ADRs |
| Reference ADR number in PRDs | Leave decisions undocumented |

## Integration with 3SX Workflow

1. **When planning:** reference relevant ADRs in `PRD.md`
2. **When reviewing:** check if change aligns with accepted ADRs
3. **When proposing new architecture:** write ADR first, get approval
4. **When reversing a decision:** mark old ADR as "Superseded by ADR-NNN"
