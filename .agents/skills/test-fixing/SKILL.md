---
name: test-fixing
description: "Systematically fix failing ctest/CMocka tests using smart error grouping. Fix infrastructure first, then API changes, then logic bugs."
---

# Test Fixing

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/test-fixing) for the 3SX C game engine using CMocka + CTest.

## When to Use

- `ctest` reports failures after refactoring
- Build succeeds but tests break
- Decomposition moved code and broke includes
- CI pipeline fails on test stage

## Systematic Approach

### 1. Initial Test Run

```powershell
cd build_tests && ctest --output-on-failure
```

Analyze output for:
- Total number of failures
- Error types (compile error, link error, assertion failure, crash)
- Affected test files

### 2. Smart Error Grouping

Group similar failures by:

| Priority | Category | Examples |
|----------|----------|----------|
| **1st** | Build / link errors | Missing `#include`, undefined symbol, unresolved external |
| **2nd** | Header / API changes | Function signature changed, struct renamed, enum moved |
| **3rd** | Logic / assertion failures | `assert_int_equal` mismatch, wrong return value |
| **4th** | Runtime crashes | Segfault, null pointer, buffer overflow |

Fix highest priority group first — infrastructure fixes often cascade and resolve downstream failures.

### 3. Fixing Process (per group)

1. **Identify root cause**
   ```powershell
   git diff HEAD~1 --stat
   cd build_tests && ctest --output-on-failure -R test_name
   ```

2. **Implement fix**
   - Update `#include` paths for moved headers
   - Fix function signatures to match new API
   - Add missing test dependencies to `CMakeLists.txt`
   - Minimal, focused changes only

3. **Verify this group passes**
   ```powershell
   cd build_tests && ctest --output-on-failure -R "pattern"
   ```

4. **Move to next group** — don't mix groups

### 4. Fix Order Strategy

**Infrastructure first:**
- Missing `#include` for new headers (e.g., `sdl_app_scale.h`)
- Undefined symbols from function moves
- CMakeLists.txt link target updates
- Missing test source files in build

**Then API changes:**
- Function renamed (e.g., `cycle_scale_mode` → `SDLAppScale_CycleMode`)
- Parameters added/removed
- Return type changed
- Struct field moved to different header

**Finally, logic issues:**
- `assert_int_equal` / `assert_string_equal` mismatches
- Off-by-one in boundary conditions
- State initialization order changes

### 5. Final Verification

After all groups fixed:

```powershell
.\recompile.bat
.\lint.bat
cd build_tests && ctest --output-on-failure
```

All three must pass. No regressions.

## Common 3SX Test Fix Patterns

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `undefined reference to 'foo'` | Function moved to new `.c` file | Add source to `CMakeLists.txt` test target |
| `fatal error: 'header.h' not found` | Header moved/renamed | Update `#include` path |
| `expected 0 was 1` | Logic changed during extraction | Check if static state initialization differs |
| `SIGSEGV` in test | Null pointer from uninitialized state | Add setup function via `cmocka_unit_test_setup` |

## Best Practices

- Fix one group at a time
- Run focused tests after each fix: `ctest -R "pattern"`
- Use `git diff` to understand recent changes
- Don't move to next group until current passes
- Keep changes minimal and focused
- Never "fix" a test by weakening the assertion
