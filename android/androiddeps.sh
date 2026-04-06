#!/usr/bin/env bash
set -euo pipefail

# ==============================================================================
# Configuration
# ==============================================================================

if [ -z "${ANDROID_NDK_ROOT:-}" ]; then
    echo "Error: ANDROID_NDK_ROOT environment variable is not set."
    echo "Please set it to the path of your Android NDK."
    echo "Example: export ANDROID_NDK_ROOT=~/Android/Sdk/ndk/26.1.10909125"
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
    ANDROID_NDK_ROOT=$(cygpath -m "$ANDROID_NDK_ROOT")
fi

ANDROID_API_LEVEL=24
TARGET_ABIS=("arm64-v8a" "armeabi-v7a" "x86_64" "x86")

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
THIRD_PARTY="$ROOT_DIR/../third_party_android"
mkdir -p "$THIRD_PARTY"

case "$HOST_OS" in
    Darwin)    HOST_TAG="darwin-x86_64" ;;
    Linux)     HOST_TAG="linux-x86_64" ;;
    MINGW*|MSYS*|CYGWIN*) HOST_TAG="windows-x86_64" ;;
    *)         echo "Unsupported host OS: $HOST_OS"; exit 1 ;;
esac

JOBS=$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
CMAKE_PROGRAM=$(which cmake)

echo "Using Android NDK: $ANDROID_NDK_ROOT"

# ==============================================================================
# Source Preparation
# ==============================================================================

