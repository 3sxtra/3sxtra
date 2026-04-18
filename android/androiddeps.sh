#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Android dependency builder for the official/simplified branch.
# Only builds the libraries actually used: SDL3, GekkoNet, SDL3_net,
# minizip-ng, tf-psa-crypto, ffmpeg (for ADX audio decoding).
# No SDL_image, SDL_mixer, Lua, RmlUi, Freetype, librashader, or GLAD.
# ==============================================================================

if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    echo "Error: ANDROID_NDK_ROOT environment variable is not set."
    echo "Please set it to the path of your Android NDK."
    echo "Example: export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/28.2.13676358"
    exit 1
fi

if [ ! -d "$ANDROID_NDK_ROOT" ]; then
    echo "Error: Android NDK not found at $ANDROID_NDK_ROOT"
    exit 1
fi

HOST_OS="$(uname -s)"
if [[ "$HOST_OS" == MINGW* || "$HOST_OS" == MSYS* || "$HOST_OS" == CYGWIN* ]]; then
    if ! command -v cygpath &> /dev/null; then
        echo "Error: 'cygpath' command not found."
        exit 1
    fi
    ANDROID_NDK_ROOT=$(cygpath -m "$ANDROID_NDK_ROOT" | tr -d '\r')
fi

ANDROID_API_LEVEL=24
TARGET_ABIS=("arm64-v8a" "armeabi-v7a" "x86_64" "x86")

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
if [[ "$HOST_OS" == MINGW* || "$HOST_OS" == MSYS* || "$HOST_OS" == CYGWIN* ]]; then
    ROOT_DIR=$(cygpath -m "$ROOT_DIR" | tr -d '\r')
fi
THIRD_PARTY="$ROOT_DIR/../third_party_android"
mkdir -p "$THIRD_PARTY"

case "$HOST_OS" in
    Darwin)    HOST_TAG="darwin-x86_64" ;;
    Linux)     HOST_TAG="linux-x86_64" ;;
    MINGW*|MSYS*|CYGWIN*) HOST_TAG="windows-x86_64" ;;
    *)         echo "Unsupported host OS: $HOST_OS"; exit 1 ;;
esac

TOOLCHAIN="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake"

# Map ABI to arch triplet for the NDK toolchain
abi_to_arch() {
    case "$1" in
        arm64-v8a)   echo "aarch64-linux-android" ;;
        armeabi-v7a) echo "armv7a-linux-androideabi" ;;
        x86_64)      echo "x86_64-linux-android" ;;
        x86)         echo "i686-linux-android" ;;
    esac
}

# ==============================================================================
# Clone sources
# ==============================================================================
clone_sources() {
    echo "=== Cloning source repositories ==="

    # SDL3 src
    if [ ! -d "$THIRD_PARTY/sdl3-src" ]; then
        git clone --depth 1 https://github.com/libsdl-org/SDL.git "$THIRD_PARTY/sdl3-src"
    fi

    # SDL3_net src
    if [ ! -d "$THIRD_PARTY/SDL_net-src" ]; then
        git clone --depth 1 https://github.com/libsdl-org/SDL_net.git "$THIRD_PARTY/SDL_net-src"
    fi

    # GekkoNet
    if [ ! -d "$THIRD_PARTY/GekkoNet" ]; then
        git clone --depth 1 https://github.com/HeatXD/GekkoNet.git "$THIRD_PARTY/GekkoNet"
    fi

    # minizip-ng
    if [ ! -d "$THIRD_PARTY/minizip-ng-src" ]; then
        git clone --depth 1 https://github.com/zlib-ng/minizip-ng.git "$THIRD_PARTY/minizip-ng-src"
    fi

    # tf-psa-crypto src
    if [ ! -d "$THIRD_PARTY/tf-psa-crypto-src" ]; then
        mkdir -p "$THIRD_PARTY/tf-psa-crypto-src"
        curl -L -o "$THIRD_PARTY/tf-psa-crypto.tar.bz2" "https://github.com/Mbed-TLS/TF-PSA-Crypto/releases/download/tf-psa-crypto-1.0.0/tf-psa-crypto-1.0.0.tar.bz2"
        tar xf "$THIRD_PARTY/tf-psa-crypto.tar.bz2" --no-same-owner -C "$THIRD_PARTY/tf-psa-crypto-src" --strip-components=1
        rm "$THIRD_PARTY/tf-psa-crypto.tar.bz2"
    fi

    # ffmpeg (needed for ADX audio decoding)
    if [ ! -d "$THIRD_PARTY/ffmpeg-src" ]; then
        git clone --depth 1 --branch release/7.1 https://github.com/FFmpeg/FFmpeg.git "$THIRD_PARTY/ffmpeg-src"
    fi
}

