cmake_minimum_required(VERSION 3.24)
project(3sx LANGUAGES C CXX)

# ======================================
# Options and platform setup
# ======================================

option(PLATFORM_RPI4 "Build for Raspberry Pi 4 / Batocera (aarch64)" OFF)
option(ENABLE_TRACY "Enable Tracy profiler instrumentation" OFF)

# --- Compile-time feature toggles (all ON by default) ---
option(ENABLE_NETPLAY          "Build with netplay/rollback support"              ON)
option(ENABLE_RMLUI            "Build with RmlUi HTML/CSS menu system"            ON)
option(ENABLE_TRAINING         "Build with Lua training mode & trials"            ON)
option(ENABLE_BROADCAST        "Build with video broadcast (Spout/Syphon)"        ON)
option(ENABLE_MODS             "Build with stage/BGM mod loading"                 ON)
option(ENABLE_BEZEL            "Build with bezel overlay rendering"               ON)
option(ENABLE_CONTROLLER_IMAGE "Build with controller image overlays"             ON)
option(ENABLE_REPLAY           "Build with replay recording & picker"             ON)
option(ENABLE_LIBRASHADER      "Build with librashader CRT/shader filters"        ON)
option(ENABLE_DEBUG_HUD        "Build with debug HUD overlay"                     ON)
option(ENABLE_LEADERBOARD      "Build with online leaderboard"                    ON)
option(ENABLE_ARCADE_ROM       "Build with arcade ROM support (minizip-ng)"       ON)

# Set third-party directory based on platform
# RPi4 uses a separate directory to avoid conflicts with Windows-built files.
# Any platform can override via -DTHIRD_PARTY_DIR=<path> (e.g. third_party_linux).
if(THIRD_PARTY_DIR)
    message(STATUS "THIRD_PARTY_DIR overridden: ${THIRD_PARTY_DIR}")
elseif(PLATFORM_RPI4)
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party_rpi4")
    message(STATUS "PLATFORM_RPI4: ON — targeting GL 3.3 Core, excluding broadcast")
    message(STATUS "Using third_party_rpi4/ for dependencies")
else()
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
endif()

# Execute the dependency build script (desktop builds only)
# When SDL3_ROOT is pre-set (e.g. flatpak modules), skip SDL3/SDL3_image builds
if(NOT PLATFORM_RPI4)
    if(SDL3_ROOT)
        set(ENV{SKIP_SDL3_BUILD} "1")
    endif()
    execute_process(COMMAND bash build-deps.sh
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMAND_ERROR_IS_FATAL ANY)
endif()

if(NOT SDL3_ROOT)
    set(SDL3_ROOT "${THIRD_PARTY_DIR}/sdl3/build")
endif()

# So that GekkoNet builds as a static lib
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ⚡ Bolt: LTO — cross-TU inlining of hot functions.
# Use -DENABLE_LTO=ON to force LTO on any build type (e.g. Debug).
# Auto-enabled for Release builds when not explicitly set.
option(ENABLE_LTO "Force LTO on (any build type)" OFF)
include(CheckIPOSupported)
check_ipo_supported(RESULT LTO_SUPPORTED)
if(LTO_SUPPORTED)
    if(ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE)
        message(STATUS "LTO: Force-enabled for all build types")
    else()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        message(STATUS "LTO: Enabled for Release builds")
    endif()
else()
    message(STATUS "LTO: Not supported by this compiler")
endif()

# ⚡ Bolt: Profile-Guided Optimization (PGO) — opt-in only.
# Usage: cmake -DENABLE_PGO=GENERATE ..., run the game, then cmake -DENABLE_PGO=USE ...
set(ENABLE_PGO "OFF" CACHE STRING "PGO mode: OFF, GENERATE, or USE")

include(FetchContent)

if(ENABLE_NETPLAY)
    message(STATUS "DEBUG: Adding GekkoNet subdirectory...")
    add_subdirectory("${THIRD_PARTY_DIR}/GekkoNet" GekkoNet)
    message(STATUS "DEBUG: GekkoNet added.")
endif()

# SDL_ShaderCross — pre-cloned by build-deps.sh into third_party/
set(SDLSHADERCROSS_VENDORED ON CACHE BOOL "" FORCE)
set(SDLSHADERCROSS_DXC OFF CACHE BOOL "" FORCE)
set(SDL3_DIR "${SDL3_ROOT}/lib/cmake/SDL3" CACHE PATH "" FORCE)

add_subdirectory("${THIRD_PARTY_DIR}/SDL_shadercross" SDL_shadercross)

# Tracy Profiler (opt-in)
if(ENABLE_TRACY)
    message(STATUS "Tracy profiler: ENABLED")
    FetchContent_Declare(
        tracy
        GIT_REPOSITORY https://github.com/wolfpld/tracy.git
        GIT_TAG        v0.13.1
        GIT_SHALLOW    TRUE
    )
    FetchContent_MakeAvailable(tracy)
endif()

# cJSON — vendored single-file JSON library for lobby_server HTTP client
# Compiled directly as a source file to avoid CMake reconfigure issues
if(ENABLE_NETPLAY OR ENABLE_LEADERBOARD)
    include_directories("${THIRD_PARTY_DIR}/cJSON")
    set_source_files_properties("${THIRD_PARTY_DIR}/cJSON/cJSON.c" PROPERTIES COMPILE_OPTIONS "-Wno-error")
endif()

# Lua 5.4 (only needed for RmlUi Lua bindings)
if(ENABLE_RMLUI)
# Lua 5.4 — vendored static library for RmlUi Lua bindings
# Must be added BEFORE RmlUi so the Lua::Lua target exists when
# RmlUi's Dependencies.cmake runs.
message(STATUS "Configuring Lua 5.4...")
add_subdirectory("${THIRD_PARTY_DIR}/lua" lua)
message(STATUS "Lua 5.4 configured.")

