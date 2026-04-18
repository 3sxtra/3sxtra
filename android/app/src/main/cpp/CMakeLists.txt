cmake_minimum_required(VERSION 3.22.1)

project("3sx")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

if(NOT ANDROID_ABI)
    set(ANDROID_ABI "arm64-v8a")
endif()

# ======================================
# Paths — shared source tree at repo root
# ======================================
set(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../..")
set(SRC_DIR "${REPO_ROOT}/src")
set(THIRD_PARTY_DIR "${REPO_ROOT}/third_party_android")
set(SDL3_ROOT "${THIRD_PARTY_DIR}/sdl3/build/${ANDROID_ABI}")
set(SDL3_NET_ROOT "${THIRD_PARTY_DIR}/SDL_net/build/${ANDROID_ABI}")
set(FFMPEG_ROOT "${THIRD_PARTY_DIR}/ffmpeg/build/${ANDROID_ABI}")
set(MINIZIP_NG_ROOT "${THIRD_PARTY_DIR}/minizip-ng/build/${ANDROID_ABI}")
set(TF_PSA_CRYPTO_ROOT "${THIRD_PARTY_DIR}/tf-psa-crypto/build/${ANDROID_ABI}")

# So that GekkoNet builds as a static lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ======================================
# Include directories
# ======================================
include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/sdk
    ${SDL3_ROOT}/include
    ${FFMPEG_ROOT}/include
    ${THIRD_PARTY_DIR}/GekkoNet/include
    ${SDL3_NET_ROOT}/include
    ${MINIZIP_NG_ROOT}/include
    ${TF_PSA_CRYPTO_ROOT}/include
)

# ======================================
# Add Subdirectories for Source Deps
# ======================================
add_subdirectory("${THIRD_PARTY_DIR}/GekkoNet" GekkoNet)

# ======================================
# Source files
# ======================================
file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS ${SRC_DIR}/*.c)

# Exclude platform-specific code not for Android
list(FILTER GAME_SRC EXCLUDE REGEX "src/platform/app/psp/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/platform/video/psp/")

# Exclude imgui (debug-only, uses OpenGL3 backend not compatible with GLES without adaptation)
list(FILTER GAME_SRC EXCLUDE REGEX "src/imgui/")

add_library(main SHARED
    ${GAME_SRC}
)

# ======================================
# Flags and Defines
# ======================================
target_compile_definitions(main PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>

    # GekkoNet
    GEKKONET_STATIC
    GEKKONET_NO_ASIO

    CRS_APP_DRIVER_SDL
    CRS_VIDEO_DRIVER_SDL

    ARCADE_ROM
    SOUND_ENABLED
    MEMCARD_DISABLED
)

target_compile_options(main PRIVATE
    -Wall
    -fno-strict-aliasing
    -Wno-pointer-to-int-cast
    -Wno-deprecated-non-prototype
    -Wno-tautological-overlap-compare
    -Wno-c2x-extensions
    -Wno-incompatible-pointer-types
    -Wno-pointer-sign
    -Wno-unused-variable
)

set_target_properties(main PROPERTIES LINKER_LANGUAGE C)

# ======================================
# Prebuilt Libraries
# ======================================
add_library(SDL3::SDL3 SHARED IMPORTED)
set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_LOCATION "${SDL3_ROOT}/lib/libSDL3.so")

add_library(SDL3_net::SDL3_net SHARED IMPORTED)
set_target_properties(SDL3_net::SDL3_net PROPERTIES IMPORTED_LOCATION "${SDL3_NET_ROOT}/lib/libSDL3_net.so")

add_library(minizip_ng::minizip_ng STATIC IMPORTED)
set_target_properties(minizip_ng::minizip_ng PROPERTIES IMPORTED_LOCATION "${MINIZIP_NG_ROOT}/lib/libminizip.a")

add_library(ppmd::ppmd STATIC IMPORTED)
set_target_properties(ppmd::ppmd PROPERTIES IMPORTED_LOCATION "${MINIZIP_NG_ROOT}/lib/libppmd.a")

add_library(tfpsacrypto::tfpsacrypto STATIC IMPORTED)
set_target_properties(tfpsacrypto::tfpsacrypto PROPERTIES IMPORTED_LOCATION "${TF_PSA_CRYPTO_ROOT}/lib/libtfpsacrypto.a")

add_library(avcodec::avcodec SHARED IMPORTED)
set_target_properties(avcodec::avcodec PROPERTIES IMPORTED_LOCATION "${FFMPEG_ROOT}/lib/libavcodec.so")

add_library(avutil::avutil SHARED IMPORTED)
set_target_properties(avutil::avutil PROPERTIES IMPORTED_LOCATION "${FFMPEG_ROOT}/lib/libavutil.so")

add_library(swresample::swresample SHARED IMPORTED)
set_target_properties(swresample::swresample PROPERTIES IMPORTED_LOCATION "${FFMPEG_ROOT}/lib/libswresample.so")

# ======================================
# Link against libraries
# ======================================
target_link_libraries(main PRIVATE
    SDL3::SDL3
    GekkoNet
    SDL3_net::SDL3_net
    minizip_ng::minizip_ng
    ppmd::ppmd
    tfpsacrypto::tfpsacrypto
    avcodec::avcodec
    avutil::avutil
    swresample::swresample
    log
    android
    EGL
    GLESv3
    z
)
