#!/usr/bin/env bash
# build-deps-arm64.sh — Cross-compile all dependencies for aarch64 (ARM64)
# Targets: Raspberry Pi 4, Batocera, RetroPie
#
# Prerequisites (Debian/Ubuntu):
#   sudo dpkg --add-architecture arm64
#   sudo apt update
#   sudo apt install crossbuild-essential-arm64 cmake ninja-build \
#       zlib1g-dev:arm64 libasound2-dev:arm64 libdrm-dev:arm64 \
#       libgbm-dev:arm64 libgles2-mesa-dev:arm64 libegl1-mesa-dev:arm64 \
#       libudev-dev:arm64 libdbus-1-dev:arm64 libx11-dev:arm64
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
THIRD_PARTY="${THIRD_PARTY_DIR:-$ROOT_DIR/third_party_arm64}"

mkdir -p "$THIRD_PARTY"

# ── Cross-compiler detection ────────────────────────────────
CROSS_PREFIX="${CROSS_PREFIX:-aarch64-linux-gnu-}"
CC="${CROSS_PREFIX}gcc"
CXX="${CROSS_PREFIX}g++"

if ! command -v "$CC" &>/dev/null; then
    echo "ERROR: Cross-compiler '$CC' not found."
    echo "Install: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
    exit 1
fi

echo "Cross-compiler: $CC"
echo "Third-party dir: $THIRD_PARTY"

# Point pkg-config at arm64 sysroot libs (for SDL3 backend detection)
if [ -d "/usr/lib/aarch64-linux-gnu/pkgconfig" ]; then
    export PKG_CONFIG_PATH="/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
    export PKG_CONFIG_LIBDIR="/usr/lib/aarch64-linux-gnu/pkgconfig:/usr/share/pkgconfig"
fi

# Common cmake cross-compile arguments
CMAKE_CROSS=(
    -DCMAKE_SYSTEM_NAME=Linux
    -DCMAKE_SYSTEM_PROCESSOR=aarch64
    -DCMAKE_C_COMPILER="$CC"
    -DCMAKE_CXX_COMPILER="$CXX"
)

echo "Using cmake from: $(which cmake)"
cmake --version

# -----------------------------
# FFmpeg  (autotools)
# -----------------------------

FFMPEG="ffmpeg-8.0"
FFMPEG_DIR="$THIRD_PARTY/ffmpeg"
FFMPEG_BUILD="$FFMPEG_DIR/build"

if [ -d "$FFMPEG_BUILD" ]; then
    echo "FFmpeg already built at $FFMPEG_BUILD"
else
    echo "Cross-compiling FFmpeg for aarch64..."
    mkdir -p "$FFMPEG_DIR"
    cd "$FFMPEG_DIR"

    if [ ! -d "$FFMPEG" ]; then
        curl -L -O "https://ffmpeg.org/releases/$FFMPEG.tar.xz"
        tar xf "$FFMPEG.tar.xz"
    fi

    cd "$FFMPEG"
    mkdir -p build && cd build

    ../configure \
        --prefix="$FFMPEG_BUILD" \
        --arch=aarch64 \
        --target-os=linux \
        --cross-prefix="${CROSS_PREFIX}" \
        --enable-cross-compile \
        --disable-all --disable-autodetect \
        --disable-static --enable-shared \
        --enable-avcodec --enable-avformat --enable-avutil --enable-swresample \
        --enable-decoder=adpcm_adx --enable-parser=adx --enable-muxer=adx \
        --enable-pic \
        --extra-cflags="-fPIC" \
        --extra-ldflags="-Wl,-rpath,\$ORIGIN/../lib"

    make -j"$(nproc)"
    make install
    echo "FFmpeg cross-compiled to $FFMPEG_BUILD"

    cd ../..
    rm -rf "$FFMPEG"
    rm -f "$FFMPEG.tar.xz"
    cd "$ROOT_DIR"
fi

# -----------------------------
# SDL3  (cmake)
# -----------------------------

SDL="SDL3-3.4.0"
SDL_DIR="$THIRD_PARTY/sdl3"
SDL_BUILD="$SDL_DIR/build"

if [ -d "$SDL_BUILD" ]; then
    echo "SDL3 already built at $SDL_BUILD"
else
    echo "Cross-compiling SDL3 for aarch64..."
    mkdir -p "$SDL_DIR"
    cd "$SDL_DIR"

    if [ ! -d "$SDL" ]; then
        curl -L -O "https://libsdl.org/release/$SDL.tar.gz"
        tar xf "$SDL.tar.gz"
    fi

    cd "$SDL"
    mkdir -p build && cd build

    cmake .. \
        "${CMAKE_CROSS[@]}" \
        -DCMAKE_INSTALL_PREFIX="$SDL_BUILD" \
        -DBUILD_SHARED_LIBS=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TESTS=OFF

    cmake --build . -j"$(nproc)"
    cmake --install .
    echo "SDL3 cross-compiled to $SDL_BUILD"

    cd ../..
    rm -rf "$SDL"
    rm -f "$SDL.tar.gz"
    cd "$ROOT_DIR"