# RmlUi — HTML/CSS UI library (always built alongside ImGui)
# Pre-cloned by build-deps.sh into third_party/rmlui
message(STATUS "Configuring RmlUi...")
set(RMLUI_SDL_VERSION_MAJOR 3 CACHE STRING "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS ON CACHE BOOL "" FORCE)
set(RMLUI_SVG_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_LOTTIE_PLUGIN OFF CACHE BOOL "" FORCE)
set(RMLUI_WARNINGS_AS_ERRORS OFF CACHE BOOL "" FORCE)


# FreeType — built by build-deps.sh into third_party/freetype/build
# We create the imported target directly so that RmlUi's find_package(Freetype)
# finds it immediately. This avoids issues with cross-compile toolchains that
# restrict FindFreetype's search paths (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY).
if(NOT TARGET Freetype::Freetype)
    set(FREETYPE_BUILD_DIR "${THIRD_PARTY_DIR}/freetype/build")
    if(EXISTS "${FREETYPE_BUILD_DIR}/lib/libfreetype.a")
        add_library(Freetype::Freetype STATIC IMPORTED)
        set_target_properties(Freetype::Freetype PROPERTIES
            IMPORTED_LOCATION "${FREETYPE_BUILD_DIR}/lib/libfreetype.a"
            INTERFACE_INCLUDE_DIRECTORIES "${FREETYPE_BUILD_DIR}/include/freetype2"
        )
        message(STATUS "FreeType: using bundled static lib at ${FREETYPE_BUILD_DIR}")
    elseif(PLATFORM_RPI4 AND EXISTS "${THIRD_PARTY_DIR}/freetype/CMakeLists.txt")
        add_subdirectory("${THIRD_PARTY_DIR}/freetype" freetype)
        message(STATUS "FreeType: using RPi4 subdirectory")
    else()
        message(STATUS "FreeType: using system package")
    endif()
endif()

add_subdirectory("${THIRD_PARTY_DIR}/rmlui" rmlui)
message(STATUS "RmlUi configured.")
endif() # ENABLE_RMLUI (Lua + RmlUi)

# ======================================
# Static Analysis (clang-tidy)
# ======================================

# Temporarily disabled to unblock build
# find_program(CLANG_TIDY_EXE NAMES clang-tidy)
# if(CLANG_TIDY_EXE)
#     message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
#     # Use extra-arg to silence clang-tidy errors about GCC-specific warning flags
#     set(CMAKE_C_CLANG_TIDY "${CLANG_TIDY_EXE}" "-extra-arg=-Wno-unknown-warning-option")
#     set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY_EXE}" "-extra-arg=-Wno-unknown-warning-option")
# else()
#     message(STATUS "clang-tidy not found")
# endif()



# ======================================
# Include directories
# ======================================

include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PROJECT_SOURCE_DIR}/src/include
    ${PROJECT_SOURCE_DIR}/src/include/sdk
    $<$<BOOL:${ENABLE_NETPLAY}>:${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include>
    ${THIRD_PARTY_DIR}/stb
    $<$<BOOL:${ENABLE_CONTROLLER_IMAGE}>:${THIRD_PARTY_DIR}/controllerimage/src>

    $<$<BOOL:${ENABLE_LIBRASHADER}>:${THIRD_PARTY_DIR}/librashader/include>
    ${THIRD_PARTY_DIR}/simde
    ${glad_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/gladsources/glad/include
    ${THIRD_PARTY_DIR}/sdl3/SDL/src/video/khronos

    # Subdirectories under src/port/sdl/ (bare-filename includes)
    ${PROJECT_SOURCE_DIR}/src/port/sdl/app
    ${PROJECT_SOURCE_DIR}/src/port/sdl/renderer
    ${PROJECT_SOURCE_DIR}/src/port/sdl/input
    ${PROJECT_SOURCE_DIR}/src/port/sdl/netplay
    ${PROJECT_SOURCE_DIR}/src/port/sdl/rmlui
    # Subdirectories under src/port/ (bare-filename includes)
    ${PROJECT_SOURCE_DIR}/src/port/config
    ${PROJECT_SOURCE_DIR}/src/port/save
    ${PROJECT_SOURCE_DIR}/src/port/mods
    ${PROJECT_SOURCE_DIR}/src/port/rendering
    # Mirrored subdirectories under src/include/port/ (public headers)
    ${PROJECT_SOURCE_DIR}/src/include/port/sdl/app
    ${PROJECT_SOURCE_DIR}/src/include/port/sdl/renderer
    ${PROJECT_SOURCE_DIR}/src/include/port/sdl/input
    ${PROJECT_SOURCE_DIR}/src/include/port/config
    ${PROJECT_SOURCE_DIR}/src/include/port/save
    ${PROJECT_SOURCE_DIR}/src/include/port/rendering
)

set(SPOUT2_DIR "${THIRD_PARTY_DIR}/Spout2")

if(ENABLE_ARCADE_ROM)
    set(MINIZIP_NG_ROOT "${THIRD_PARTY_DIR}/minizip-ng/build")
    set(TF_PSA_CRYPTO_ROOT "${THIRD_PARTY_DIR}/tf-psa-crypto/build")
    
    # minizip-ng provides a CMake config file
    # On Debian/Ubuntu arm64, GNUInstallDirs puts it in lib/aarch64-linux-gnu/cmake/minizip-ng
    file(GLOB MINIZIP_NG_CONFIG_DIRS
        "${MINIZIP_NG_ROOT}/lib/cmake/minizip-ng"
        "${MINIZIP_NG_ROOT}/lib64/cmake/minizip-ng"
        "${MINIZIP_NG_ROOT}/lib/*/cmake/minizip-ng"
    )
    if(MINIZIP_NG_CONFIG_DIRS)
        list(GET MINIZIP_NG_CONFIG_DIRS 0 MINIZIP_NG_EXACT_DIR)
        set(minizip-ng_DIR "${MINIZIP_NG_EXACT_DIR}")
    endif()
    find_package(minizip-ng REQUIRED)
    
    # tf-psa-crypto is a vendored static library built by build-deps.sh.
    # NO_CMAKE_FIND_ROOT_PATH is required because cross-compile toolchains set
    # CMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY, which silently ignores HINTS
    # that fall outside the sysroot (our third_party_arm64/ directory).
    find_library(TF_PSA_CRYPTO_LIB tfpsacrypto
        HINTS "${TF_PSA_CRYPTO_ROOT}/lib" "${TF_PSA_CRYPTO_ROOT}/lib64"
        NO_DEFAULT_PATH
        NO_CMAKE_FIND_ROOT_PATH
    )

    include_directories(
        ${MINIZIP_NG_ROOT}/include
        ${TF_PSA_CRYPTO_ROOT}/include
    )
endif()

if(WIN32)
    include_directories(${SPOUT2_DIR}/SPOUTSDK/SpoutGL)
endif()

# ======================================
# Source collection
# ======================================

file(GLOB_RECURSE GAME_SRC src/*.c src/*.cpp)

# Exclude platform-specific broadcast backends (added conditionally below)
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/win32/")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/linux/")

# --- Feature-toggle conditional source exclusions ---
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

find_package(ZLIB REQUIRED)

# Disable clang-tidy for all third-party sources
set_source_files_properties(src/port/win32/broadcast_spout.cpp
    PROPERTIES
        SKIP_LINTING ON
        C_CLANG_TIDY ""
        CXX_CLANG_TIDY ""
)

# ControllerImage library: suppress warnings for vendored source
set_source_files_properties(
    ${THIRD_PARTY_DIR}/controllerimage/src/controllerimage.c
    PROPERTIES
        COMPILE_OPTIONS "-Wno-error"
        SKIP_LINTING ON
        C_CLANG_TIDY ""
)

# RmlUi backend sources: suppress warnings and set SDL3 version
set_source_files_properties(
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Wno-error"
        COMPILE_DEFINITIONS "RMLUI_SDL_VERSION_MAJOR=3"
        SKIP_LINTING ON
        CXX_CLANG_TIDY ""
)
# SDL_GPU backend header has abstract Command class with non-virtual dtor —
# suppress the warning for files that include it (wrapper + backend itself)
set_source_files_properties(
    src/port/sdl/rmlui/rmlui_wrapper.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Wno-error;-Wno-delete-abstract-non-virtual-dtor;-Wno-delete-non-abstract-non-virtual-dtor"
)
# Use our glad via RMLUI_GL3_CUSTOM_LOADER instead of RmlUi's bundled glad
# ⚡ On Pi4, disable MSAA (wasteful at 384×224 on tile-based V3D GPU)
if(PLATFORM_RPI4)
    set_source_files_properties(
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
        PROPERTIES
            COMPILE_DEFINITIONS "RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>;RMLUI_SDL_VERSION_MAJOR=3;RMLUI_NUM_MSAA_SAMPLES=0"
    )
else()
    set_source_files_properties(
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
        PROPERTIES
            COMPILE_DEFINITIONS "RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>;RMLUI_SDL_VERSION_MAJOR=3"
    )
endif()

if(WIN32)
    set_source_files_properties(src/port/win32/broadcast_spout.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
endif()

# Broadcast: broadcast.c has a null backend fallback when no platform backend is linked.
# On RPi4, broadcast_pipewire.cpp is excluded (below), so broadcast.c uses the null backend.

add_executable(3sx MACOSX_BUNDLE
    ${GAME_SRC}
)

# --- Conditional third-party sources ---
if(ENABLE_CONTROLLER_IMAGE)
    target_sources(3sx PRIVATE ${THIRD_PARTY_DIR}/controllerimage/src/controllerimage.c)
endif()

if(ENABLE_NETPLAY OR ENABLE_LEADERBOARD)
    target_sources(3sx PRIVATE ${THIRD_PARTY_DIR}/cJSON/cJSON.c)
endif()

if(ENABLE_RMLUI)
    target_sources(3sx PRIVATE
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp
        ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp
    )
endif()


target_include_directories(3sx PRIVATE
    ${THIRD_PARTY_DIR}/rmlui/Backends
)

set_target_properties(3sx PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(3sx PRIVATE glad_gl_core)

# ======================================
# Compiler and linker flags
# ======================================

target_compile_definitions(3sx PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG>
    $<$<CONFIG:Release>:RELEASE>
    $<$<CONFIG:Release>:CHECKSUM>
    TARGET_SDL3
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>
    $<$<BOOL:${PLATFORM_RPI4}>:PLATFORM_RPI4>
    $<$<BOOL:${ENABLE_TRACY}>:TRACY_ENABLE>
    # Feature toggles
    $<$<BOOL:${ENABLE_NETPLAY}>:ENABLE_NETPLAY>
    $<$<BOOL:${ENABLE_RMLUI}>:ENABLE_RMLUI>
    $<$<BOOL:${ENABLE_TRAINING}>:ENABLE_TRAINING>
    $<$<BOOL:${ENABLE_BROADCAST}>:ENABLE_BROADCAST>
    $<$<BOOL:${ENABLE_MODS}>:ENABLE_MODS>
    $<$<BOOL:${ENABLE_BEZEL}>:ENABLE_BEZEL>
    $<$<BOOL:${ENABLE_CONTROLLER_IMAGE}>:ENABLE_CONTROLLER_IMAGE>
    $<$<BOOL:${ENABLE_REPLAY}>:ENABLE_REPLAY>
    $<$<BOOL:${ENABLE_LIBRASHADER}>:ENABLE_LIBRASHADER>
    $<$<BOOL:${ENABLE_DEBUG_HUD}>:ENABLE_DEBUG_HUD>
    $<$<BOOL:${ENABLE_LEADERBOARD}>:ENABLE_LEADERBOARD>
    $<$<BOOL:${ENABLE_RMLUI}>:RMLUI_SDL_VERSION_MAJOR=3>
)


if(CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    set(C_DISABLED_WARNINGS
        -Wpointer-to-int-cast
        -Wno-deprecated-non-prototype
        -Wno-tautological-overlap-compare
        -Wno-c2x-extensions
        -Wno-incompatible-pointer-types
        -Wno-pointer-sign
        -Wno-absolute-value
        -Wno-implicit-function-declaration
        -Wno-unused-parameter
        -Wno-unused-variable
    )
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    set(C_DISABLED_WARNINGS
        -Wno-unused-but-set-variable
        -Wno-pointer-sign
        -Wno-parentheses
        -Wno-incompatible-pointer-types
        -Wno-discarded-qualifiers
        -Wno-format-overflow
        -Wno-comment
        -Wno-maybe-uninitialized
        -Wno-array-bounds
        -Wno-aggressive-loop-optimizations
        -Wno-stringop-overflow
        -Wno-restrict
        -Wno-stringop-truncation
        -Wno-unused-result
        -Wno-format-truncation
        -Wno-unused-parameter
        -Wno-unused-variable
    )
endif()

target_compile_options(3sx PRIVATE
    -Wall
    -Werror
    -fno-strict-aliasing
    $<$<COMPILE_LANGUAGE:C>:${C_DISABLED_WARNINGS}>
)

# ⚡ Bolt: Platform-specific SIMD flags.
if(PLATFORM_RPI4)
    # ARM Cortex-A72 (RPi4): NEON is baseline, add crypto+CRC extensions.
    # -O3 overrides RelWithDebInfo's -O2 for max throughput on the constrained Pi4.
    target_compile_options(3sx PRIVATE -O3 -mcpu=cortex-a72 -mtune=cortex-a72)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "aarch64|arm64|ARM64")
    # Generic ARM64: NEON is baseline, no specific CPU tuning.
    message(STATUS "SIMD: ARM64 target — NEON is implicit, no extra flags needed")
else()
        # x86-64: SSE4.2 � supported on Intel Nehalem (2008+) / AMD Bulldozer (2011+).
    # SIMDE maps intrinsics portably; compiler auto-vectorizes with 128-bit SSE.
    target_compile_options(3sx PRIVATE -msse4.2)
endif()


# ⚡ Bolt: PGO compile/link flags — only active when ENABLE_PGO is set.
if(ENABLE_PGO STREQUAL "GENERATE")
    message(STATUS "PGO: GENERATE mode — profiling instrumentation enabled")
    target_compile_options(3sx PRIVATE -fprofile-generate)
    target_link_options(3sx PRIVATE -fprofile-generate)
elseif(ENABLE_PGO STREQUAL "USE")
    message(STATUS "PGO: USE mode — optimizing with collected profile data")
    target_compile_options(3sx PRIVATE -fprofile-use -fprofile-correction)
    target_link_options(3sx PRIVATE -fprofile-use)
endif()

# Determine librashader binary path based on target platform
if(ENABLE_LIBRASHADER)
    if(PLATFORM_RPI4)
        set(LIBRASHADER_LIB ${THIRD_PARTY_DIR}/librashader/target/aarch64-unknown-linux-gnu/release/liblibrashader_capi.a)
    else()
        set(LIBRASHADER_LIB ${THIRD_PARTY_DIR}/librashader/target/release/liblibrashader_capi.a)
    endif()
endif()

target_link_libraries(3sx PRIVATE
    m
    $<$<BOOL:${ENABLE_ARCADE_ROM}>:MINIZIP::minizip-ng>
    $<$<BOOL:${ENABLE_ARCADE_ROM}>:${TF_PSA_CRYPTO_LIB}>
    $<$<BOOL:${ENABLE_NETPLAY}>:GekkoNet>
    stdc++
    glad_gl_core
    ZLIB::ZLIB
    $<$<BOOL:${ENABLE_LIBRASHADER}>:${LIBRASHADER_LIB}>
    SDL3_shadercross::SDL3_shadercross
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<PLATFORM_ID:Darwin>>:rmlui>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<PLATFORM_ID:Darwin>>:rmlui_core>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<PLATFORM_ID:Darwin>>:rmlui_lua>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<PLATFORM_ID:Darwin>>:rmlui_debugger>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--start-group>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<NOT:$<PLATFORM_ID:Darwin>>>:rmlui>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<NOT:$<PLATFORM_ID:Darwin>>>:rmlui_core>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<NOT:$<PLATFORM_ID:Darwin>>>:rmlui_lua>
    $<$<AND:$<BOOL:${ENABLE_RMLUI}>,$<NOT:$<PLATFORM_ID:Darwin>>>:rmlui_debugger>
    $<$<NOT:$<PLATFORM_ID:Darwin>>:-Wl,--end-group>
    $<$<BOOL:${ENABLE_RMLUI}>:lua_static>
)

if(ENABLE_TRACY)
    target_link_libraries(3sx PRIVATE TracyClient)
endif()

if(UNIX)
    target_link_libraries(3sx PRIVATE dl pthread curl)



    # Homebrew on macOS ARM installs to /opt/homebrew
    if(APPLE)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
        include_directories(/opt/homebrew/include)
        link_directories(/opt/homebrew/lib)
    endif()

    if(ENABLE_NETPLAY)
        find_library(MINIUPNPC_LIB miniupnpc HINTS /opt/homebrew/lib /usr/local/lib)
        if(MINIUPNPC_LIB)
            target_link_libraries(3sx PRIVATE ${MINIUPNPC_LIB})
            target_compile_definitions(3sx PRIVATE HAVE_UPNP)
            message(STATUS "UPnP support: enabled (miniupnpc found)")
        else()
            message(STATUS "UPnP support: disabled (miniupnpc not found)")
        endif()
    endif()
endif()

if(WIN32)
    set(ICON_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    set(ICON_FILE_PATH "${CMAKE_SOURCE_DIR}/assets/3sxlogo.ico")
    string(REPLACE "/" "\\\\" ICON_FILE_PATH_WIN "${ICON_FILE_PATH}")
    file(WRITE ${ICON_RC_FILE} "1 ICON \"${ICON_FILE_PATH_WIN}\"")
    target_sources(3sx PRIVATE ${ICON_RC_FILE})
    if(ENABLE_BROADCAST)
        target_sources(3sx PRIVATE src/port/win32/broadcast_spout.cpp)
    endif()

    target_link_libraries(3sx PRIVATE
        userenv
        ntdll
        ws2_32
        advapi32
        bcrypt
        $<$<BOOL:${ENABLE_NETPLAY}>:miniupnpc>
        $<$<BOOL:${ENABLE_NETPLAY}>:iphlpapi>
        curl
    )
    if(ENABLE_NETPLAY)
        target_compile_definitions(3sx PRIVATE HAVE_UPNP)
    endif()
endif()


set_target_properties(3sx PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_SOURCE_DIR}/cmake/Info.plist"
)

if(APPLE)
    set_target_properties(3sx PROPERTIES
        INSTALL_RPATH "@executable_path/../Frameworks"
    )
    target_link_libraries(3sx PRIVATE "-framework OpenGL")

    # Syphon framework is optional (not available on CI runners)
    if(ENABLE_BROADCAST)
    find_library(SYPHON_FRAMEWORK Syphon)
    if(SYPHON_FRAMEWORK)
        target_sources(3sx PRIVATE src/port/macos/broadcast_syphon.mm)
        target_link_libraries(3sx PRIVATE "-framework Syphon")
        target_compile_definitions(3sx PRIVATE HAVE_SYPHON)
        message(STATUS "Syphon broadcast: enabled")
    else()
        message(STATUS "Syphon broadcast: disabled (framework not found)")
    endif()
    endif() # ENABLE_BROADCAST
elseif(UNIX AND NOT APPLE)
    set_target_properties(3sx PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib"
    )
    if(ENABLE_BROADCAST AND NOT PLATFORM_RPI4)
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)
        target_include_directories(3sx PRIVATE ${PIPEWIRE_INCLUDE_DIRS})
        target_link_options(3sx PRIVATE ${PIPEWIRE_LDFLAGS})
        target_link_libraries(3sx PRIVATE ${PIPEWIRE_LIBRARIES})
        
        target_sources(3sx PRIVATE src/port/linux/broadcast_pipewire.cpp)
    endif()
endif()

# ======================================
# Dependencies
# ======================================

# message(STATUS "DEBUG: Fetching glad...")
# include(FetchContent)
# FetchContent_Declare(
#     glad
#     GIT_REPOSITORY https://github.com/Dav1dde/glad.git
#     GIT_TAG v2.0.8
#     SOURCE_SUBDIR cmake
# )
# FetchContent_MakeAvailable(glad)
# message(STATUS "DEBUG: glad fetched.")

# Use local glad cloned by build-deps.sh
add_subdirectory("${THIRD_PARTY_DIR}/glad/cmake" glad_cmake)

if(PLATFORM_RPI4)
    glad_add_library(glad_gl_core REPRODUCIBLE LOADER API gl:core=3.3)
else()
    glad_add_library(glad_gl_core REPRODUCIBLE LOADER API gl:core=4.6)
endif()

# Spout2 for Windows video broadcast
if(ENABLE_BROADCAST AND WIN32)
    set(SPOUT_BUILD_SPOUTDX OFF CACHE BOOL "" FORCE)
    set(SPOUT_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SPOUT_BUILD_CMT OFF CACHE BOOL "" FORCE)
    add_subdirectory(${SPOUT2_DIR})
    target_compile_options(Spout_static PRIVATE -Wno-error)
    target_compile_options(Spout PRIVATE -Wno-error)
    set_target_properties(Spout_static PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
    set_target_properties(Spout PROPERTIES INTERPROCEDURAL_OPTIMIZATION OFF)
endif()

# add_subdirectory("${glad_SOURCE_DIR}/cmake" glad_cmake)

# THIRD_PARTY_DIR is already set above based on PLATFORM_RPI4
# SDL3_ROOT is already set above

# SDL3_net (only needed for netplay)
if(ENABLE_NETPLAY)
# SDL3_net — follow the same SDL3_ROOT fallback pattern as SDL3_image/SDL3_mixer
if(NOT SDL3_NET_DIR)
    if(SDL3_ROOT STREQUAL "${THIRD_PARTY_DIR}/sdl3/build")
        set(SDL3_NET_DIR "${THIRD_PARTY_DIR}/sdl3_net/build")
    else()
        set(SDL3_NET_DIR "${SDL3_ROOT}")
    endif()
endif()
if(SDL3_NET_DIR STREQUAL "${THIRD_PARTY_DIR}/sdl3_net/build")
    set(SDL3_net_DIR "${SDL3_NET_DIR}/lib/cmake/SDL3_net" CACHE PATH "" FORCE)
endif()
find_package(SDL3 REQUIRED HINTS "${SDL3_ROOT}")
find_package(SDL3_net REQUIRED HINTS "${SDL3_NET_DIR}")
target_link_libraries(3sx PRIVATE SDL3_net::SDL3_net)
endif() # ENABLE_NETPLAY

# If SDL3_ROOT was set externally (e.g. -DSDL3_ROOT=/app for flatpak),
# SDL3_image and SDL3_mixer are also installed there. Otherwise use third_party path.
if(NOT SDL3_IMAGE_ROOT)
    if(SDL3_ROOT STREQUAL "${THIRD_PARTY_DIR}/sdl3/build")
        set(SDL3_IMAGE_ROOT "${THIRD_PARTY_DIR}/sdl3_image/build")
    else()
        set(SDL3_IMAGE_ROOT "${SDL3_ROOT}")
    endif()
endif()

if(NOT SDL3_MIXER_ROOT)
    if(SDL3_ROOT STREQUAL "${THIRD_PARTY_DIR}/sdl3/build")
        set(SDL3_MIXER_ROOT "${THIRD_PARTY_DIR}/sdl3_mixer/build")
    else()
        set(SDL3_MIXER_ROOT "${SDL3_ROOT}")
    endif()
endif()

include_directories(
    ${SDL3_ROOT}/include
    ${SDL3_IMAGE_ROOT}/include
    ${SDL3_MIXER_ROOT}/include
    $<$<BOOL:${ENABLE_NETPLAY}>:${SDL3_NET_DIR}/include>
)

if(APPLE)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dylib
        ${SDL3_MIXER_ROOT}/lib/libSDL3_mixer.dylib
    )
elseif(WIN32)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.dll.a
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dll.a
        ${SDL3_MIXER_ROOT}/lib/libSDL3_mixer.dll.a
		dbghelp
        $<$<BOOL:${ENABLE_BROADCAST}>:Spout_static>
    )
    target_link_options(3sx PRIVATE
        --for-linker
        --pdb=3sx.pdb
    )
elseif(UNIX)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.so
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.so
        ${SDL3_MIXER_ROOT}/lib/libSDL3_mixer.so
    )
endif()

# ======================================
# Installation
# ======================================

# ======================================
# SPIR-V Shader Compilation
# ======================================
# Compile all .gpu.* (GLSL 450) shaders → .spv (SPIR-V) at build time.
# Source files use the naming convention: <name>.gpu.<stage>
# Output SPV files: <name>.<stage>.spv (with two legacy exceptions below)
find_program(GLSLC_EXE glslc HINTS ENV VULKAN_SDK PATH_SUFFIXES bin Bin)
find_program(GLSLANG_EXE glslangValidator HINTS ENV VULKAN_SDK PATH_SUFFIXES bin Bin)

set(SHADER_SRC_DIR "${CMAKE_SOURCE_DIR}/src/shaders")

# Two parallel lists: shader sources and their corresponding SPV output names.
set(GPU_SHADER_SRCS
    blit.gpu.frag
    blit.gpu.vert
    palette_convert.gpu.comp
    rect.gpu.frag
    rect.gpu.vert
    scene.gpu.vert
    scene_array.gpu.frag
    text.gpu.frag
    text.gpu.vert
)
set(GPU_SHADER_OUTS
    blit.frag.spv
    blit.vert.spv
    palette_convert.comp.spv
    rect.frag.spv
    rect.vert.spv
    vert.spv
    scene.spv
    text.frag.spv
    text.vert.spv
)

set(ALL_SHADER_SPVS "")
list(LENGTH GPU_SHADER_SRCS SHADER_COUNT)
math(EXPR SHADER_LAST "${SHADER_COUNT} - 1")

if(GLSLC_EXE OR GLSLANG_EXE)
    foreach(IDX RANGE ${SHADER_LAST})
        list(GET GPU_SHADER_SRCS ${IDX} SHADER_SRC_NAME)
        list(GET GPU_SHADER_OUTS ${IDX} SHADER_SPV_NAME)

        set(SHADER_SRC_PATH "${SHADER_SRC_DIR}/${SHADER_SRC_NAME}")
        set(SHADER_SPV_PATH "${SHADER_SRC_DIR}/${SHADER_SPV_NAME}")

        if(GLSLC_EXE)
            add_custom_command(
                OUTPUT "${SHADER_SPV_PATH}"
                COMMAND "${GLSLC_EXE}" "${SHADER_SRC_PATH}" -o "${SHADER_SPV_PATH}"
                DEPENDS "${SHADER_SRC_PATH}"
                COMMENT "Compiling ${SHADER_SRC_NAME} -> ${SHADER_SPV_NAME}"
                VERBATIM
            )
        else()
            add_custom_command(
                OUTPUT "${SHADER_SPV_PATH}"
                COMMAND "${GLSLANG_EXE}" -V "${SHADER_SRC_PATH}" -o "${SHADER_SPV_PATH}"
                DEPENDS "${SHADER_SRC_PATH}"
                COMMENT "Compiling ${SHADER_SRC_NAME} -> ${SHADER_SPV_NAME}"
                VERBATIM
            )
        endif()

        list(APPEND ALL_SHADER_SPVS "${SHADER_SPV_PATH}")
    endforeach()

    add_custom_target(compile_gpu_shaders DEPENDS ${ALL_SHADER_SPVS})
    add_dependencies(3sx compile_gpu_shaders)
else()
    message(WARNING "No SPIR-V compiler found (glslc or glslangValidator). "
        "GPU shaders will not be compiled. Install Vulkan SDK or ensure .spv files are pre-built.")
endif()

# ---------------------------------------------------------------------------
# Post-build: symlink heavy asset dirs, copy small shader files.
# On Windows uses mklink /J (NTFS junctions — no admin/Developer Mode needed).
# On Unix uses cmake -E create_symlink.
# Saves copying 3+ GB of sprites and 100+ MB of libretro shaders each build.
# ---------------------------------------------------------------------------
if(WIN32)
    # Convert paths to native backslashes for mklink /J
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets" _ASSETS_SRC)
    file(TO_NATIVE_PATH "${THIRD_PARTY_DIR}/slang-shaders" _SLANG_SRC)
    # Write a helper batch script at configure time.
    # It creates the shaders directory first (for the libretro junction),
    # then creates junctions only if they do not already exist.
    file(WRITE "${CMAKE_BINARY_DIR}/create_asset_links.bat"
        "@echo off\n"
        "if not exist \"%1\\assets\" (mklink /J \"%1\\assets\" \"${_ASSETS_SRC}\")\n"
        "if not exist \"%1\\shaders\" (mkdir \"%1\\shaders\")\n"
        "if not exist \"%1\\shaders\\libretro\" (mklink /J \"%1\\shaders\\libretro\" \"${_SLANG_SRC}\")\n"
    )
    add_custom_command(TARGET 3sx POST_BUILD
        # Create asset + libretro shader junctions via .bat
        COMMAND "${CMAKE_BINARY_DIR}/create_asset_links.bat" "$<TARGET_FILE_DIR:3sx>"
        # shaders/ -> small copy (37 GLSL source files, overwrites into the dir)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_SOURCE_DIR}/src/shaders" "$<TARGET_FILE_DIR:3sx>/shaders"
        COMMENT "Creating asset junctions + copying shaders"
    )
    if(ENABLE_LIBRASHADER)
        add_custom_command(TARGET 3sx POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/shaders/custom" "$<TARGET_FILE_DIR:3sx>/shaders/libretro/custom"
            COMMENT "Copying custom shader presets"
        )
    endif()
else()
    if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_BINARY_DIR)
        add_custom_command(TARGET 3sx POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E create_symlink "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:3sx>/assets"
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                "${CMAKE_SOURCE_DIR}/src/shaders" "$<TARGET_FILE_DIR:3sx>/shaders"
            COMMENT "Symlinking assets + copying shaders"
        )
        if(ENABLE_LIBRASHADER)
            add_custom_command(TARGET 3sx POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E create_symlink "${THIRD_PARTY_DIR}/slang-shaders" "$<TARGET_FILE_DIR:3sx>/shaders/libretro"
                COMMAND ${CMAKE_COMMAND} -E copy_directory
                    "${CMAKE_SOURCE_DIR}/shaders/custom" "$<TARGET_FILE_DIR:3sx>/shaders/libretro/custom"
                COMMENT "Symlinking libretro shaders + copying custom presets"
            )
        endif()
    endif()
endif()

if(ENABLE_TRAINING)
    add_custom_command(TARGET 3sx POST_BUILD
        # Lua scripts for trials/training
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:3sx>/lua
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/lua/sf3_3rd_trial_lua/sf3_3rd_trial_clean.lua $<TARGET_FILE_DIR:3sx>/lua/sf3_3rd_trial_clean.lua
        # Lua compat modules (FBNeo API shim)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/lua/compat $<TARGET_FILE_DIR:3sx>/lua/compat
        # Training mode bootstrap
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/src/lua/training_main.lua $<TARGET_FILE_DIR:3sx>/lua/training_main.lua
        # Effie's training Lua modules (data, tools, prediction)
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/lua/3rd_training_lua-main/src $<TARGET_FILE_DIR:3sx>/lua/src
        # Effie's framedata JSONs
        COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/src/lua/3rd_training_lua-main/data $<TARGET_FILE_DIR:3sx>/lua/data
    )
endif()

# Heavy asset/shader dirs: create junctions (Win) or symlinks (Unix)
# instead of copying 3+ GB of sprites and 100+ MB of libretro shaders.
if(WIN32)
    file(TO_NATIVE_PATH "${CMAKE_SOURCE_DIR}/assets" _INST_ASSETS_SRC)
    file(TO_NATIVE_PATH "${THIRD_PARTY_DIR}/slang-shaders" _INST_SLANG_SRC)
    file(WRITE "${CMAKE_BINARY_DIR}/create_install_links.bat"
        "@echo off\n"
        "if not exist \"%1\\assets\" (mklink /J \"%1\\assets\" \"${_INST_ASSETS_SRC}\")\n"
        "if not exist \"%1\\shaders\" (mkdir \"%1\\shaders\")\n"
        "if not exist \"%1\\shaders\\libretro\" (mklink /J \"%1\\shaders\\libretro\" \"${_INST_SLANG_SRC}\")\n"
    )
    install(CODE "execute_process(COMMAND \"${CMAKE_BINARY_DIR}/create_install_links.bat\" \"\${CMAKE_INSTALL_PREFIX}/bin\")")
else()
    install(CODE "
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
            \"${CMAKE_SOURCE_DIR}/assets\" \"\${CMAKE_INSTALL_PREFIX}/bin/assets\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E create_symlink
            \"${THIRD_PARTY_DIR}/slang-shaders\" \"\${CMAKE_INSTALL_PREFIX}/bin/shaders/libretro\")
    ")
endif()
# Shaders: small copy (37 files) + custom presets
install(DIRECTORY src/shaders DESTINATION bin)
if(ENABLE_LIBRASHADER)
    install(DIRECTORY shaders/custom/ DESTINATION bin/shaders/libretro/custom)
endif()
# Lua runtime files (only when training is enabled)
if(ENABLE_TRAINING)
    install(DIRECTORY src/lua/compat DESTINATION bin/lua)
    install(FILES src/lua/training_main.lua DESTINATION bin/lua)
    install(FILES src/lua/sf3_3rd_trial_lua/sf3_3rd_trial_clean.lua DESTINATION bin/lua)
    install(DIRECTORY src/lua/3rd_training_lua-main/src DESTINATION bin/lua)
    install(DIRECTORY src/lua/3rd_training_lua-main/data DESTINATION bin/lua)
endif()



if(APPLE)
    install(TARGETS 3sx
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )

    install(FILES
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dylib
        ${SDL3_MIXER_ROOT}/lib/libSDL3_mixer.dylib
        DESTINATION 3SX.app/Contents/Frameworks
    )
    if(ENABLE_NETPLAY)
        install(FILES ${SDL3_NET_DIR}/lib/libSDL3_net.dylib
            DESTINATION 3SX.app/Contents/Frameworks)
    endif()
elseif(WIN32)
    install(TARGETS 3sx
		RUNTIME_DEPENDENCY_SET deps
        RUNTIME DESTINATION bin
    )

    execute_process(COMMAND cygpath -m /mingw64/bin
		OUTPUT_VARIABLE binPath
		OUTPUT_STRIP_TRAILING_WHITESPACE
	)
	
	# automatically copies all the dependent DLLS, and ignores the system ones because they aren't necessary
	install(RUNTIME_DEPENDENCY_SET deps
		DIRECTORIES ${binPath} "${SDL3_ROOT}/bin" "${SDL3_IMAGE_ROOT}/bin" "${SDL3_MIXER_ROOT}/bin" "${SDL3_NET_DIR}/bin"
		PRE_EXCLUDE_REGEXES "^api"
		POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
		DESTINATION bin
	)

    install(FILES
        "build/3sx.pdb"
        "${binPath}/zlib1.dll"
        "${SDL3_ROOT}/bin/SDL3.dll"
        "${SDL3_IMAGE_ROOT}/bin/SDL3_image.dll"
        "${SDL3_MIXER_ROOT}/bin/SDL3_mixer.dll"
        $<$<BOOL:${ENABLE_NETPLAY}>:${SDL3_NET_DIR}/bin/SDL3_net.dll>
        DESTINATION bin
    )
    install(FILES $<TARGET_FILE:renderer_hd> DESTINATION bin OPTIONAL)
elseif(UNIX)
    install(TARGETS 3sx
        RUNTIME_DEPENDENCY_SET deps
        RUNTIME DESTINATION bin
    )

    install(RUNTIME_DEPENDENCY_SET deps
        DIRECTORIES "${SDL3_ROOT}/lib" "${SDL3_IMAGE_ROOT}/lib" "${SDL3_MIXER_ROOT}/lib" "${SDL3_NET_DIR}/lib" "${THIRD_PARTY_DIR}/SDL_shadercross/build"
        POST_EXCLUDE_REGEXES "^/lib" "^/usr" "^/opt"
        DESTINATION lib
    )
    install(FILES $<TARGET_FILE:renderer_hd> DESTINATION bin OPTIONAL)
endif()

# ======================================
# Packaging
# ======================================

set(CPACK_PACKAGE_NAME "3SX")
set(CPACK_PACKAGE_VENDOR "3sxtra")
# set(CPACK_PACKAGE_VERSION "1.0.0")

if(WIN32)
    set(CPACK_GENERATOR "ZIP")
elseif(APPLE)
    set(CPACK_GENERATOR "DragNDrop")
else(UNIX)
    set(CPACK_GENERATOR "TGZ")
endif()

include(CPack)




# ======================================
# Feature Toggle Summary
# ======================================
message(STATUS "")
message(STATUS "=== 3SX Feature Toggles ===")
message(STATUS "  ENABLE_NETPLAY          = ${ENABLE_NETPLAY}")
message(STATUS "  ENABLE_RMLUI            = ${ENABLE_RMLUI}")
message(STATUS "  ENABLE_TRAINING         = ${ENABLE_TRAINING}")
message(STATUS "  ENABLE_BROADCAST        = ${ENABLE_BROADCAST}")
message(STATUS "  ENABLE_MODS             = ${ENABLE_MODS}")
message(STATUS "  ENABLE_BEZEL            = ${ENABLE_BEZEL}")
message(STATUS "  ENABLE_CONTROLLER_IMAGE = ${ENABLE_CONTROLLER_IMAGE}")
message(STATUS "  ENABLE_REPLAY           = ${ENABLE_REPLAY}")
message(STATUS "  ENABLE_LIBRASHADER      = ${ENABLE_LIBRASHADER}")
message(STATUS "  ENABLE_DEBUG_HUD        = ${ENABLE_DEBUG_HUD}")
message(STATUS "  ENABLE_LEADERBOARD      = ${ENABLE_LEADERBOARD}")
message(STATUS "===========================")
message(STATUS "")

# ======================================
# Testing Framework (CMocka)
# ======================================

option(ENABLE_TESTS "Enable unit tests" ON)

if(ENABLE_TESTS)
    message(STATUS "Configuring CMocka...")
    FetchContent_Declare(
        cmocka
        GIT_REPOSITORY https://git.cryptomilk.org/projects/cmocka.git
        GIT_TAG        cmocka-1.1.7
    )
    set(WITH_STATIC_LIB ON CACHE BOOL "CMocka: Build with static library" FORCE)
    set(WITH_CMOCKERY_SUPPORT OFF CACHE BOOL "CMocka: Install cmockery header" FORCE)
    set(WITH_EXAMPLES OFF CACHE BOOL "CMocka: Build examples" FORCE)
    set(UNIT_TESTING OFF CACHE BOOL "CMocka: Build with unit testing" FORCE)



    
    FetchContent_MakeAvailable(cmocka)
    message(STATUS "CMocka configured.")

    target_compile_definitions(cmocka PRIVATE HAVE_SIGNAL=0 HAVE_SIGNAL_H=0)
    add_subdirectory(tests)
endif()


# Renderer HD plugin
if(ENABLE_LIBRASHADER)
    add_subdirectory(plugins/renderer_hd)
endif()