# ==============================================================================
# Build SDL3
# ==============================================================================
build_sdl3() {
    local ABI=$1
    local BUILD_DIR="$THIRD_PARTY/sdl3/build-$ABI"
    local INSTALL_DIR="$THIRD_PARTY/sdl3/build/$ABI"

    if [ -f "$INSTALL_DIR/lib/libSDL3.so" ]; then
        echo "SDL3 ($ABI): already built, skipping."
        return
    fi

    echo "=== Building SDL3 for $ABI ==="
    cmake -S "$THIRD_PARTY/sdl3-src" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DSDL_SHARED=ON \
        -DSDL_STATIC=OFF \
        -DSDL_TEST=OFF \
        -G Ninja

    cmake --build "$BUILD_DIR" --parallel
    cmake --install "$BUILD_DIR"
}

# ==============================================================================
# Build SDL3_net
# ==============================================================================
build_sdl3_net() {
    local ABI=$1
    local BUILD_DIR="$THIRD_PARTY/SDL_net/build-$ABI"
    local INSTALL_DIR="$THIRD_PARTY/SDL_net/build/$ABI"

    if [ -f "$INSTALL_DIR/lib/libSDL3_net.so" ]; then
        echo "SDL3_net ($ABI): already built, skipping."
        return
    fi

    echo "=== Building SDL3_net for $ABI ==="
    cmake -S "$THIRD_PARTY/SDL_net-src" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DCMAKE_PREFIX_PATH="$THIRD_PARTY/sdl3/build/$ABI" \
        -DSDL3_DIR="$THIRD_PARTY/sdl3/build/$ABI/lib/cmake/SDL3" \
        -DSDL3NET_SHARED=ON \
        -DSDL3NET_STATIC=OFF \
        -G Ninja

    cmake --build "$BUILD_DIR" --parallel
    cmake --install "$BUILD_DIR"
}

# ==============================================================================
# Build minizip-ng
# ==============================================================================
build_minizip_ng() {
    local ABI=$1
    local BUILD_DIR="$THIRD_PARTY/minizip-ng/build-$ABI"
    local INSTALL_DIR="$THIRD_PARTY/minizip-ng/build/$ABI"

    if [ -f "$INSTALL_DIR/lib/libminizip-ng.a" ]; then
        echo "minizip-ng ($ABI): already built, skipping."
        return
    fi

    echo "=== Building minizip-ng for $ABI ==="
    cmake -S "$THIRD_PARTY/minizip-ng-src" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DFETCHCONTENT_BASE_DIR="$THIRD_PARTY/minizip-ng-src/_deps" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DMZ_BZIP2=OFF \
        -DMZ_LZMA=OFF \
        -DMZ_ZSTD=OFF \
        -DMZ_OPENSSL=OFF \
        -DMZ_FETCH_LIBS=OFF \
        -DBUILD_SHARED_LIBS=OFF \
        -G Ninja

    cmake --build "$BUILD_DIR" --parallel
    cmake --install "$BUILD_DIR"
}

