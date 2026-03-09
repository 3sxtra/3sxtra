---
name: concise-planning
description: "Turn a user request into a clear, actionable, atomic checklist. Adapted for C game engine PRD-driven workflows."
---

# Concise Planning

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/concise-planning) for the 3SX C game engine.

## Goal

Turn a user request into a **single, actionable plan** with atomic steps.

## Workflow

### 1. Scan Context

- Read `PRD.md`, `progress.txt`, and relevant source files
- Identify constraints (C only, behavior-preserving, lint/build/test gates)
- Check which renderers are affected (GL, GPU, SDL, classic)

### 2. Minimal Interaction

- Ask **at most 1–2 questions** and only if truly blocking
- Make reasonable assumptions for non-blocking unknowns

### 3. Generate Plan

Use the following structure:

## Plan Template

```markdown
# Plan

<High-level approach in 1-3 sentences>

## Scope

- In: <what changes>
- Out: <what is NOT touched>

## Action Items

[ ] <Step 1: Discovery/research>
[ ] <Step 2: Implementation>
[ ] <Step 3: Implementation>
[ ] <Step 4: Validation — .\recompile.bat>
[ ] <Step 5: Validation — .\lint.bat>
[ ] <Step 6: Validation — ctest --output-on-failure>

## Open Questions

- <max 3 questions>
```

## Checklist Guidelines

| Rule | Example |
|------|---------|
| **Atomic** | Each step is a single logical unit of work |
| **Verb-first** | "Extract...", "Add...", "Remove...", "Verify..." |
| **Concrete** | Name specific files: `sdl_app.c`, `CMakeLists.txt` |
| **Testable** | Every plan ends with verification steps |

## 3SX Conventions

Every plan MUST include these verification steps at the end:
1. `.\recompile.bat` succeeds
2. `.\lint.bat` passes
3. `cd build_tests && ctest --output-on-failure` passes

Plans MUST respect:
- No game logic changes unless explicitly requested
- No breaking changes to public API signatures
- All `static` state stays `static` unless cross-file access is needed
- New files added to `CMakeLists.txt`
