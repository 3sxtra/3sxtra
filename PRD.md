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

## Task 5: Edge cases in existing test files
Several existing tests are missing edge case and error path coverage. Add test functions to the **existing** test files:

### `test_stun.c` — add:
- `test_decode_endpoint_null_args` — NULL code, NULL out_ip, NULL out_port → returns false
- `test_decode_endpoint_empty_string` — empty string → returns false
- `test_decode_endpoint_malformed` — truncated or garbage input → returns false
- `test_socket_recv_from_bad_fd` — invalid fd (-1) → returns error
- `test_socket_recv_from_zero_buf` — buf_size = 0 → returns 0 or error

### `test_paths.c` — add:
- `test_is_portable_with_marker_file` — create a temp marker file, verify returns 1
- `test_is_portable_without_marker` — no marker file, verify returns 0

### `test_bezel_assets.c` or `test_bezel_layout.c` — add:
- `test_bezel_shutdown_null_safe` — call `BezelSystem_Shutdown()` when not initialized → no crash
- `test_bezel_set_characters_valid` — call `BezelSystem_SetCharacters(0, 1)` with valid indices
- `test_bezel_set_characters_out_of_range` — call with invalid character indices → handled gracefully

### `test_netplay_run.c` or `test_netplay_refactor.c` — add:
- `test_handle_menu_exit_not_connected` — call `Netplay_HandleMenuExit()` when netplay is not active → no crash

### `test_menu_bridge.c` — add:
- `test_step_gate_no_active_gate` — call `MenuBridge_StepGate()` when no gate is set → no crash, returns expected default

### `test_lobby_server.c` — add:
- `test_update_presence_not_connected` — call `LobbyServer_UpdatePresence()` when not connected → graceful no-op

### `test_stun.c` — also add:
- `test_parse_binding_response_truncated` — truncated STUN response → returns error
- `test_parse_binding_response_wrong_type` — valid STUN header but wrong message type → returns error

Acceptance criteria:
- All new test functions compile and pass
- No regressions in the existing tests in those files
- Each edge case tests a distinct boundary or error condition

---

## Task 6: CharData_ApplyFixups — `char_data.c`
Create `tests/unit/test_char_data.c`.
Source under test: `src/port/char_data.c`.
Header: `src/port/char_data.h` (depends on `structs.h` for `CharInitData`).

Test cases:
- `test_fixups_akuma` — character_id 14 (Akuma), verify `hiit[0x5A..0x5D].cuix` are zeroed
- `test_fixups_other_chars` — character_id != 14, verify data is unchanged
- `test_fixups_null_data` — NULL data pointer → no crash (if the function handles it; if not, document)

Add to CMakeLists.txt:
```cmake
add_unit_test(test_char_data
    test_char_data.c
    ${PROJECT_SOURCE_DIR}/src/port/char_data.c
)
target_include_directories(test_char_data PRIVATE ${PROJECT_SOURCE_DIR}/include)
```

Acceptance criteria:
- All test cases pass via `ctest -R test_char_data`
- Akuma fixup behavior is verified against actual hitbox indices

---

## Task 7: Verify all existing tests still pass
Before adding new tests, run the full existing suite to establish a green baseline.

Steps:
1. `CC=clang cmake -B build_tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON`
2. `cmake --build build_tests`
3. `cd build_tests && ctest --output-on-failure`

Acceptance criteria:
- All pre-existing tests pass
- Build has no errors (warnings are acceptable)

---

## Task 8: Build and run all new tests
Rebuild with all new test files and run the complete suite.

Steps:
1. `CC=clang cmake -B build_tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON`
2. `cmake --build build_tests`
3. `cd build_tests && ctest --output-on-failure`

Acceptance criteria:
- All existing tests still pass (no regressions)
- All new test targets pass
- No compiler warnings in test files

---

## Task 9: Clean up and document
Update `tests/README.md` to mention the new test files.
Ensure all new test files have proper file-level doc comments following the existing pattern.

Acceptance criteria:
- README lists the new test files
- Each new file has a `@file` / `@brief` doc comment
- No orphan files or dead code
