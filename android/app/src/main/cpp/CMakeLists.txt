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
set(SDL3_IMAGE_ROOT "${THIRD_PARTY_DIR}/sdl3_image/build/${ANDROID_ABI}")
set(SDL3_MIXER_ROOT "${THIRD_PARTY_DIR}/sdl3_mixer/build/${ANDROID_ABI}")
set(SDL3_NET_ROOT "${THIRD_PARTY_DIR}/sdl3_net/build/${ANDROID_ABI}")
set(FREETYPE_ROOT "${THIRD_PARTY_DIR}/freetype/build/${ANDROID_ABI}")
set(MINIZIP_NG_ROOT "${THIRD_PARTY_DIR}/minizip-ng/build/${ANDROID_ABI}")
set(TF_PSA_CRYPTO_ROOT "${THIRD_PARTY_DIR}/tf-psa-crypto/build/${ANDROID_ABI}")

# So that GekkoNet builds as a static lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# ======================================
# Feature toggles
# ======================================
set(ENABLE_NETPLAY ON)
set(ENABLE_RMLUI ON)
set(ENABLE_TRAINING ON)
set(ENABLE_BROADCAST OFF)
set(ENABLE_MODS ON)
set(ENABLE_BEZEL ON)
set(ENABLE_CONTROLLER_IMAGE ON)
set(ENABLE_REPLAY ON)
set(ENABLE_LIBRASHADER ON CACHE BOOL "" FORCE)
set(ENABLE_DEBUG_HUD ON)
set(ENABLE_LEADERBOARD ON)
set(ENABLE_ARCADE_ROM ON)

# Validate librashader availability for this ABI (must run before source filters)
if(ENABLE_LIBRASHADER)
    if(ANDROID_ABI STREQUAL "arm64-v8a")
        set(RUST_TARGET "aarch64-linux-android")
    elseif(ANDROID_ABI STREQUAL "armeabi-v7a")
        set(RUST_TARGET "armv7-linux-androideabi")
    elseif(ANDROID_ABI STREQUAL "x86_64")
        set(RUST_TARGET "x86_64-linux-android")
    elseif(ANDROID_ABI STREQUAL "x86")
        set(RUST_TARGET "i686-linux-android")
    endif()
    set(LIBRASHADER_LIB_PATH "${REPO_ROOT}/third_party/librashader/target/${RUST_TARGET}/release/liblibrashader_capi.a")
    if(NOT EXISTS "${LIBRASHADER_LIB_PATH}")
        message(WARNING "librashader: NOT found for ${ANDROID_ABI} — disabling (${LIBRASHADER_LIB_PATH})")
        set(ENABLE_LIBRASHADER OFF)
    endif()
endif()

# ======================================
# Add Subdirectories for Source Deps
# ======================================
add_subdirectory("${THIRD_PARTY_DIR}/GekkoNet" GekkoNet)
add_subdirectory("${THIRD_PARTY_DIR}/lua" lua)