fetch_source() {
    # simde
    if [ ! -d "$THIRD_PARTY/simde" ]; then git clone https://github.com/simd-everywhere/simde.git "$THIRD_PARTY/simde"; fi
    # stb
    mkdir -p "$THIRD_PARTY/stb"
    if [ ! -f "$THIRD_PARTY/stb/stb_truetype.h" ]; then curl -L -o "$THIRD_PARTY/stb/stb_truetype.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_truetype.h"; fi
    if [ ! -f "$THIRD_PARTY/stb/stb_image.h" ]; then curl -L -o "$THIRD_PARTY/stb/stb_image.h" "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"; fi
    # GekkoNet
    if [ ! -d "$THIRD_PARTY/GekkoNet" ]; then git clone --depth 1 https://github.com/HeatXD/GekkoNet.git "$THIRD_PARTY/GekkoNet"; fi
    # cJSON
    if [ ! -d "$THIRD_PARTY/cJSON" ]; then git clone --depth 1 https://github.com/DaveGamble/cJSON.git "$THIRD_PARTY/cJSON"; fi
    # glad
    if [ ! -d "$THIRD_PARTY/glad" ]; then git clone --branch v2.0.8 https://github.com/Dav1dde/glad.git "$THIRD_PARTY/glad"; fi
    if [ ! -d "$THIRD_PARTY/glad_generated/include/glad" ]; then
        echo "Generating GLAD..."
        cd "$THIRD_PARTY/glad" && python -m glad --api gl:core=4.6 --reproducible --out-path "../glad_generated" c --loader
        cd "$ROOT_DIR"
    fi
    # SDL_shadercross
    if [ ! -d "$THIRD_PARTY/SDL_shadercross" ]; then 
        git clone https://github.com/libsdl-org/SDL_shadercross.git "$THIRD_PARTY/SDL_shadercross"
        cd "$THIRD_PARTY/SDL_shadercross" && git submodule update --init --recursive
        cd "$ROOT_DIR"
    fi
    # RmlUi
    if [ ! -d "$THIRD_PARTY/rmlui" ]; then git clone --depth 1 --branch 6.2 https://github.com/mikke89/RmlUi.git "$THIRD_PARTY/rmlui"; fi
    
    # Lua 5.4
    if [ ! -d "$THIRD_PARTY/lua" ]; then
        mkdir -p "$THIRD_PARTY/lua"
        git clone --depth 1 --branch v5.4.7 https://github.com/lua/lua.git "$THIRD_PARTY/lua/src_repo"
        mkdir -p "$THIRD_PARTY/lua/src"
        cp "$THIRD_PARTY/lua/src_repo/"*.c "$THIRD_PARTY/lua/src/"
        cp "$THIRD_PARTY/lua/src_repo/"*.h "$THIRD_PARTY/lua/src/"
        rm -rf "$THIRD_PARTY/lua/src_repo"
        cat > "$THIRD_PARTY/lua/CMakeLists.txt" << 'LUACMAKE'
cmake_minimum_required(VERSION 3.10)
project(lua LANGUAGES C)
set(LUA_SRC src/lapi.c src/lauxlib.c src/lbaselib.c src/lcode.c src/lcorolib.c src/lctype.c src/ldblib.c src/ldebug.c src/ldo.c src/ldump.c src/lfunc.c src/lgc.c src/linit.c src/liolib.c src/llex.c src/lmathlib.c src/lmem.c src/loadlib.c src/lobject.c src/lopcodes.c src/loslib.c src/lparser.c src/lstate.c src/lstring.c src/lstrlib.c src/ltable.c src/ltablib.c src/ltm.c src/lundump.c src/lutf8lib.c src/lvm.c src/lzio.c)
add_library(lua_static STATIC ${LUA_SRC})
target_include_directories(lua_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_compile_options(lua_static PRIVATE -w)
add_library(Lua::Lua INTERFACE IMPORTED GLOBAL)
set_target_properties(Lua::Lua PROPERTIES INTERFACE_LINK_LIBRARIES lua_static INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/src")
set(LUA_FOUND TRUE PARENT_SCOPE)
set(LUA_LIBRARIES lua_static PARENT_SCOPE)
set(LUA_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src" PARENT_SCOPE)
set(LUA_VERSION_STRING "5.4.7" PARENT_SCOPE)
LUACMAKE
    fi

    # SDL3 src
    if [ ! -d "$THIRD_PARTY/sdl3-src" ]; then git clone --depth 1 https://github.com/libsdl-org/SDL.git "$THIRD_PARTY/sdl3-src"; fi
    # SDL3_net src
    if [ ! -d "$THIRD_PARTY/sdl3_net-src" ]; then git clone --depth 1 https://github.com/libsdl-org/SDL_net.git "$THIRD_PARTY/sdl3_net-src"; fi
    # SDL3_image src
    if [ ! -d "$THIRD_PARTY/sdl3_image-src" ]; then git clone --depth 1 https://github.com/libsdl-org/SDL_image.git "$THIRD_PARTY/sdl3_image-src"; fi
    # SDL3_mixer src
    if [ ! -d "$THIRD_PARTY/sdl3_mixer-src" ]; then 
        git clone --depth 1 https://github.com/libsdl-org/SDL_mixer.git "$THIRD_PARTY/sdl3_mixer-src"
    fi
    MIXER_EXT="$THIRD_PARTY/sdl3_mixer-src/external"
    clone_ext() {
        if [ ! -f "$MIXER_EXT/$1/CMakeLists.txt" ]; then
            rm -rf "$MIXER_EXT/$1"
            git clone --depth 1 --branch "$3" "$2" "$MIXER_EXT/$1"
        fi
    }
    clone_ext ogg       https://github.com/libsdl-org/ogg.git            v1.3.5-SDL
    clone_ext vorbis    https://github.com/libsdl-org/vorbis.git         v1.3.7-SDL
    clone_ext flac      https://github.com/libsdl-org/flac.git           1.3.4-SDL
    clone_ext opus      https://github.com/libsdl-org/opus.git           v1.4.x-SDL
    clone_ext opusfile  https://github.com/libsdl-org/opusfile.git       v0.13-git-SDL
    clone_ext tremor    https://github.com/libsdl-org/tremor.git         v1.2.1-SDL
    clone_ext mpg123    https://github.com/libsdl-org/mpg123.git         v1.33.4-SDL
    clone_ext libxmp    https://github.com/libsdl-org/libxmp.git         4.7.0-SDL
    clone_ext wavpack   https://github.com/libsdl-org/wavpack.git        5.9.0-SDL
    clone_ext libgme    https://github.com/libsdl-org/game-music-emu.git v0.6.4-SDL

    # freetype src
    if [ ! -d "$THIRD_PARTY/freetype-src" ]; then git clone --depth 1 --branch VER-2-13-3 https://github.com/freetype/freetype.git "$THIRD_PARTY/freetype-src"; fi
    # minizip-ng src
    if [ ! -d "$THIRD_PARTY/minizip-ng-src" ]; then git clone --depth 1 --branch "4.1.0" https://github.com/zlib-ng/minizip-ng "$THIRD_PARTY/minizip-ng-src"; fi
    # tf-psa-crypto src
    if [ ! -d "$THIRD_PARTY/tf-psa-crypto-src" ]; then
        mkdir -p "$THIRD_PARTY/tf-psa-crypto-src"
        curl -L -o "$THIRD_PARTY/tf-psa-crypto.tar.bz2" "https://github.com/Mbed-TLS/TF-PSA-Crypto/releases/download/tf-psa-crypto-1.0.0/tf-psa-crypto-1.0.0.tar.bz2"
        tar xf "$THIRD_PARTY/tf-psa-crypto.tar.bz2" --no-same-owner -C "$THIRD_PARTY/tf-psa-crypto-src" --strip-components=1
        rm "$THIRD_PARTY/tf-psa-crypto.tar.bz2"
    fi

    # curl
    if [ ! -d "$THIRD_PARTY/curl" ]; then
        git clone --depth 1 --branch curl-8_7_1 https://github.com/curl/curl.git "$THIRD_PARTY/curl"
    fi
}

build_sdl3() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/sdl3/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building SDL3 for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/sdl3-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=ON -DSDL_STATIC=OFF -DSDL_TEST=OFF
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_sdl3_image() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/sdl3_image/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building SDL3_image for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/sdl3_image-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DSDL3_DIR="$THIRD_PARTY/sdl3/build/$abi/lib/cmake/SDL3" \
        -DCMAKE_PREFIX_PATH="$THIRD_PARTY/sdl3/build/$abi" \
        -DBUILD_SHARED_LIBS=ON -DSDLIMAGE_TESTS=OFF -DSDLIMAGE_SAMPLES=OFF
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_sdl3_mixer() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/sdl3_mixer/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building SDL3_mixer for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/sdl3_mixer-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DSDL3_DIR="$THIRD_PARTY/sdl3/build/$abi/lib/cmake/SDL3" \
        -DCMAKE_PREFIX_PATH="$THIRD_PARTY/sdl3/build/$abi" \
        -DBUILD_SHARED_LIBS=ON -DSDLMIXER_VENDORED=ON -DSDLMIXER_EXAMPLES=OFF -DSDLMIXER_TESTS=OFF -DSDLMIXER_MP3_MPG123=OFF -DSDLMIXER_GME=OFF
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_sdl3_net() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/sdl3_net/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building SDL3_net for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/sdl3_net-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DSDL3_DIR="$THIRD_PARTY/sdl3/build/$abi/lib/cmake/SDL3" \
        -DCMAKE_PREFIX_PATH="$THIRD_PARTY/sdl3/build/$abi" \
        -DBUILD_SHARED_LIBS=ON -DSDLNET_EXAMPLES=OFF -DSDLNET_TESTS=OFF
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_freetype() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/freetype/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building freeType for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/freetype-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DBUILD_SHARED_LIBS=OFF -DFT_DISABLE_ZLIB=ON -DFT_DISABLE_BZIP2=ON \
        -DFT_DISABLE_PNG=ON -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_minizip_ng() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/minizip-ng/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building minizip-ng for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/minizip-ng-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DMZ_COMPAT=OFF -DMZ_ZLIB_FLAVOR=zlib -DMZ_BZIP2=OFF -DMZ_LZMA=OFF -DMZ_PPMD=OFF -DMZ_ZSTD=OFF \
        -DMZ_LIBCOMP=OFF -DMZ_PKCRYPT=OFF -DMZ_WZAES=OFF -DMZ_OPENSSL=OFF -DMZ_LIBBSD=OFF -DMZ_DECOMPRESS_ONLY=ON
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

build_tf_psa_crypto() {
    local abi="$1"
    local BUILD_DIR="$THIRD_PARTY/tf-psa-crypto/build/$abi"
    if [ -d "$BUILD_DIR/lib" ]; then return; fi
    echo "Building tf-psa-crypto for $abi..."
    mkdir -p "$BUILD_DIR/tmp" && cd "$BUILD_DIR/tmp"
    cmake "$THIRD_PARTY/tf-psa-crypto-src" -G Ninja \
        -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
        -DANDROID_HOST_TAG="$HOST_TAG" \
        -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$ANDROID_API_LEVEL" \
        -DCMAKE_INSTALL_PREFIX="$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_C_FLAGS="-Wno-error=array-bounds" \
        -DENABLE_PROGRAMS=OFF -DENABLE_TESTING=OFF -DUSE_SHARED_TF_PSA_CRYPTO_LIBRARY=OFF -DUSE_STATIC_TF_PSA_CRYPTO_LIBRARY=ON
    cmake --build . -j"$JOBS" --target install
    cd "$ROOT_DIR"
}

fetch_source

echo "Building libraries..."
for abi in "${TARGET_ABIS[@]}"; do
    echo "[$abi]"
    build_sdl3 "$abi"
    build_sdl3_image "$abi"
    build_sdl3_mixer "$abi"
    build_sdl3_net "$abi"
    build_freetype "$abi"
    build_minizip_ng "$abi"
    build_tf_psa_crypto "$abi"
done

echo "Copying to jniLibs..."
for abi in "${TARGET_ABIS[@]}"; do
    mkdir -p $ROOT_DIR/app/src/main/jniLibs/${abi}
    cp $THIRD_PARTY/sdl3/build/${abi}/lib/*.so $ROOT_DIR/app/src/main/jniLibs/${abi}/ 2>/dev/null || true
    cp $THIRD_PARTY/sdl3_image/build/${abi}/lib/*.so $ROOT_DIR/app/src/main/jniLibs/${abi}/ 2>/dev/null || true
    cp $THIRD_PARTY/sdl3_mixer/build/${abi}/lib/*.so $ROOT_DIR/app/src/main/jniLibs/${abi}/ 2>/dev/null || true
    cp $THIRD_PARTY/sdl3_net/build/${abi}/lib/*.so $ROOT_DIR/app/src/main/jniLibs/${abi}/ 2>/dev/null || true
    
    case "$abi" in
        arm64-v8a)   target_triple="aarch64-linux-android" ;;
        armeabi-v7a) target_triple="arm-linux-androideabi" ;;
        x86_64)      target_triple="x86_64-linux-android" ;;
        x86)         target_triple="i686-linux-android" ;;
    esac
    libcxx_path="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/$HOST_TAG/sysroot/usr/lib/$target_triple/libc++_shared.so"
    cp "${libcxx_path}" $ROOT_DIR/app/src/main/jniLibs/${abi}/ 2>/dev/null || true
done
echo "Complete!"