fi

# -----------------------------
# GekkoNet  (cmake)
# -----------------------------

GEKKONET_REF="7be848c"
GEKKONET_DIR="$THIRD_PARTY/GekkoNet"
GEKKONET_BUILD="$GEKKONET_DIR/build"

if [ -d "$GEKKONET_BUILD" ]; then
    echo "GekkoNet already built at $GEKKONET_BUILD"
else
    echo "Cross-compiling GekkoNet for aarch64..."

    GEKKONET_SRC=$(mktemp -d)
    git clone https://github.com/HeatXD/GekkoNet.git "$GEKKONET_SRC"
    git -C "$GEKKONET_SRC" -c advice.detachedHead=false checkout "$GEKKONET_REF"

    cmake -S "$GEKKONET_SRC" -B "$GEKKONET_SRC/cmake-build" \
        "${CMAKE_CROSS[@]}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DNO_ASIO_BUILD=ON \
        -DBUILD_SHARED_LIBS=OFF

    cmake --build "$GEKKONET_SRC/cmake-build" -j"$(nproc)"

    mkdir -p "$GEKKONET_BUILD/include" "$GEKKONET_BUILD/lib"
    cp -r "$GEKKONET_SRC/GekkoLib/include/." "$GEKKONET_BUILD/include/"
    find "$GEKKONET_SRC" -name "*.a" -exec cp {} "$GEKKONET_BUILD/lib/libGekkoNet.a" \;

    rm -rf "$GEKKONET_SRC"
    echo "GekkoNet cross-compiled to $GEKKONET_BUILD"
fi

# -----------------------------
# SDL3_net  (cmake)
# -----------------------------

SDL3_NET_REF="92022dc"
SDL3_NET_DIR="$THIRD_PARTY/SDL_net"
SDL3_NET_BUILD="$SDL3_NET_DIR/build"

if [ -d "$SDL3_NET_BUILD" ]; then
    echo "SDL3_net already built at $SDL3_NET_BUILD"
else
    echo "Cross-compiling SDL3_net for aarch64..."

    SDL3_NET_SRC=$(mktemp -d)
    git clone https://github.com/libsdl-org/SDL_net.git "$SDL3_NET_SRC"
    git -C "$SDL3_NET_SRC" -c advice.detachedHead=false checkout "$SDL3_NET_REF"

    cmake -S "$SDL3_NET_SRC" -B "$SDL3_NET_SRC/cmake-build" \
        "${CMAKE_CROSS[@]}" \
        -DCMAKE_INSTALL_PREFIX="$SDL3_NET_BUILD" \
        -DCMAKE_PREFIX_PATH="$SDL_BUILD" \
        -DBUILD_SHARED_LIBS=OFF \
        -DSDLNET_INSTALL=ON

    cmake --build "$SDL3_NET_SRC/cmake-build" -j"$(nproc)"
    cmake --install "$SDL3_NET_SRC/cmake-build"

    rm -rf "$SDL3_NET_SRC"
    echo "SDL3_net cross-compiled to $SDL3_NET_BUILD"
fi

# -----------------------------
# libcdio  (autotools)
# -----------------------------

LIBCDIO_VERSION="2.3.0"
LIBCDIO="libcdio-$LIBCDIO_VERSION"
LIBCDIO_DIR="$THIRD_PARTY/libcdio"
LIBCDIO_BUILD="$LIBCDIO_DIR/build"

if [ -d "$LIBCDIO_DIR" ]; then
    echo "libcdio already built at $LIBCDIO_BUILD"
else
    echo "Cross-compiling libcdio for aarch64..."
    mkdir -p "$LIBCDIO_DIR"
    cd "$LIBCDIO_DIR"

    if [ ! -d "$LIBCDIO" ]; then
        curl -L -O "https://github.com/libcdio/libcdio/releases/download/$LIBCDIO_VERSION/$LIBCDIO.tar.gz"
        tar xf "$LIBCDIO.tar.gz"
    fi

    cd "$LIBCDIO"
    mkdir -p build && cd build

    ../configure MAKE=make \
        --host=aarch64-linux-gnu \
        --prefix="$LIBCDIO_BUILD" \
        --enable-static \
        --disable-shared \
        --disable-cxx \
        --disable-example-progs

    make -j"$(nproc)"
    make install
    echo "libcdio cross-compiled to $LIBCDIO_BUILD"

    cd ../..
    rm -rf "$LIBCDIO"
    rm -f "$LIBCDIO.tar.gz"
    cd "$ROOT_DIR"
