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
# imgui
# -----------------------------

IMGUI_DIR="$THIRD_PARTY/imgui"

if [ -d "$IMGUI_DIR" ]; then
    echo "imgui already exists."
else
    git clone --branch docking https://github.com/ocornut/imgui.git "$IMGUI_DIR"
fi

echo "All sources downloaded to $THIRD_PARTY"