# Curl (for Netplay Lobby Server)
if(ENABLE_NETPLAY)
    set(BUILD_CURL_EXE OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
    set(CURL_USE_OPENSSL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_MBEDTLS OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SCHANNEL OFF CACHE BOOL "" FORCE)
    set(CURL_USE_SECTRANSP OFF CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAP ON CACHE BOOL "" FORCE)
    set(CURL_DISABLE_LDAPS ON CACHE BOOL "" FORCE)
    set(CURL_STATICLIB ON CACHE BOOL "" FORCE)
    add_subdirectory("${THIRD_PARTY_DIR}/curl" curl)
endif()

if(NOT TARGET Freetype::Freetype)
    if(EXISTS "${FREETYPE_ROOT}/lib/libfreetype.a")
        add_library(Freetype::Freetype STATIC IMPORTED)
        set_target_properties(Freetype::Freetype PROPERTIES
            IMPORTED_LOCATION "${FREETYPE_ROOT}/lib/libfreetype.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_ROOT}/include/freetype2"
        )
    endif()
endif()

set(RMLUI_SDL_VERSION_MAJOR 3 CACHE STRING "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS ON CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)
add_subdirectory("${THIRD_PARTY_DIR}/rmlui" rmlui EXCLUDE_FROM_ALL)

# ======================================
# SDL_ShaderCross
# ======================================
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_DXC OFF CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_CLI OFF CACHE BOOL "" FORCE)
set(SDL3_DIR "${SDL3_ROOT}/lib/cmake/SDL3" CACHE PATH "" FORCE)
add_subdirectory("${THIRD_PARTY_DIR}/SDL_shadercross" SDL_shadercross EXCLUDE_FROM_ALL)

# ======================================
# Includes — point to shared repo src/
# ======================================
include_directories(
    ${SRC_DIR}
    ${SRC_DIR}/include
    ${SRC_DIR}/include/sdk
    ${SRC_DIR}/include/port/sdl/app
    ${SRC_DIR}/include/port/sdl/renderer
    ${SRC_DIR}/include/port/sdl/input
    ${SRC_DIR}/include/port/config
    ${SRC_DIR}/include/port/save
    ${SRC_DIR}/include/port/rendering
    ${REPO_ROOT}/zlib
    ${THIRD_PARTY_DIR}/simde
    ${THIRD_PARTY_DIR}/stb
    ${THIRD_PARTY_DIR}/cJSON
    ${THIRD_PARTY_DIR}/glad_gles_generated/include
    ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include
    ${THIRD_PARTY_DIR}/slang-shaders
    ${THIRD_PARTY_DIR}/rmlui/Include
    ${THIRD_PARTY_DIR}/rmlui/Backends
    ${SRC_DIR}/port/sdl/app
    ${SRC_DIR}/port/sdl/renderer
    ${SRC_DIR}/port/sdl/input
    ${SRC_DIR}/port/sdl/netplay
    ${SRC_DIR}/port/sdl/rmlui
    ${SRC_DIR}/port/config
    ${SRC_DIR}/port/save
    ${SRC_DIR}/port/mods
    ${SRC_DIR}/port/rendering
    ${SDL3_ROOT}/include
    ${SDL3_IMAGE_ROOT}/include
    ${SDL3_MIXER_ROOT}/include
    ${SDL3_NET_ROOT}/include
    ${MINIZIP_NG_ROOT}/include
    ${TF_PSA_CRYPTO_ROOT}/include
    ${REPO_ROOT}/third_party/librashader/include
)

# ======================================
# Source files — from shared repo src/
# ======================================
file(GLOB_RECURSE GAME_SRC CONFIGURE_DEPENDS ${SRC_DIR}/*.c ${SRC_DIR}/*.cpp)
file(GLOB_RECURSE ZLIB_SRC ${REPO_ROOT}/zlib/*.c)
file(GLOB_RECURSE GLAD_SRC "${THIRD_PARTY_DIR}/glad_gles_generated/src/*.c")
set_source_files_properties(${GLAD_SRC} PROPERTIES COMPILE_DEFINITIONS "GLAD_GLES2_USE_SYSTEM_EGL=1")

if(ENABLE_RMLUI)
    list(APPEND GAME_SRC
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp"
    )
    set_source_files_properties(
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp"
        "${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp"
        PROPERTIES
            COMPILE_OPTIONS "-Wno-error;-Wno-delete-abstract-non-virtual-dtor;-Wno-delete-non-abstract-non-virtual-dtor"
            COMPILE_DEFINITIONS "RMLUI_SDL_VERSION_MAJOR=3;RMLUI_GL3_CUSTOM_LOADER=<glad/gles2.h>"
    )
    set_source_files_properties(
        "${SRC_DIR}/port/sdl/rmlui/rmlui_wrapper.cpp"
        PROPERTIES
            COMPILE_OPTIONS "-Wno-error;-Wno-delete-abstract-non-virtual-dtor;-Wno-delete-non-abstract-non-virtual-dtor"
    )
endif()

list(FILTER GAME_SRC EXCLUDE REGEX "src/port/win32/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/linux/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/macos/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/ps3/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/broadcast")

if(NOT ENABLE_NETPLAY)
    list(FILTER GAME_SRC EXCLUDE REGEX "src/netplay/")
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/sdl/netplay/")
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/sdl/netstats_renderer")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_network_lobby")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_casual_lobby")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_tournament_lobby")
endif()

if(NOT ENABLE_RMLUI)
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/sdl/rmlui/")
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/menu_bridge")
endif()

if(NOT ENABLE_TRAINING)
    list(FILTER GAME_SRC EXCLUDE REGEX "lua_engine_bridge")
    list(FILTER GAME_SRC EXCLUDE REGEX "lua_trials_loader")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_training_")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_trials_")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_training")
endif()

if(NOT ENABLE_BROADCAST)
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/broadcast")
endif()

if(NOT ENABLE_MODS)
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/mods/")
    list(FILTER GAME_SRC EXCLUDE REGEX "modded_bgm")
    list(FILTER GAME_SRC EXCLUDE REGEX "modded_stage")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_mods_menu")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_stage_config")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_sysdir")
endif()

if(NOT ENABLE_LIBRASHADER)
    list(FILTER GAME_SRC EXCLUDE REGEX "src/shaders/librashader_.*")
    list(FILTER GAME_SRC EXCLUDE REGEX "src/port/sdl/renderer/sdl_game_renderer_gl_librashader.*")
endif()

if(NOT ENABLE_BEZEL)
    list(FILTER GAME_SRC EXCLUDE REGEX "sdl_app_bezel")
    list(FILTER GAME_SRC EXCLUDE REGEX "sdl_bezel")
endif()

if(NOT ENABLE_CONTROLLER_IMAGE)
    list(FILTER GAME_SRC EXCLUDE REGEX "controller_image")
endif()

if(NOT ENABLE_REPLAY)
    list(FILTER GAME_SRC EXCLUDE REGEX "replay_picker")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_replay")
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_save_replay")
endif()

if(NOT ENABLE_LIBRASHADER)
    list(FILTER GAME_SRC EXCLUDE REGEX "sdl_app_shader_config")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_shader_menu")
endif()

if(NOT ENABLE_DEBUG_HUD)
    list(FILTER GAME_SRC EXCLUDE REGEX "sdl_app_debug_hud")
endif()

if(NOT ENABLE_LEADERBOARD)
    list(FILTER GAME_SRC EXCLUDE REGEX "ms_leaderboard")
    list(FILTER GAME_SRC EXCLUDE REGEX "rmlui_leaderboard")
endif()

add_library(main SHARED 
    ${GAME_SRC} 
    ${ZLIB_SRC}
    ${GLAD_SRC}
    ${THIRD_PARTY_DIR}/cJSON/cJSON.c
)

# Subdirectory vendored flags
set_source_files_properties(${THIRD_PARTY_DIR}/cJSON/cJSON.c PROPERTIES COMPILE_OPTIONS "-Wno-error")
set_source_files_properties(${SRC_DIR}/port/sdl/input/controllerimage/controllerimage.c PROPERTIES COMPILE_OPTIONS "-Wno-error")
set_source_files_properties(
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp
    PROPERTIES COMPILE_OPTIONS "-Wno-error" COMPILE_DEFINITIONS "RMLUI_SDL_VERSION_MAJOR=3;RMLUI_GL3_CUSTOM_LOADER=<glad/gles2.h>"
)

# ======================================
# Flags and Defines
# ======================================
target_compile_definitions(main PRIVATE
    $<$<CONFIG:Debug>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
    TARGET_SDL3
    _POSIX_C_SOURCE=200809L
    $<$<BOOL:${ENABLE_NETPLAY}>:ENABLE_NETPLAY>
    $<$<BOOL:${ENABLE_RMLUI}>:ENABLE_RMLUI>
    $<$<BOOL:${ENABLE_TRAINING}>:ENABLE_TRAINING>
    $<$<BOOL:${ENABLE_BROADCAST}>:ENABLE_BROADCAST>
    $<$<BOOL:${ENABLE_MODS}>:ENABLE_MODS>
    $<$<BOOL:${ENABLE_BEZEL}>:ENABLE_BEZEL>
    $<$<BOOL:${ENABLE_CONTROLLER_IMAGE}>:ENABLE_CONTROLLER_IMAGE>
    $<$<BOOL:${ENABLE_REPLAY}>:ENABLE_REPLAY>
    $<$<BOOL:${ENABLE_DEBUG_HUD}>:ENABLE_DEBUG_HUD>
    $<$<BOOL:${ENABLE_LEADERBOARD}>:ENABLE_LEADERBOARD>
    $<$<BOOL:${ENABLE_ARCADE_ROM}>:ENABLE_ARCADE_ROM>
    $<$<BOOL:${ENABLE_LIBRASHADER}>:ENABLE_LIBRASHADER>
    $<$<BOOL:${ENABLE_RMLUI}>:RMLUI_SDL_VERSION_MAJOR=3>
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
)

set_target_properties(main PROPERTIES LINKER_LANGUAGE CXX)

# ======================================
# Prebuilt Libraries
# ======================================
add_library(SDL3::SDL3 SHARED IMPORTED)
set_target_properties(SDL3::SDL3 PROPERTIES IMPORTED_LOCATION "${SDL3_ROOT}/lib/libSDL3.so")

add_library(SDL3_image::SDL3_image SHARED IMPORTED)
set_target_properties(SDL3_image::SDL3_image PROPERTIES IMPORTED_LOCATION "${SDL3_IMAGE_ROOT}/lib/libSDL3_image.so")

add_library(SDL3_mixer::SDL3_mixer SHARED IMPORTED)
set_target_properties(SDL3_mixer::SDL3_mixer PROPERTIES IMPORTED_LOCATION "${SDL3_MIXER_ROOT}/lib/libSDL3_mixer.so")

add_library(SDL3_net::SDL3_net SHARED IMPORTED)
set_target_properties(SDL3_net::SDL3_net PROPERTIES IMPORTED_LOCATION "${SDL3_NET_ROOT}/lib/libSDL3_net.so")

add_library(minizip_ng::minizip_ng STATIC IMPORTED)
set_target_properties(minizip_ng::minizip_ng PROPERTIES IMPORTED_LOCATION "${MINIZIP_NG_ROOT}/lib/libminizip-ng.a")

add_library(tfpsacrypto::tfpsacrypto STATIC IMPORTED)
set_target_properties(tfpsacrypto::tfpsacrypto PROPERTIES IMPORTED_LOCATION "${TF_PSA_CRYPTO_ROOT}/lib/libtfpsacrypto.a")

# ======================================
# Librashader (static Rust library)
# ======================================
if(ENABLE_LIBRASHADER)
    add_library(librashader_capi STATIC IMPORTED)
    set_target_properties(librashader_capi PROPERTIES
        IMPORTED_LOCATION "${LIBRASHADER_LIB_PATH}"
    )
    message(STATUS "librashader: enabled for ${ANDROID_ABI}")
endif()

# ======================================
# Link against libraries
# ======================================
target_link_libraries(main PRIVATE
    SDL3::SDL3
    SDL3_image::SDL3_image
    SDL3_mixer::SDL3_mixer
    SDL3_net::SDL3_net
    GekkoNet
    $<$<BOOL:${ENABLE_NETPLAY}>:libcurl>
    minizip_ng::minizip_ng
    tfpsacrypto::tfpsacrypto
    rmlui
    rmlui_core
    rmlui_lua
    rmlui_debugger
    lua_static
    SDL3_shadercross::SDL3_shadercross
    $<$<BOOL:${ENABLE_LIBRASHADER}>:librashader_capi>
    log
    android
    EGL
    GLESv3
)
