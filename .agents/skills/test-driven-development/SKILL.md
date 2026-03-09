---
name: test-driven-development
description: "Red-Green-Refactor for C with CMocka. Write failing test first, make it pass, clean up. Adapted for the 3SX unit test framework."
---

# Test-Driven Development (TDD)

> Adapted from [antigravity-awesome-skills](https://github.com/sickn33/antigravity-awesome-skills/tree/main/skills/test-driven-development) for the 3SX C game engine using CMocka.

## The Iron Law

```
NO PRODUCTION CODE WITHOUT A FAILING TEST FIRST
```

## When to Use

- Adding new utility functions or modules
- Fixing bugs (write test that reproduces the bug first)
- Refactoring existing code (tests prove no regression)
- Implementing game logic helpers (geometry, scaling, config parsing)

## Red-Green-Refactor

### RED — Write Failing Test

```c
// tests/test_scale_mode.c
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
#include "sdl_app_scale.h"

static void test_cycle_scale_mode_wraps(void **state) {
    (void)state;
    ScaleMode mode = SCALE_COUNT - 1;  // last valid mode
    ScaleMode next = cycle_scale_mode(mode);
    assert_int_equal(next, SCALE_FIT);  // should wrap to first
}
```

### Verify RED — Watch It Fail

```powershell
cd build_tests && ctest --output-on-failure -R test_scale_mode
```

Confirm the test **fails** for the right reason (missing function, wrong result — NOT a compile error or crash).

### GREEN — Minimal Code

Write the **minimum** production code to make the test pass:

```c
// src/port/sdl/app/sdl_app_scale.c
ScaleMode cycle_scale_mode(ScaleMode current) {
    return (ScaleMode)((current + 1) % SCALE_COUNT);
}
```

### Verify GREEN — Watch It Pass

```powershell
.\recompile.bat
cd build_tests && ctest --output-on-failure -R test_scale_mode
```

### REFACTOR — Clean Up

- Remove duplication
- Improve naming
- Add `const` correctness
- Keep tests passing throughout

### Repeat

Move to next test case. One test at a time.

## Good Tests

| Property | Meaning |
|----------|---------|
| **Fast** | No file I/O, no network, no sleep |
| **Isolated** | No shared mutable state between tests |
| **Deterministic** | Same result every run |
| **Descriptive** | Test name says what and why |

## CMocka Test File Template

```c
/**
 * @file test_module_name.c
 * @brief Unit tests for module_name
 */
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>

#include "module_header.h"

static void test_function_does_expected_thing(void **state) {
    (void)state;
    // Arrange
    int input = 42;
    // Act
    int result = function_under_test(input);
    // Assert
    assert_int_equal(result, expected_value);
}

int main(void) {
    const struct CMUnitTest tests[] = {
        cmocka_unit_test(test_function_does_expected_thing),
    };
    return cmocka_run_group_tests(tests, NULL, NULL);
}
```

Don't forget to add the test to `CMakeLists.txt`:
```cmake
add_test(NAME test_module_name COMMAND test_module_name)
```

## Verification Checklist

After each cycle:
- [ ] Test failed for the right reason (RED)
- [ ] Minimal code makes it pass (GREEN)
- [ ] `.\recompile.bat` succeeds
- [ ] `.\lint.bat` passes
- [ ] `ctest --output-on-failure` passes (all tests, not just new one)

## Testing Anti-Patterns

| ❌ Don't | ✅ Do |
|----------|-------|
| Write test after code | Write test before code |
| Test private implementation details | Test public API behavior |
| Share state between tests | Each test is independent |
| Skip the RED step | Always see the test fail first |
| Write multiple tests at once | One test at a time |
| Test trivial getters/setters | Test behavior and edge cases |

## When Stuck

1. Can't write a test? — The interface may need redesign
2. Test too complex? — Function under test does too much, split it
3. Too many mocks? — Too much coupling, simplify dependencies
4. Flaky test? — Hidden shared state or timing dependency
