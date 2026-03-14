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
# ControllerImage (controller button glyphs)
# -----------------------------

CTRLIMG_DIR="$THIRD_PARTY/controllerimage"

if [ -d "$CTRLIMG_DIR" ]; then
    echo "ControllerImage already exists at $CTRLIMG_DIR"
else
    echo "Cloning ControllerImage..."
    git clone --depth 1 https://github.com/icculus/ControllerImage.git "$CTRLIMG_DIR"
    echo "ControllerImage cloned to $CTRLIMG_DIR"
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
# -----------------------------
# SDL3_mixer
# -----------------------------

SDL_MIXER_DIR="$THIRD_PARTY/sdl3_mixer"
SDL_MIXER_BUILD="$SDL_MIXER_DIR/build"

if [ "${SKIP_SDL3_BUILD:-}" = "1" ]; then
    echo "SKIP_SDL3_BUILD=1 — skipping SDL3_mixer (pre-installed)"
elif [ -d "$SDL_MIXER_BUILD" ]; then
    echo "SDL3_mixer already built at $SDL_MIXER_BUILD"
else
    echo "Building SDL3_mixer..."
    mkdir -p "$SDL_MIXER_DIR"
    cd "$SDL_MIXER_DIR"

    SDL_MIXER_SRC="$SDL_MIXER_DIR/SDL_mixer"

    if [ ! -d "$SDL_MIXER_SRC" ]; then
        echo "Cloning SDL3_mixer from git..."
        git clone --depth 1 https://github.com/libsdl-org/SDL_mixer.git "$SDL_MIXER_SRC"
    fi

    # Clone vendored codec libraries (replaces git submodules)
    MIXER_EXT="$SDL_MIXER_SRC/external"
    clone_ext() { [ -d "$MIXER_EXT/$1" ] && [ "$(ls -A "$MIXER_EXT/$1")" ] || git clone --depth 1 --branch "$3" "$2" "$MIXER_EXT/$1"; }
    clone_ext ogg       https://github.com/libsdl-org/ogg.git            v1.3.5-SDL
    clone_ext vorbis    https://github.com/libsdl-org/vorbis.git         v1.3.7-SDL
    clone_ext flac      https://github.com/libsdl-org/flac.git           1.3.4-SDL
    clone_ext opus      https://github.com/libsdl-org/opus.git           v1.4.x-SDL
    clone_ext opusfile  https://github.com/libsdl-org/opusfile.git       v0.13-git-SDL
    clone_ext mpg123    https://github.com/libsdl-org/mpg123.git         v1.33.4-SDL
    clone_ext libxmp    https://github.com/libsdl-org/libxmp.git         4.7.0-SDL
    clone_ext libgme    https://github.com/libsdl-org/game-music-emu.git v0.6.4-SDL
    clone_ext wavpack   https://github.com/libsdl-org/wavpack.git        5.9.0-SDL
    clone_ext tremor    https://github.com/libsdl-org/tremor.git         v1.2.1-SDL

    cd "$SDL_MIXER_SRC"

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
                -DCMAKE_INSTALL_PREFIX="$SDL_MIXER_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDLMIXER_VENDORED=ON \
                -DSDLMIXER_EXAMPLES=OFF \
                $CMAKE_EXTRA_ARGS
            ;;
        MINGW*|MSYS*|CYGWIN*)
            cmake .. \
                -G "MSYS Makefiles" \
                -DCMAKE_C_COMPILER=gcc \
                -DCMAKE_INSTALL_PREFIX="$SDL_MIXER_BUILD" \
                -DSDL3_DIR="$SDL_BUILD/lib/cmake/SDL3" \
                -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
                -DBUILD_SHARED_LIBS=ON \
                -DSDLMIXER_VENDORED=ON \
                -DSDLMIXER_EXAMPLES=OFF
            ;;
    esac

    cmake --build . -j$(nproc)
    cmake --install .
    echo "SDL3_mixer installed to $SDL_MIXER_BUILD"

    cd "$ROOT_DIR"
fi

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
# FreeType (required by RmlUi)
# -----------------------------

FREETYPE_DIR="$THIRD_PARTY/freetype"
FREETYPE_BUILD="$FREETYPE_DIR/build"

if [ -d "$FREETYPE_BUILD" ]; then
    echo "FreeType already built at $FREETYPE_BUILD"
