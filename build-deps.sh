#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
THIRD_PARTY="${THIRD_PARTY:-$ROOT_DIR/third_party}"

mkdir -p "$THIRD_PARTY"

# Detect OS
OS="$(uname -s)"
echo "Detected OS: $OS"

# Optional: set to "universal" for macOS fat binaries (arm64+x86_64)
TARGET_ARCH="${TARGET_ARCH:-}"

echo "Using cmake from: $(which cmake)"
cmake --version

# glad code generation needs jinja2 — ensure it's available for whichever Python cmake finds
python3 -m pip install --quiet --break-system-packages jinja2 2>/dev/null \
    || python3 -m pip install --quiet jinja2 2>/dev/null \
    || true

# Note: Rust/cargo is only needed for librashader. Check is deferred to that section
# so all other deps (SDL3, glad, SDL_shadercross, etc.) still build without Rust.

# -----------------------------
# SDL3
# -----------------------------

SDL_DIR="$THIRD_PARTY/sdl3"
SDL_SRC="$SDL_DIR/SDL"
SDL_BUILD="$SDL_DIR/build"

if [ "${SKIP_SDL3_BUILD:-}" = "1" ]; then
    echo "SKIP_SDL3_BUILD=1 — skipping SDL3 (pre-installed)"
elif [ -d "$SDL_BUILD" ]; then
    echo "SDL3 already built at $SDL_BUILD"
else
    echo "Building SDL3..."
    mkdir -p "$SDL_DIR"
    cd "$SDL_DIR"

    if [ ! -d "$SDL_SRC" ]; then
        echo "Cloning SDL3 from git..."
        git clone --depth 1 https://github.com/libsdl-org/SDL.git "$SDL_SRC"
    fi

    cd "$SDL_SRC"

    mkdir -p build_temp
    cd build_temp

    case "$OS" in
        Darwin|Linux)
            CMAKE_EXTRA_ARGS=""
            if [ "$OS" = "Darwin" ] && [ "$TARGET_ARCH" = "universal" ]; then
                CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
            fi
            cmake .. \
                ${CC:+-DCMAKE_C_COMPILER=$CC} \
                ${CXX:+-DCMAKE_CXX_COMPILER=$CXX} \
                -DCMAKE_INSTALL_PREFIX="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDL_STATIC=OFF \
                -DSDL_TESTS=OFF \
                $CMAKE_EXTRA_ARGS
            ;;
        MINGW*|MSYS*|CYGWIN*)
            # Disable OpenGL ES to avoid missing EGL headers on MSYS2
            cmake .. \
                -G "MSYS Makefiles" \
                -DCMAKE_C_COMPILER=gcc \
                -DCMAKE_INSTALL_PREFIX="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDL_TESTS=OFF \
                -DSDL_OPENGLES=OFF
            ;;
    esac

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3 installed to $SDL_BUILD"

    cd ..
    rm -rf build_temp
fi



# -----------------------------
# simde
# -----------------------------

SIMDE_DIR="$THIRD_PARTY/simde"

if [ -d "$SIMDE_DIR" ]; then
    echo "simde already exists at $SIMDE_DIR"
else
    echo "Cloning simde..."
    git clone https://github.com/simd-everywhere/simde.git "$SIMDE_DIR"
    echo "simde cloned to $SIMDE_DIR"
fi

# -----------------------------
# stb
# -----------------------------

STB_DIR="$THIRD_PARTY/stb"
mkdir -p "$STB_DIR"

if [ ! -f "$STB_DIR/stb_truetype.h" ]; then
    echo "Downloading stb_truetype.h..."
    curl -L -o "$STB_DIR/stb_truetype.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"
    echo "stb_truetype.h downloaded to $STB_DIR"
fi

if [ ! -f "$STB_DIR/stb_image.h" ]; then
    echo "Downloading stb_image.h..."
    curl -L -o "$STB_DIR/stb_image.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
    echo "stb_image.h downloaded to $STB_DIR"
fi

# -----------------------------
# GekkoNet (rollback networking)
# -----------------------------

