---
name: systematic-debugging
description: "4-phase debugging for C game engines: root-cause investigation, pattern analysis, hypothesis testing, implementation. Adapted for SDL2/OpenGL/CPS3 emulation."
---

# Systematic Debugging

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/systematic-debugging) for the 3SX C game engine.

## The Iron Law

```
NO FIXES WITHOUT ROOT CAUSE INVESTIGATION FIRST
```

If you haven't completed Phase 1, you cannot propose fixes.

## When to Use

Use for ANY technical issue in the 3SX codebase:
- Rendering glitches (GL, GPU, SDL renderer)
- Netplay desync or rollback failures
- Emulation parity regressions
- Build / lint / ctest failures
- RmlUi layout or binding issues
- Audio crackle, timing drift
- Performance regressions

**Use ESPECIALLY when:**
- Under time pressure
- "Just one quick fix" seems obvious
- Previous fix didn't work
- You've already tried 2+ fixes

## The Four Phases

Complete each phase before proceeding.

### Phase 1: Root Cause Investigation

**BEFORE attempting ANY fix:**

1. **Read error output carefully**
   - `.\recompile.bat` errors: read the full compiler message, note file + line
   - `.\lint.bat` warnings: check clang-tidy rule name
   - `ctest` failures: read the CMocka assertion output
   - SDL_Log / stderr output for runtime crashes

2. **Reproduce consistently**
   - Can you trigger it reliably?
   - Which renderer? (GL / GPU / SDL / classic)
   - Which game mode? (training, vs, replay, netplay)
   - Which character/stage combination?

3. **Check recent changes**
   ```bash
   git diff HEAD~3 --stat
   git log --oneline -10
   ```
   - What changed that could cause this?
   - Any new `#include` or `extern` additions?
   - Any struct layout changes?

4. **Gather evidence in multi-layer systems**
   - For rendering: add `SDL_Log` at each pipeline stage
   - For netplay: log state checksums at frame boundaries
   - For emulation: compare against known-good replay
   - For RmlUi: check data model bindings with `SDL_Log`

5. **Trace data flow**
   - Where does the bad value originate?
   - Trace backward through call stack
   - Fix at source, not at symptom

### Phase 2: Pattern Analysis

1. **Find working examples** — locate similar working code in the codebase
2. **Compare against references** — read reference implementation completely
3. **Identify differences** — list every difference, however small
4. **Understand dependencies** — what headers, externs, state does this need?

### Phase 3: Hypothesis and Testing

1. **Form single hypothesis** — "I think X is the root cause because Y"
2. **Test minimally** — smallest possible change, ONE variable at a time
3. **Verify before continuing** — did it work? If not, form NEW hypothesis

### Phase 4: Implementation

1. **Create failing test case** (CMocka if possible)
   ```c
   static void test_bug_repro(void **state) {
       // Simplest possible reproduction
       assert_int_equal(actual, expected);
   }
   ```

2. **Implement single fix** — ONE change at a time, no "while I'm here" improvements

3. **Verify fix**
   ```powershell
   .\recompile.bat
   .\lint.bat
   cd build_tests && ctest --output-on-failure
   ```

4. **If fix doesn't work after 3 attempts** — STOP and question the architecture. Discuss with your human partner before attempting more fixes.

## Red Flags — STOP and Return to Phase 1

If you catch yourself thinking:
- "Quick fix for now, investigate later"
- "Just try changing X and see if it works"
- "Add multiple changes, run tests"
- "It's probably X, let me fix that"
- "One more fix attempt" (when already tried 2+)
- Proposing solutions before tracing data flow

## 3SX-Specific Debugging Tools

| Tool | Use For |
|------|---------|
| `SDL_Log()` | Runtime diagnostics (all platforms) |
| `.\lint.bat` | Static analysis (clang-tidy) |
| `.\recompile.bat` | Incremental build verification |
| `ctest --output-on-failure` | CMocka unit test suite |
| `xxd` (`C:\Program Files\Git\usr\bin\xxd.exe`) | Binary/hex inspection |
| `git diff` | Recent change analysis |

## Quick Reference

| Phase | Key Activities | Success Criteria |
|-------|---------------|------------------|
| **1. Root Cause** | Read errors, reproduce, check changes | Understand WHAT and WHY |
| **2. Pattern** | Find working examples, compare | Identify differences |
| **3. Hypothesis** | Form theory, test minimally | Confirmed or new hypothesis |
| **4. Implementation** | Create test, fix, verify | Bug resolved, all checks pass |