fi

# -----------------------------
# minizip-ng  (cmake)
# -----------------------------

MINIZIP_NG_TAG="4.1.0"
MINIZIP_NG_DIR="$THIRD_PARTY/minizip-ng"
MINIZIP_NG_BUILD="$MINIZIP_NG_DIR/build"

if [ -d "$MINIZIP_NG_BUILD" ]; then
    echo "minizip-ng already built at $MINIZIP_NG_BUILD"
else
    echo "Cross-compiling minizip-ng for aarch64..."

    mkdir -p "$MINIZIP_NG_BUILD"
    MINIZIP_NG_SRC=$(mktemp -d)

    git clone \
        --branch "$MINIZIP_NG_TAG" \
        --single-branch \
        https://github.com/zlib-ng/minizip-ng \
        "$MINIZIP_NG_SRC"

    cmake -S "$MINIZIP_NG_SRC" -B "$MINIZIP_NG_SRC/cmake-build" \
        "${CMAKE_CROSS[@]}" \
        -DCMAKE_INSTALL_PREFIX="$MINIZIP_NG_BUILD" \
        -DMZ_COMPAT=OFF \
        -DMZ_ZLIB_FLAVOR=zlib \
        -DMZ_BZIP2=OFF \
        -DMZ_LZMA=OFF \
        -DMZ_PPMD=OFF \
        -DMZ_ZSTD=OFF \
        -DMZ_LIBCOMP=OFF \
        -DMZ_PKCRYPT=OFF \
        -DMZ_WZAES=OFF \
        -DMZ_OPENSSL=OFF \
        -DMZ_LIBBSD=OFF \
        -DMZ_DECOMPRESS_ONLY=ON

    cmake --build "$MINIZIP_NG_SRC/cmake-build" -j"$(nproc)"
    cmake --install "$MINIZIP_NG_SRC/cmake-build"

    rm -rf "$MINIZIP_NG_SRC"
    echo "minizip-ng cross-compiled to $MINIZIP_NG_BUILD"
fi

# -----------------------------
# tf-psa-crypto  (cmake)
# -----------------------------

TF_PSA_CRYPTO_VERSION="1.0.0"
TF_PSA_CRYPTO_URL="https://github.com/Mbed-TLS/TF-PSA-Crypto/releases/download/tf-psa-crypto-$TF_PSA_CRYPTO_VERSION/tf-psa-crypto-$TF_PSA_CRYPTO_VERSION.tar.bz2"
TF_PSA_CRYPTO_DIR="$THIRD_PARTY/tf-psa-crypto"
TF_PSA_CRYPTO_BUILD="$TF_PSA_CRYPTO_DIR/build"

if [ -d "$TF_PSA_CRYPTO_BUILD" ]; then
    echo "tf-psa-crypto already built at $TF_PSA_CRYPTO_BUILD"
else
    echo "Cross-compiling tf-psa-crypto for aarch64..."

    mkdir -p "$TF_PSA_CRYPTO_BUILD"
    TF_PSA_CRYPTO_SRC=$(mktemp -d)

    curl -L -o "$TF_PSA_CRYPTO_SRC/tf-psa-crypto.tar.bz2" "$TF_PSA_CRYPTO_URL"
    tar xf "$TF_PSA_CRYPTO_SRC/tf-psa-crypto.tar.bz2" -C "$TF_PSA_CRYPTO_SRC"

    cmake -S "$TF_PSA_CRYPTO_SRC/tf-psa-crypto-$TF_PSA_CRYPTO_VERSION" -B "$TF_PSA_CRYPTO_SRC/cmake-build" \
        "${CMAKE_CROSS[@]}" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$TF_PSA_CRYPTO_BUILD" \
        -DENABLE_PROGRAMS=OFF \
        -DENABLE_TESTING=OFF \
        -DUSE_SHARED_TF_PSA_CRYPTO_LIBRARY=OFF \
        -DUSE_STATIC_TF_PSA_CRYPTO_LIBRARY=ON \
        -DTF_PSA_CRYPTO_CONFIG_FILE="configs/crypto-config-ccm-aes-sha256.h"

    cmake --build "$TF_PSA_CRYPTO_SRC/cmake-build" -j"$(nproc)"
    cmake --install "$TF_PSA_CRYPTO_SRC/cmake-build"

    rm -rf "$TF_PSA_CRYPTO_SRC"
    echo "tf-psa-crypto cross-compiled to $TF_PSA_CRYPTO_BUILD"
fi

echo ""
echo "All ARM64 dependencies built successfully in $THIRD_PARTY"