GEKKONET_DIR="$THIRD_PARTY/GekkoNet"

if [ -d "$GEKKONET_DIR" ]; then
    echo "GekkoNet already exists at $GEKKONET_DIR"
else
    echo "Cloning GekkoNet..."
    git clone --depth 1 https://github.com/HeatXD/GekkoNet.git "$GEKKONET_DIR"
    echo "GekkoNet cloned to $GEKKONET_DIR"
fi

# -----------------------------
# Spout2 (Windows video broadcast)
# -----------------------------

SPOUT2_DIR="$THIRD_PARTY/Spout2"

if [ -d "$SPOUT2_DIR" ]; then
    echo "Spout2 already exists at $SPOUT2_DIR"
else
    echo "Cloning Spout2..."
    git clone --depth 1 https://github.com/leadedge/Spout2.git "$SPOUT2_DIR"
    echo "Spout2 cloned to $SPOUT2_DIR"
fi

# -----------------------------
# glad
# -----------------------------

GLAD_DIR="$THIRD_PARTY/glad"

if [ -d "$GLAD_DIR" ]; then
    echo "glad already exists at $GLAD_DIR"
else
    echo "Cloning glad..."
    git clone --branch v2.0.8 https://github.com/Dav1dde/glad.git "$GLAD_DIR"
    echo "glad cloned to $GLAD_DIR"
fi

# -----------------------------
# librashader
# -----------------------------

LIBRASHADER_DIR="$THIRD_PARTY/librashader"

if [ -d "$LIBRASHADER_DIR" ]; then
    echo "librashader already exists at $LIBRASHADER_DIR"
    # Optional: check if build exists?
else
    echo "Cloning librashader..."
    git clone https://github.com/SnowflakePowered/librashader.git "$LIBRASHADER_DIR"
    echo "librashader cloned to $LIBRASHADER_DIR"
fi

# Patch: Fix Vulkan LUT alignment panic (gpu-allocator may return a larger mapped region)
LUTS_RS="$LIBRASHADER_DIR/librashader-runtime-vk/src/luts.rs"
if grep -q 'staging.as_mut_slice()?.copy_from_slice' "$LUTS_RS" 2>/dev/null; then
    echo "Patching librashader luts.rs (Vulkan buffer alignment fix)..."
    sed 's|staging.as_mut_slice()?.copy_from_slice(\&image.bytes);|let staging_slice = staging.as_mut_slice()?;\n        staging_slice[..image.bytes.len()].copy_from_slice(\&image.bytes);|' "$LUTS_RS" > "$LUTS_RS.tmp" && mv "$LUTS_RS.tmp" "$LUTS_RS"
fi

# Build librashader-capi (requires Rust/cargo)
if ! command -v cargo &> /dev/null; then
    echo "WARNING: cargo (Rust) not found — skipping librashader build."
    echo "         librashader is required for shader support. Install Rust to enable it."
elif [ -f "$LIBRASHADER_DIR/target/release/liblibrashader_capi.a" ]; then
    echo "librashader-capi already built."
else
    echo "Building librashader-capi..."
    cd "$LIBRASHADER_DIR"
    cargo build --release -p librashader-capi --no-default-features --features runtime-opengl,runtime-vulkan,stable
    echo "librashader-capi built."
    cd ../.. 
fi

# -----------------------------
# libretro slang-shaders
# -----------------------------

SLANG_SHADERS_DIR="$THIRD_PARTY/slang-shaders"

if [ -d "$SLANG_SHADERS_DIR" ]; then
    echo "slang-shaders already exists at $SLANG_SHADERS_DIR"
else
    echo "Cloning slang-shaders..."
    # Clone full repo or specific depth? Depth 1 is safer for space.
    git clone --depth 1 https://github.com/libretro/slang-shaders.git "$SLANG_SHADERS_DIR"
    echo "slang-shaders cloned to $SLANG_SHADERS_DIR"
fi

# -----------------------------
# SDL3_image
# -----------------------------

# Try to download the latest release tag from GitHub; fall back to release-3.0.0
SDL_IMAGE_DIR="$THIRD_PARTY/sdl3_image"
SDL_IMAGE_BUILD="$SDL_IMAGE_DIR/build"

