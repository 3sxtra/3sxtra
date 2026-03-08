# PRD: Test Scaffolding — Systematic Unit Test Coverage

## Overview
Add unit tests for untested modules in the 3SX engine.
All tests use the **CMocka** framework and follow the existing conventions in `tests/unit/`.
The project already has ~33 unit tests covering netplay, game state, config, CLI, paths, bezels,
broadcasting, renderer interface, radix sort, menu bridge, trials, and font rendering.
This PRD targets the remaining high-value gaps.

## Goals
- Cover every testable public API that currently lacks unit tests
- Keep each test file self-contained (no hardware, no SDL window, no GPU)
- Register every new test in `tests/unit/CMakeLists.txt` so `ctest` picks it up
- Maintain the existing conventions: CMocka, `add_unit_test()` helper, organized link groups

## Conventions (DO NOT deviate)
- Test files live in `tests/unit/test_<name>.c`
- Source under test is compiled into the test target directly (see existing CMake patterns)
- Include dirs: use `target_include_directories` for `${PROJECT_SOURCE_DIR}/include`, `${SDL3_ROOT}/include`, etc.
- Link helpers already defined: `target_link_sdl3()`, `target_link_sdl3_glad()`, `target_link_gekkonet_sdl3()`
- Mock files: `mocks_*.c` in `tests/unit/` — create new mocks only if strictly necessary
- Existing test examples to follow: `test_radix_sort.c` (header-only algo), `test_config.c` (with mocks)

---

## Task 1: Matrix math — `legacy_matrix.c`
Create `tests/unit/test_legacy_matrix.c`.
Source under test: `src/port/rendering/legacy_matrix.c`.
Header: `src/include/port/rendering/legacy_matrix.h`.
Dependencies: `common.h` → `types.h`, `structs.h` (type definitions only, no runtime deps).

Test the following functions:
- `njUnitMatrix` — identity on explicit matrix and on NULL (global cmtx)
- `njScale` — known scale factors, verify matrix elements
- `njTranslate` — translate by (x,y,z), verify row 3
- `njTranslateZ` — fast-path equivalent to njTranslate(NULL, 0, 0, z)
- `njCalcPoint` — transform a point, compare to hand-computed result
- `njCalcPoints` — array variant
- `njGetMatrix` / `njSetMatrix` — round-trip copy

Add to CMakeLists.txt:
```cmake
add_unit_test(test_legacy_matrix
    test_legacy_matrix.c
    ${PROJECT_SOURCE_DIR}/src/port/rendering/legacy_matrix.c
)
target_include_directories(test_legacy_matrix PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

Acceptance criteria:
- All test cases pass via `ctest -R test_legacy_matrix`
- At least 8 test functions covering identity, scale, translate, translateZ, calcPoint
- Edge cases: NULL matrix pointer (uses global cmtx), zero scale, negative translate

---

## Task 2: ADX decoder — `adx_decoder.c`
Create `tests/unit/test_adx_decoder.c`.
Source under test: `src/port/sound/adx_decoder.c`.
Header: `src/port/sound/adx_decoder.h`.
Dependencies: `types.h` only (no SDL, no file I/O).

Test the following functions:
- `ADX_InitContext` — valid mono header, valid stereo header, invalid magic, too-small header, bad channels
- `ADX_Decode` — NULL args return -1, zero frame_size return -1
- Construct a minimal synthetic ADX frame (18-byte block with known scale + nibbles), decode it, verify output samples match hand-computed ADPCM

Add to CMakeLists.txt:
```cmake
add_unit_test(test_adx_decoder
    test_adx_decoder.c
    ${PROJECT_SOURCE_DIR}/src/port/sound/adx_decoder.c
)
target_include_directories(test_adx_decoder PRIVATE ${PROJECT_SOURCE_DIR}/src)
```

Acceptance criteria:
- All test cases pass via `ctest -R test_adx_decoder`
- At least 6 test functions covering init validation and basic decode
- Edge cases: 0-channel, oversized channel count, minimum header size

---

## Task 3: Stage config parser — `stage_config.c`
Create `tests/unit/test_stage_config.c`.
Source under test: `src/port/mods/stage_config.c`.
Header: `src/port/mods/stage_config.h`.
Dependencies: `paths.h` (`Paths_GetBasePath()`) — mock it. Also needs `bg_data.h` externs (`use_real_scr`, `stage_bgw_number`) — provide small stubs.

Test the following functions:
- `StageConfig_Init` — all layers get expected defaults
- `StageConfig_SetDefaultLayer` — boundary: -1, 0, MAX_STAGE_LAYERS-1, MAX_STAGE_LAYERS
- `StageConfig_Load` — write a temp INI, load it, verify fields. Test missing file (defaults preserved).
- `StageConfig_Save` + `StageConfig_Load` round-trip — save then reload, compare fields

Create `tests/unit/mocks_stage_config.c` with stub for `Paths_GetBasePath()` (return temp dir) and minimal `use_real_scr[]` / `stage_bgw_number[][]` arrays.

Add to CMakeLists.txt:
```cmake
add_unit_test(test_stage_config
    test_stage_config.c
    mocks_stage_config.c
    ${PROJECT_SOURCE_DIR}/src/port/mods/stage_config.c
)
target_include_directories(test_stage_config PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src)
```

Acceptance criteria:
- All test cases pass via `ctest -R test_stage_config`
- At least 6 test functions covering init, defaults, load, save, round-trip
- Edge cases: out-of-range layer index, missing INI file, malformed INI line

---

## Task 4: AFS archive validation — `afs.c` (header parsing only)
Create `tests/unit/test_afs_validation.c`.
Source under test: Only the `is_valid_attribute_data()` function from `src/port/io/afs.c`.
Since `is_valid_attribute_data` is `static`, either:
  (a) Copy it into the test file for isolated testing, or
  (b) Use `#include "../../src/port/io/afs.c"` pattern (not ideal but acceptable for static functions).

Option (a) is preferred. Copy the function signature and body; it is pure logic with no dependencies.

Test cases:
- Zero offset or size → false
- Size exceeds file bounds → false
- Size less than entries * entry_size → false
- Offset before entries_end → false
- Offset after file_size - size → false
- Valid parameters → true

Add to CMakeLists.txt:
```cmake
add_unit_test(test_afs_validation test_afs_validation.c)
target_include_directories(test_afs_validation PRIVATE ${PROJECT_SOURCE_DIR}/src)
```

Acceptance criteria:
- All test cases pass via `ctest -R test_afs_validation`
- At least 6 test functions covering each validation branch
- No SDL dependency required (pure integer logic)

---

## Task 5: Build and run all new tests
Build the project with tests enabled and run the full test suite.

Steps:
1. `cmake -B build` (or reconfigure existing build)
2. `cmake --build build`
3. `cd build && ctest --output-on-failure`

Acceptance criteria:
- All existing tests still pass (no regressions)
- All 4 new test targets pass
- No compiler warnings in test files

---

## Task 6: Clean up and document
Update `tests/README.md` to mention the new test files.
Ensure all new test files have proper file-level doc comments following the existing pattern.

Acceptance criteria:
- README lists the new test files
- Each new file has a `@file` / `@brief` doc comment
- No orphan files or dead code
