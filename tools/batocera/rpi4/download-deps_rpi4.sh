#!/usr/bin/env bash
# download-deps_rpi4.sh - Download source tarballs for RPi4 cross-compilation
# Run this on the host BEFORE entering the Batocera Docker container.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
THIRD_PARTY="$ROOT_DIR/third_party_rpi4"

mkdir -p "$THIRD_PARTY"

# -----------------------------
# SDL3
# -----------------------------

SDL_DIR="$THIRD_PARTY/sdl3"
mkdir -p "$SDL_DIR"

if [ -d "$SDL_DIR/SDL" ]; then
    echo "SDL3 source already exists at $SDL_DIR/SDL"
else
    echo "Cloning SDL3..."
    git clone --depth 1 https://github.com/libsdl-org/SDL.git "$SDL_DIR/SDL"
    echo "SDL3 cloned."
fi

# -----------------------------
# SDL3_image
# -----------------------------

SDL_IMAGE_DIR="$THIRD_PARTY/sdl3_image"
mkdir -p "$SDL_IMAGE_DIR"

SDL_IMAGE_TAG="$(curl -s https://api.github.com/repos/libsdl-org/SDL_image/releases/latest | grep -Po '"tag_name":\s*"\K[^"]+' || true)"
if [ -z "$SDL_IMAGE_TAG" ]; then
    SDL_IMAGE_TAG="release-3.0.0"
fi

ARCHIVE_URL="https://github.com/libsdl-org/SDL_image/archive/refs/tags/$SDL_IMAGE_TAG.tar.gz"
ARCHIVE_FILE="$SDL_IMAGE_DIR/SDL_image-$SDL_IMAGE_TAG.tar.gz"

if [ -d "$SDL_IMAGE_DIR/SDL_image-$SDL_IMAGE_TAG" ]; then
    echo "SDL3_image source already exists."
else
    echo "Downloading SDL_image $SDL_IMAGE_TAG..."
    curl -L -o "$ARCHIVE_FILE" "$ARCHIVE_URL"
    cd "$SDL_IMAGE_DIR"
    tar xf "$ARCHIVE_FILE" --exclude "*/Xcode"
    rm -f "$ARCHIVE_FILE"
    echo "SDL3_image downloaded."
fi

# -----------------------------
# SDL3_mixer
# -----------------------------

SDL_MIXER_DIR="$THIRD_PARTY/sdl3_mixer"
mkdir -p "$SDL_MIXER_DIR"

if [ -d "$SDL_MIXER_DIR/SDL_mixer" ]; then
    echo "SDL3_mixer source already exists."
else
    echo "Cloning SDL3_mixer..."
    git clone --depth 1 https://github.com/libsdl-org/SDL_mixer.git "$SDL_MIXER_DIR/SDL_mixer"
    echo "SDL3_mixer cloned."
fi

# Clone vendored codec libraries (replaces git submodules)
MIXER_EXT="$SDL_MIXER_DIR/SDL_mixer/external"
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

# -----------------------------
# SDL3_net
# -----------------------------

SDL_NET_DIR="$THIRD_PARTY/sdl3_net"
mkdir -p "$SDL_NET_DIR"

if [ -d "$SDL_NET_DIR/SDL_net" ]; then
    echo "SDL3_net source already exists."
else
    echo "Cloning SDL3_net..."
    git clone --depth 1 https://github.com/libsdl-org/SDL_net.git "$SDL_NET_DIR/SDL_net"
    echo "SDL3_net cloned."
fi

# -----------------------------
# stb (header-only)
# -----------------------------

STB_DIR="$THIRD_PARTY/stb"
mkdir -p "$STB_DIR"

if [ ! -f "$STB_DIR/stb_truetype.h" ]; then
    curl -L -o "$STB_DIR/stb_truetype.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"
fi

if [ ! -f "$STB_DIR/stb_image.h" ]; then
    curl -L -o "$STB_DIR/stb_image.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"
fi

# -----------------------------
# simde (header-only)
# -----------------------------

SIMDE_DIR="$THIRD_PARTY/simde"

if [ -d "$SIMDE_DIR" ]; then
    echo "simde already exists."
else
    git clone --depth 1 https://github.com/simd-everywhere/simde.git "$SIMDE_DIR"
fi

# -----------------------------
# glad
# -----------------------------

GLAD_DIR="$THIRD_PARTY/glad"

if [ -d "$GLAD_DIR" ]; then
    echo "glad already exists."
else
    git clone --branch v2.0.8 https://github.com/Dav1dde/glad.git "$GLAD_DIR"
fi

# -----------------------------
# librashader
# -----------------------------

LIBRASHADER_DIR="$THIRD_PARTY/librashader"

if [ -d "$LIBRASHADER_DIR" ]; then
    echo "librashader already exists."
else
    git clone https://github.com/SnowflakePowered/librashader.git "$LIBRASHADER_DIR"
fi

# -----------------------------
# slang-shaders (data files)
# -----------------------------

SLANG_SHADERS_DIR="$THIRD_PARTY/slang-shaders"

if [ -d "$SLANG_SHADERS_DIR" ]; then
    echo "slang-shaders already exists."
else
    git clone --depth 1 https://github.com/libretro/slang-shaders.git "$SLANG_SHADERS_DIR"
fi

# -----------------------------
# SDL_shadercross
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


# -----------------------------
# Freetype (required by RmlUi)
# -----------------------------

FREETYPE_DIR="$THIRD_PARTY/freetype"
FREETYPE_BUILD="$FREETYPE_DIR/build"

if [ -f "$FREETYPE_BUILD/lib/libfreetype.a" ]; then
    echo "Freetype already built for aarch64."
else
    echo "Building Freetype for aarch64 cross-compilation..."
    if [ ! -d "$FREETYPE_DIR/src" ]; then
        FREETYPE_VER="2.13.3"
        curl -L -o "$THIRD_PARTY/freetype-$FREETYPE_VER.tar.xz" \
            "https://download.savannah.gnu.org/releases/freetype/freetype-$FREETYPE_VER.tar.xz"
        mkdir -p "$FREETYPE_DIR"
        tar xf "$THIRD_PARTY/freetype-$FREETYPE_VER.tar.xz" -C "$FREETYPE_DIR" --strip-components=1
        rm -f "$THIRD_PARTY/freetype-$FREETYPE_VER.tar.xz"
    fi
    cmake -S "$FREETYPE_DIR" -B "$FREETYPE_BUILD" \
        -DCMAKE_SYSTEM_NAME=Linux \
        -DCMAKE_SYSTEM_PROCESSOR=aarch64 \
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc \
        -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF \
        -DFT_DISABLE_HARFBUZZ=ON \
        -DFT_DISABLE_BROTLI=ON \
        -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON \
        -DFT_DISABLE_ZLIB=ON
    cmake --build "$FREETYPE_BUILD" -j"$(nproc)"
    cmake --install "$FREETYPE_BUILD" --prefix "$FREETYPE_BUILD"
    echo "Freetype built successfully."
fi

# -----------------------------
# RmlUi
# -----------------------------

RMLUI_DIR="$THIRD_PARTY/rmlui"

if [ -d "$RMLUI_DIR" ]; then
    echo "rmlui already exists."
else
    git clone --depth 1 --branch 6.2 https://github.com/mikke89/RmlUi.git "$RMLUI_DIR"
fi

# -----------------------------
# GekkoNet (rollback networking)
# -----------------------------

GEKKONET_DIR="$THIRD_PARTY/GekkoNet"

if [ -d "$GEKKONET_DIR" ]; then
    echo "GekkoNet already exists."
else
    git clone --depth 1 https://github.com/HeatXD/GekkoNet.git "$GEKKONET_DIR"
fi

# -----------------------------
# ControllerImage (controller button glyphs)
# -----------------------------

CTRLIMG_DIR="$THIRD_PARTY/controllerimage"

if [ -d "$CTRLIMG_DIR" ]; then
    echo "ControllerImage already exists."
else
    git clone --depth 1 https://github.com/icculus/ControllerImage.git "$CTRLIMG_DIR"
fi

echo "All sources downloaded to $THIRD_PARTY"
