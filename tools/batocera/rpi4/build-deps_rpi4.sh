#!/usr/bin/env bash
# build-deps_rpi4.sh - Cross-compile dependencies for RPi4 (aarch64)
# This script is called from within the Batocera Docker build environment.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
THIRD_PARTY="$ROOT_DIR/third_party_rpi4"

# Batocera toolchain paths
# These are set by the Batocera build system, but can be overridden.
# Auto-detect: try Batocera Docker paths first, fall back to Ubuntu packages.
TOOLCHAIN_DIR="${TOOLCHAIN_DIR:-/bcm2711/host}"
CROSS_PREFIX="${CROSS_PREFIX:-}"

if [ -z "$CROSS_PREFIX" ]; then
    if [ -x "${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-gcc" ]; then
        CROSS_PREFIX="${TOOLCHAIN_DIR}/bin/aarch64-buildroot-linux-gnu-"
        SYSROOT="${SYSROOT:-${TOOLCHAIN_DIR}/aarch64-buildroot-linux-gnu/sysroot}"
    elif command -v aarch64-linux-gnu-gcc &>/dev/null; then
        CROSS_PREFIX="aarch64-linux-gnu-"
        SYSROOT="${SYSROOT:-/usr/aarch64-linux-gnu}"
        echo "Using Ubuntu cross-compiler (aarch64-linux-gnu-*)"
    else
        echo "ERROR: No aarch64 cross-compiler found."
        echo "Install with: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
        echo "Or set TOOLCHAIN_DIR / CROSS_PREFIX manually."
        exit 1
    fi
else
    SYSROOT="${SYSROOT:-${TOOLCHAIN_DIR}/aarch64-buildroot-linux-gnu/sysroot}"
fi

CC="${CROSS_PREFIX}gcc"
CXX="${CROSS_PREFIX}g++"

echo "Cross-compiler: $CC"
echo "Sysroot: $SYSROOT"