if [ "${SKIP_SDL3_BUILD:-}" = "1" ]; then
    echo "SKIP_SDL3_BUILD=1 — skipping SDL3_image (pre-installed)"
elif [ -d "$SDL_IMAGE_BUILD" ]; then
    echo "SDL3_image already built at $SDL_IMAGE_BUILD"
else
    echo "Building SDL3_image..."
    mkdir -p "$SDL_IMAGE_DIR"
    cd "$SDL_IMAGE_DIR"

    SDL_IMAGE_SRC="$SDL_IMAGE_DIR/SDL_image"

    if [ ! -d "$SDL_IMAGE_SRC" ]; then
        echo "Cloning SDL3_image from git..."
        git clone --depth 1 https://github.com/libsdl-org/SDL_image.git "$SDL_IMAGE_SRC"
    fi

    cd "$SDL_IMAGE_SRC"

    mkdir -p build
    cd build

    case "$OS" in
        Darwin|Linux)
            CMAKE_EXTRA_ARGS=""
            if [ "$OS" = "Darwin" ] && [ "$TARGET_ARCH" = "universal" ]; then
                CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
            fi
            cmake .. \
                ${CC:+-DCMAKE_C_COMPILER=$CC} \
                ${CXX:+-DCMAKE_CXX_COMPILER=$CXX} \
                -DCMAKE_INSTALL_PREFIX="$SDL_IMAGE_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                $CMAKE_EXTRA_ARGS
            ;;
        MINGW*|MSYS*|CYGWIN*)
            cmake .. \
                -G "MSYS Makefiles" \
                -DCMAKE_C_COMPILER=gcc \
                -DCMAKE_INSTALL_PREFIX="$SDL_IMAGE_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON
            ;;
    esac

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3_image installed to $SDL_IMAGE_BUILD"

    cd "$SDL_IMAGE_DIR"
fi

# -----------------------------
# ImGui
# -----------------------------

IMGUI_DIR="$THIRD_PARTY/imgui"

if [ -d "$IMGUI_DIR" ]; then
    echo "imgui already exists at $IMGUI_DIR"
else
    echo "Cloning imgui..."
    git clone --branch docking https://github.com/ocornut/imgui.git "$IMGUI_DIR"
    echo "imgui cloned to $IMGUI_DIR"
fi

# -----------------------------
# RmlUi
# -----------------------------

RMLUI_DIR="$THIRD_PARTY/rmlui"

if [ -d "$RMLUI_DIR" ]; then
    echo "rmlui already exists at $RMLUI_DIR"
else
    echo "Cloning RmlUi..."
    git clone --depth 1 --branch 6.2 https://github.com/mikke89/RmlUi.git "$RMLUI_DIR"
    echo "rmlui cloned to $RMLUI_DIR"
fi

# -----------------------------
# SDL_ShaderCross
# -----------------------------

SHADERCROSS_DIR="$THIRD_PARTY/SDL_shadercross"

if [ -d "$SHADERCROSS_DIR" ]; then
    echo "SDL_shadercross already exists at $SHADERCROSS_DIR"
else
    echo "Cloning SDL_shadercross..."
    git clone https://github.com/libsdl-org/SDL_shadercross.git "$SHADERCROSS_DIR"
    cd "$SHADERCROSS_DIR"
    git checkout 7b7365a
    # Only init vendored deps we need (DXC is disabled)
    git submodule update --init --depth 1 external/SPIRV-Cross external/SPIRV-Headers external/SPIRV-Tools
    # DXC is disabled but SDL_ShaderCross checks for its directory unconditionally — create a stub
    mkdir -p external/DirectXShaderCompiler
    echo "# Stub — DXC disabled via SDLSHADERCROSS_DXC=OFF" > external/DirectXShaderCompiler/CMakeLists.txt
    echo "SDL_shadercross cloned to $SHADERCROSS_DIR"
    cd "$ROOT_DIR"
fi

echo "All dependencies installed successfully in $THIRD_PARTY"