else
    echo "Building FreeType..."
    if [ ! -d "$FREETYPE_DIR" ]; then
        git clone --depth 1 --branch VER-2-13-3 https://github.com/freetype/freetype.git "$FREETYPE_DIR"
    fi
    cd "$FREETYPE_DIR"
    mkdir -p build_temp
    cd build_temp

    CMAKE_EXTRA_ARGS=""
    case "$OS" in
        Darwin)
            if [ "$TARGET_ARCH" = "universal" ]; then
                CMAKE_EXTRA_ARGS="-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64"
            fi
            ;;
        MINGW*|MSYS*|CYGWIN*)
            CMAKE_EXTRA_ARGS="-G \"MSYS Makefiles\""
            ;;
    esac

    eval cmake .. \
        ${CC:+-DCMAKE_C_COMPILER=$CC} \
        ${CXX:+-DCMAKE_CXX_COMPILER=$CXX} \
        -DCMAKE_INSTALL_PREFIX="$FREETYPE_BUILD" \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_ZLIB=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON \
        $CMAKE_EXTRA_ARGS

    cmake --build . -j$(nproc)
    cmake --install .
    echo "FreeType installed to $FREETYPE_BUILD"

    cd "$FREETYPE_DIR"
    rm -rf build_temp
fi

# -----------------------------
# Lua 5.4 (required by RmlUi Lua bindings)
# -----------------------------

LUA_DIR="$THIRD_PARTY/lua"

if [ -d "$LUA_DIR/src" ]; then
    echo "Lua 5.4 already exists at $LUA_DIR"
else
    echo "Cloning Lua 5.4..."
    mkdir -p "$LUA_DIR"
    git clone --depth 1 --branch v5.4.7 https://github.com/lua/lua.git "$LUA_DIR/src_repo"
    # Move source files into the expected layout (src/ directory)
    mkdir -p "$LUA_DIR/src"
    cp "$LUA_DIR/src_repo/"*.c "$LUA_DIR/src/"
    cp "$LUA_DIR/src_repo/"*.h "$LUA_DIR/src/"
    rm -rf "$LUA_DIR/src_repo"

    # Create CMakeLists.txt for building Lua as a static library
    cat > "$LUA_DIR/CMakeLists.txt" << 'LUACMAKE'
# Lua 5.4 — built as a static C library for RmlUi Lua bindings
# This CMakeLists.txt builds the core Lua library (liblua) without
# the standalone interpreter (lua.c) or compiler (luac.c).

cmake_minimum_required(VERSION 3.10)
project(lua LANGUAGES C)

set(LUA_SRC
    src/lapi.c src/lauxlib.c src/lbaselib.c src/lcode.c src/lcorolib.c
    src/lctype.c src/ldblib.c src/ldebug.c src/ldo.c src/ldump.c
    src/lfunc.c src/lgc.c src/linit.c src/liolib.c src/llex.c
    src/lmathlib.c src/lmem.c src/loadlib.c src/lobject.c src/lopcodes.c
    src/loslib.c src/lparser.c src/lstate.c src/lstring.c src/lstrlib.c
    src/ltable.c src/ltablib.c src/ltm.c src/lundump.c src/lutf8lib.c
    src/lvm.c src/lzio.c
)

add_library(lua_static STATIC ${LUA_SRC})
target_include_directories(lua_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Suppress warnings in vendored Lua source
target_compile_options(lua_static PRIVATE -w)

# Create the Lua::Lua IMPORTED target that RmlUi's Dependencies.cmake expects.
# We use INTERFACE IMPORTED (not ALIAS) so RmlUi can alias it again as
# RmlUi::External::Lua without hitting CMake's alias-of-alias restriction.
add_library(Lua::Lua INTERFACE IMPORTED GLOBAL)
set_target_properties(Lua::Lua PROPERTIES
    INTERFACE_LINK_LIBRARIES lua_static
    INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

# Also set the variables that FindLua would normally set
set(LUA_FOUND TRUE PARENT_SCOPE)
set(LUA_LIBRARIES lua_static PARENT_SCOPE)
set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" PARENT_SCOPE)
set(LUA_VERSION_STRING "5.4.7" PARENT_SCOPE)
LUACMAKE

    echo "Lua 5.4 installed to $LUA_DIR"
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
