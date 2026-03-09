# Unit Tests

This directory contains unit tests for the 3SX engine, utilizing the [CMocka](https://cmocka.org/) framework.

## Directory Structure

- `unit/`: Contains unit test source files (`test_*.c`).
- `CMakeLists.txt`: Main test configuration.

## Test Files

| Test file | Source under test | What it covers |
|-----------|-------------------|----------------|
| `test_smoke.c` | — | Basic sanity / framework self-test |
| `test_memman.c` | `memman.c` | Memory manager alloc/free |
| `test_renderer_interface.c` | `renderer.c` | Renderer API compile-time interface |
| `test_font_rendering.c` | `font_rendering.c` | Font/glyph rendering utilities |
| `test_game_state.c` | `netplay/game_state.c` | Save/load round-trip, NULL safety |
| `test_game_state_roundtrip.c` | `netplay/game_state.c` | Extended round-trip scenarios |
| `test_stun.c` | `stun.c` | STUN endpoint decode, socket recv edge cases |
| `test_netplay_*.c` | `netplay/*.c` | Netplay subsystem (metrics, events, OOB, init, catchup, run, refactor, UI) |
| `test_paths.c` | `config/paths.c` | Path helpers, portable-marker detection |
| `test_config.c` | `config/config.c` | INI config load/save |
| `test_lobby_server.c` | `lobby_server.c` | Lobby server presence updates |
| `test_bezel_assets.c` | `rendering/sdl_bezel.c` | Bezel asset loading, shutdown, character selection |
| `test_bezel_layout.c` | `rendering/sdl_bezel.c` | Bezel layout calculations |
| `test_menu_bridge.c` | `menu_bridge.c` | MenuBridge gate step / no-gate edge case |
| `test_globals_access.c` | — | Global variable read/write sanity |
| `test_broadcast_win32.c` | `broadcast_win32.c` | Windows broadcast socket |
| `test_broadcast_config.c` | `broadcast_config.c` | Broadcast configuration |
| `test_cli.c` | `config/cli_parser.c` | CLI argument parsing |
| `test_native_save.c` | `save/native_save.c` | Native save-file I/O |
| `test_trials.c` | `trials.c` | Trials mode logic |
| `test_radix_sort.c` | *(inline)* | Radix sort algorithm |
| `test_charset_poc.c` | `charset.c` | Character-set proof of concept |

| `test_state_differ.c` | `state_differ.c` | State diff / desync detection |
| `test_effect_state_persistence.c` | effect state | Effect state save/restore |
| `test_legacy_matrix.c` | `port/rendering/legacy_matrix.c` | Matrix identity, scale, translate, calcPoint, get/set round-trip |
| `test_adx_decoder.c` | `port/sound/adx_decoder.c` | ADX ADPCM header init validation, synthetic decode |
| `test_stage_config.c` | `port/mods/stage_config.c` | INI load/save, defaults, boundary, round-trip |
| `test_afs_validation.c` | `port/io/afs.c` (validation logic) | AFS attribute bounds checking (pure logic, no I/O) |
| `test_char_data.c` | `port/char_data.c` | CharData_ApplyFixups: Akuma fixup, non-Akuma unchanged, NULL safety |



## Running Tests

Tests are integrated into the CMake build system.

### From Command Line

1.  Configure the project (ensure `ENABLE_TESTS` is ON, which is default):
    ```bash
    cmake -B build
    ```

2.  Build the tests:
    ```bash
    cmake --build build
    ```

3.  Run the tests using CTest:
    ```bash
    cd build
    ctest --output-on-failure
    ```

    Or run specific test executables directly:
    ```bash
    ./build/tests/unit/test_memman.exe
    ```

## Adding New Tests

1.  Create a new test file in `tests/unit/` (e.g., `test_myfeature.c`).
2.  Include `<cmocka.h>` and the headers for the code you are testing.
3.  Write test functions matching the signature `void test_func(void **state)`.
4.  Register the test suite in `main()`.
5.  Add the test target to `tests/unit/CMakeLists.txt` using the helper:
    ```cmake
    add_unit_test(test_myfeature 
        test_myfeature.c
        ${PROJECT_SOURCE_DIR}/src/path/to/source.c
    )
    ```

## Netplay Desync Debugging

The `tools/compare_states.py` utility helps investigate netplay desyncs by comparing
state dumps and providing symbolic paths to differing bytes.

### Prerequisites

- **Python 3.10+**
- **dwarfdump**: Install via MSYS2: `pacman -S mingw-w64-x86_64-dwarfutils`

### Capturing State Dumps

1. Build in DEBUG mode:
   ```bash
   cmake -B build -DCMAKE_BUILD_TYPE=Debug
   cmake --build build
   ```

2. Run a netplay session. When a desync is detected, state dumps are written to `./states/`.

### Analyzing Desyncs

Run the comparison utility:
```bash
# Windows (from repository root)
tools\compare_states.bat build\3sx.exe

# Or directly via Python in MSYS2
python tools/compare_states.py build/3sx.exe
```

The output shows symbolic paths for each byte mismatch:
```
1234: mismatch at byte 0x1A8 (0x05 vs 0x06). Path: gs.gs_Random_ix16
```

Use `--info` to inspect parsed struct layout:
```bash
python tools/compare_states.py build/3sx.exe --info
```

