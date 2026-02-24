# Build Guide

This fork introduces numerous features, performance optimizations, and platform targets, which require a specific toolchain and set of dependencies.

---

## 1. Prerequisites & Setup

### Windows

The easiest way to build on Windows is using MSYS2 and the MinGW64 toolchain. This fork provides a 1-click script to automate the entire process.

**Automated Setup:**
Simply double-click `tools\1click_windows_v2.bat`.
This script will:
1. Download a portable MSYS2 environment.
2. Install all required packages (CMake, Ninja, Clang, Compiler Headers).
3. Build the third-party dependencies.
4. Compile the `3sx` executable.

**Manual Setup:**
If you prefer to use an existing MSYS2 installation:
1. Launch the **MinGW64** shell.
2. Install the required packages:
   ```bash
   pacman -S make mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja mingw-w64-x86_64-clang mingw-w64-x86_64-headers-git
   ```

### Linux (Ubuntu / Debian)

You will need the standard build tools, Clang, plus the dependencies required to build SDL3 and the new camera/audio backends like PipeWire.

```bash
sudo apt-get update
# Install SDL3 dependencies (including PipeWire, Wayland, X11, ALSA, PulseAudio, etc.)
sudo apt-get install -y $(cat tools/requirements-ubuntu-sdl3.txt)
# Install compiler and build tools
sudo apt-get install -y clang curl cmake ninja-build
```

### macOS

You should be able to build the project using the Xcode Command Line Tools.

1. Check if Command Line Tools are installed:
   ```bash
   xcode-select -p
   ```
2. Install them if needed:
   ```bash
   xcode-select --install
   ```

---

## 2. Building Dependencies

Before building the game, you must compile the third-party libraries (SDL3, SDL3_image, librashader, Spout2, etc.).

Run the dependency build script from the repository root:
```bash
./build-deps.sh
```
*(On Windows, this is handled automatically if you use `1click_windows_v2.bat`)*

---

## 3. Compiling the Game

Once dependencies are built, you can compile the game using CMake.

### Standard Build (Release)
We highly recommend building in `Release` mode. This enables Link-Time Optimization (LTO) and Profile-Guided Optimization (PGO) which are critical for the performance optimizations in this fork.

```bash
# Configure the build directory
CC=clang CXX=clang++ cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile the project
cmake --build build --parallel --config Release

# Install to the output directory (build/application)
cmake --install build --prefix build/application
```

### Build Scripts
For convenience, this repository includes helper scripts:
- **Windows**: Run `compile.bat` from a MinGW64 shell to configure, build, and deploy the application automatically.
- **Unit Tests**: Run `compile_tests.bat` to build and execute the CMocka test suite.

---

## 4. Advanced: Cross-Compilation & Packaging

This fork adds support for specific Linux environments and packaging formats. The logic for these is contained within GitHub Actions workflows (e.g., `.github/workflows/`), but they can be run locally.

### Raspberry Pi 4 (Batocera)
We support cross-compiling for the Raspberry Pi 4 using the Batocera Linux buildroot toolchain.
- The setup scripts are located in `tools/batocera/rpi4/`.
- See the `build_rpi4.yml` workflow for the exact steps to configure the Docker container, run `build-deps_rpi4.sh`, and compile the ARM64 binaries.

### Flatpak
Linux desktop packaging is available via Flatpak.
- The manifest and metadata files are located in `flatpak/`.
- You can build the Flatpak bundle locally using `flatpak-builder`. See the `build_flatpak.yml` workflow for reference.