# Ubuntu cross packages use absolute paths in their libc.so linker scripts.
# Passing --sysroot causes the linker to double-nest those absolute paths.
# Ubuntu sysroots live under /usr/; Batocera uses /bcm2711/host/... paths.
SYSROOT_FLAGS=()
if [[ "$SYSROOT" == /usr/* ]]; then
    echo "Skipping CMAKE_SYSROOT (Ubuntu cross-compiler — linker scripts use absolute paths)"
else
    SYSROOT_FLAGS=(-DCMAKE_SYSROOT="$SYSROOT")
    echo "Using sysroot: $SYSROOT"
fi

# -----------------------------
# SDL3
# -----------------------------

SDL_DIR="$THIRD_PARTY/sdl3"
SDL_SRC="$SDL_DIR/SDL"
SDL_BUILD="$SDL_DIR/build"

if [ -d "$SDL_BUILD" ]; then
    echo "SDL3 already built at $SDL_BUILD"
else
    echo "Cross-compiling SDL3 for aarch64..."
    cd "$SDL_SRC"
    mkdir -p build_temp && cd build_temp

    cmake .. \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        "${SYSROOT_FLAGS[@]}" \
        -DCMAKE_INSTALL_PREFIX="$SDL_BUILD" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DBUILD_SHARED_LIBS=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3 cross-compiled to $SDL_BUILD"

    cd ..
    rm -rf build_temp
fi

# -----------------------------
# SDL3_image
# -----------------------------

SDL_IMAGE_DIR="$THIRD_PARTY/sdl3_image"
SDL_IMAGE_BUILD="$SDL_IMAGE_DIR/build"

if [ -d "$SDL_IMAGE_BUILD" ]; then
    echo "SDL3_image already built at $SDL_IMAGE_BUILD"
else
    echo "Cross-compiling SDL3_image for aarch64..."

    # Find the extracted source directory
    SDL_IMAGE_SRC=$(find "$SDL_IMAGE_DIR" -maxdepth 1 -type d -name "SDL_image-*" | head -n 1)

    if [ -z "$SDL_IMAGE_SRC" ]; then
        echo "ERROR: SDL_image source not found. Run download-deps_rpi4.sh first."
        exit 1
    fi

    cd "$SDL_IMAGE_SRC"
    mkdir -p build && cd build

    cmake .. \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        "${SYSROOT_FLAGS[@]}" \
        -DCMAKE_INSTALL_PREFIX="$SDL_IMAGE_BUILD" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
        -DBUILD_SHARED_LIBS=ON

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3_image cross-compiled to $SDL_IMAGE_BUILD"

    cd ../..
fi

# -----------------------------
# SDL3_mixer
# -----------------------------

SDL_MIXER_DIR="$THIRD_PARTY/sdl3_mixer"
SDL_MIXER_BUILD="$SDL_MIXER_DIR/build"

if [ -d "$SDL_MIXER_BUILD" ]; then
    echo "SDL3_mixer already built at $SDL_MIXER_BUILD"
else
    echo "Cross-compiling SDL3_mixer for aarch64..."

    SDL_MIXER_SRC="$SDL_MIXER_DIR/SDL_mixer"

    if [ ! -d "$SDL_MIXER_SRC" ]; then
        echo "ERROR: SDL_mixer source not found. Run download-deps_rpi4.sh first."
        exit 1
    fi

    # Clean any stale in-source build cache from a previous failed attempt
    rm -rf "$SDL_MIXER_SRC/build"

    cd "$SDL_MIXER_SRC"
    mkdir -p build && cd build

    # Ubuntu multiarch fix: when CMAKE_SYSROOT is skipped (linker script issue),
    # the compiler may pick up x86_64 host headers via CMake-added include paths.
    # Explicitly prioritise the aarch64 system includes.
    CROSS_C_FLAGS=""
    if [ ${#SYSROOT_FLAGS[@]} -eq 0 ] && [ -d "/usr/aarch64-linux-gnu/include" ]; then
        CROSS_C_FLAGS="-isystem /usr/aarch64-linux-gnu/include"
        echo "  (Ubuntu multiarch: adding aarch64 isystem include)"
    fi

    cmake .. \
        -DCMAKE_C_COMPILER="$CC" \
        -DCMAKE_CXX_COMPILER="$CXX" \
        "${SYSROOT_FLAGS[@]}" \
        ${CROSS_C_FLAGS:+-DCMAKE_C_FLAGS="$CROSS_C_FLAGS"} \
        -DCMAKE_INSTALL_PREFIX="$SDL_MIXER_BUILD" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
        -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
        -DBUILD_SHARED_LIBS=ON \
        -DSDLMIXER_VENDORED=ON

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3_mixer cross-compiled to $SDL_MIXER_BUILD"

    cd ../..
fi

# -----------------------------
# librashader (Rust cross-compile)
# -----------------------------

LIBRASHADER_DIR="$THIRD_PARTY/librashader"
LIBRASHADER_TARGET="aarch64-unknown-linux-gnu"
LIBRASHADER_LIB="$LIBRASHADER_DIR/target/$LIBRASHADER_TARGET/release/liblibrashader_capi.a"

if [ -f "$LIBRASHADER_LIB" ]; then
    echo "librashader already cross-compiled."
else
    # Patch: Fix Vulkan LUT alignment panic (gpu-allocator may return a larger mapped region)
    LUTS_RS="$LIBRASHADER_DIR/librashader-runtime-vk/src/luts.rs"
    if grep -q 'staging.as_mut_slice()?.copy_from_slice' "$LUTS_RS" 2>/dev/null; then
        echo "Patching librashader luts.rs (Vulkan buffer alignment fix)..."
        sed -i 's|staging.as_mut_slice()?.copy_from_slice(\&image.bytes);|let staging_slice = staging.as_mut_slice()?;\n        staging_slice[..image.bytes.len()].copy_from_slice(\&image.bytes);|' "$LUTS_RS"
    fi

    echo "Cross-compiling librashader for $LIBRASHADER_TARGET..."

    # Ensure Rust target is installed
    rustup target add "$LIBRASHADER_TARGET"

    # Configure Rust linker for aarch64
    mkdir -p "$LIBRASHADER_DIR/.cargo"
    cat > "$LIBRASHADER_DIR/.cargo/config.toml" << EOF
[target.aarch64-unknown-linux-gnu]
linker = "${CROSS_PREFIX}gcc"
EOF

    cd "$LIBRASHADER_DIR"
    cargo build \
        --target "$LIBRASHADER_TARGET" \
        --release \
        -p librashader-capi \
        --no-default-features \
        --features runtime-opengl,runtime-vulkan,stable

    echo "librashader cross-compiled to $LIBRASHADER_LIB"
fi

echo "All RPi4 dependencies built successfully."
