# Build Guide

---

## 1. Prerequisites

All platforms need:
- **CMake** 3.24+
- **Clang** (C11 + C++17) — GCC works on Linux but Clang is recommended
- **Ninja** (recommended) or Make
- **Rust/Cargo** — required for librashader (shader support)
- **Python 3 + jinja2** — required for GLAD code generation
- **curl** — required to download stb headers

### Windows (MSYS2)

**Automated (recommended):**
Double-click `tools\1click_windows_v2.bat`. It downloads a portable MSYS2, installs everything, builds deps, and compiles the executable.

**Manual:**
1. Launch the **MinGW64** shell.
2. Install packages:
   ```bash
   pacman -S --needed $(cat tools/requirements-windows.txt)
   ```

The Windows requirements file includes: cmake, ninja, clang, zlib, rust, python-jinja, miniupnpc, and compiler headers.

### Linux (Ubuntu / Debian)

```bash
sudo apt-get update
sudo apt-get install -y $(cat tools/requirements-ubuntu.txt)
```

You also need Rust if not already installed:
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
```

### macOS

1. Install Xcode Command Line Tools:
   ```bash
   xcode-select --install
   ```
2. Install required Homebrew packages:
   ```bash
   brew install miniupnpc zlib
   python3 -m pip install --break-system-packages jinja2
   ```
3. Install Rust:
   ```bash
   curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
   ```

---

## 2. Building Dependencies

`build-deps.sh` clones and builds all third-party libraries:
SDL3, SDL3_mixer, SDL3_image, FreeType, Lua 5.4, RmlUi, GekkoNet, librashader, GLAD, SIMDe, stb, Spout2, SDL_shadercross, and slang-shaders.

```bash
./build-deps.sh
```

On Windows this is handled automatically by `1click_windows_v2.bat` and `compile.bat`.

---

## 3. Compiling the Game

### Linux / macOS

```bash
CC=clang CXX=clang++ cmake -B build -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DENABLE_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix build/application
```

> [!NOTE]
> On macOS you may need to add `-DZLIB_ROOT=$(brew --prefix zlib)` to the cmake configure line.

### Windows

From a MinGW64 shell, run:
```
.\compile.bat
```

This configures with `RelWithDebInfo` by default (includes Tracy profiling). Pass `--debug` for a Debug build.

Or manually:
```bash
CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DENABLE_TESTS=OFF
cmake --build build --parallel
cmake --install build --prefix build/application
```

### Unit Tests

```bash
cmake -B build_tests -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TESTS=ON
cmake --build build_tests --parallel
cd build_tests && ctest --output-on-failure
```

---

## 4. Cross-Compilation & Packaging

### Raspberry Pi 4 (Batocera)
Cross-compile for RPi4 using the Batocera Linux buildroot toolchain.
- Setup scripts: `tools/batocera/rpi4/`
- See `build_rpi4.yml` workflow for Docker container setup, `build-deps_rpi4.sh`, and ARM64 compilation steps.

### Flatpak
- Manifest and metadata: `flatpak/`
- Build locally with `flatpak-builder`. See `build_flatpak.yml` workflow for reference.