# ==============================================================================
# Build tf-psa-crypto
# ==============================================================================
build_tf_psa_crypto() {
    local ABI=$1
    local BUILD_DIR="$THIRD_PARTY/tf-psa-crypto/build-$ABI"
    local INSTALL_DIR="$THIRD_PARTY/tf-psa-crypto/build/$ABI"

    if [ -f "$INSTALL_DIR/lib/libtfpsacrypto.a" ]; then
        echo "tf-psa-crypto ($ABI): already built, skipping."
        return
    fi

    echo "=== Building tf-psa-crypto for $ABI ==="
    cmake -S "$THIRD_PARTY/tf-psa-crypto-src" -B "$BUILD_DIR" \
        -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
        -DANDROID_ABI="$ABI" \
        -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$INSTALL_DIR" \
        -DBUILD_SHARED_LIBS=OFF \
        -G Ninja

    cmake --build "$BUILD_DIR" --parallel
    cmake --install "$BUILD_DIR"
}

# ==============================================================================
# Build ffmpeg (uses its own configure, not CMake)
# ==============================================================================
build_ffmpeg() {
    local ABI=$1
    local ARCH_TRIPLET
    ARCH_TRIPLET=$(abi_to_arch "$ABI")
    local BUILD_DIR="$THIRD_PARTY/ffmpeg/build-$ABI"
    local INSTALL_DIR="$THIRD_PARTY/ffmpeg/build/$ABI"

    if [ -f "$INSTALL_DIR/lib/libavcodec.so" ]; then
        echo "ffmpeg ($ABI): already built, skipping."
        return
    fi

    echo "=== Building ffmpeg for $ABI ==="

    # NDK toolchain paths
    local TOOLCHAIN_BIN="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/bin"
    local CC="$TOOLCHAIN_BIN/${ARCH_TRIPLET}${ANDROID_API_LEVEL}-clang"
    local CXX="$TOOLCHAIN_BIN/${ARCH_TRIPLET}${ANDROID_API_LEVEL}-clang++"
    local AR="$TOOLCHAIN_BIN/llvm-ar"
    local STRIP="$TOOLCHAIN_BIN/llvm-strip"
    local NM="$TOOLCHAIN_BIN/llvm-nm"
    local RANLIB="$TOOLCHAIN_BIN/llvm-ranlib"

    # Map ABI to ffmpeg arch
    local FFMPEG_ARCH
    case "$ABI" in
        arm64-v8a)   FFMPEG_ARCH="aarch64" ;;
        armeabi-v7a) FFMPEG_ARCH="arm" ;;
        x86_64)      FFMPEG_ARCH="x86_64" ;;
        x86)         FFMPEG_ARCH="x86" ;;
    esac

    mkdir -p "$BUILD_DIR"
    pushd "$BUILD_DIR" > /dev/null

    "$THIRD_PARTY/ffmpeg-src/configure" \
        --prefix="$INSTALL_DIR" \
        --target-os=android \
        --arch="$FFMPEG_ARCH" \
        --enable-cross-compile \
        --cross-prefix="$TOOLCHAIN_BIN/llvm-" \
        --host-cc="clang" \
        --host-ld="clang" \
        --cc="$CC" \
        --cxx="$CXX" \
        --ar="$AR" \
        --strip="$STRIP" \
        --nm="$NM" \
        --ranlib="$RANLIB" \
        --enable-shared \
        --disable-static \
        --disable-programs \
        --disable-doc \
        --disable-avdevice \
        --disable-avformat \
        --disable-avfilter \
        --disable-swscale \
        --disable-postproc \
        --disable-network \
        --disable-everything \
        --enable-decoder=adpcm_adx \
        --enable-parser=adx \
        --enable-swresample \
        --disable-asm

    make -j"$(nproc)"
    make install

    popd > /dev/null
}

# ==============================================================================
# Main
# ==============================================================================
clone_sources

for ABI in "${TARGET_ABIS[@]}"; do
    echo ""
    echo "================================================================"
    echo "Building all dependencies for ABI: $ABI"
    echo "================================================================"

    build_sdl3 "$ABI"
    build_sdl3_net "$ABI"
    build_minizip_ng "$ABI"
    build_tf_psa_crypto "$ABI"
    build_ffmpeg "$ABI"
done

echo ""
echo "=== All Android dependencies built successfully ==="
