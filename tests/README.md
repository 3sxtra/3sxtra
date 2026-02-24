# Unit Tests

This directory contains unit tests for the 3SX engine, utilizing the [CMocka](https://cmocka.org/) framework.

## Directory Structure

- `unit/`: Contains unit test source files (`test_*.c`).
- `CMakeLists.txt`: Main test configuration.

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

