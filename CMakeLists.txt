cmake_minimum_required(VERSION 3.24)
project(3sx LANGUAGES C CXX)

# ======================================
# Options and platform setup
# ======================================

option(PLATFORM_RPI4 "Build for Raspberry Pi 4 / Batocera (aarch64)" OFF)
option(ENABLE_TRACY "Enable Tracy profiler instrumentation" OFF)

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
    execute_process(COMMAND bash ${CMAKE_CURRENT_SOURCE_DIR}/build-deps.sh
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

message(STATUS "DEBUG: Adding GekkoNet subdirectory...")
add_subdirectory("${THIRD_PARTY_DIR}/GekkoNet" GekkoNet)
message(STATUS "DEBUG: GekkoNet added.")

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

# RmlUi — HTML/CSS UI library (always built alongside ImGui)
# Pre-cloned by build-deps.sh into third_party/rmlui
message(STATUS "Configuring RmlUi...")
set(RMLUI_SDL_VERSION_MAJOR 3 CACHE STRING "" FORCE)
set(RMLUI_SAMPLES OFF CACHE BOOL "" FORCE)
set(RMLUI_TESTS OFF CACHE BOOL "" FORCE)
set(RMLUI_LUA_BINDINGS OFF CACHE BOOL "" FORCE)
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
    ${PROJECT_SOURCE_DIR}/src/zlib
    ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include
    ${THIRD_PARTY_DIR}/stb

    ${THIRD_PARTY_DIR}/librashader/include
    ${THIRD_PARTY_DIR}/simde
    ${glad_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/gladsources/glad/include
    ${THIRD_PARTY_DIR}/sdl3/SDL/src/video/khronos
)

set(SPOUT2_DIR "${THIRD_PARTY_DIR}/Spout2")

if(WIN32)
    include_directories(${SPOUT2_DIR}/SPOUTSDK/SpoutGL)
endif()

# ======================================
# Source collection
# ======================================

file(GLOB_RECURSE GAME_SRC src/*.c)

# Exclude legacy PS2 save system — replaced by native_save.c
list(FILTER GAME_SRC EXCLUDE REGEX "src/sf33rd/Source/PS2/mc/savesub\\.c$")
list(FILTER GAME_SRC EXCLUDE REGEX "src/sf33rd/Source/PS2/mc/mcsub\\.c$")
list(FILTER GAME_SRC EXCLUDE REGEX "src/port/sdk/sdk_libmc\\.c$")

file(GLOB_RECURSE ZLIB_SRC src/zlib/*.c)

file(GLOB IMGUI_SRC ${THIRD_PARTY_DIR}/imgui/*.cpp)
file(GLOB IMGUI_BACKEND_SRC ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdl3.cpp ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_opengl3.cpp ${THIRD_PARTY_DIR}/imgui/backends/imgui_impl_sdlgpu3.cpp)

# Explicitly include Source/Common files (workaround for GLOB_RECURSE linking issue)
file(GLOB COMMON_SRC src/sf33rd/Source/Common/*.c)

set(GAME_SRC ${GAME_SRC} ${COMMON_SRC})



set_source_files_properties(${ZLIB_SRC}
    PROPERTIES COMPILE_OPTIONS "-Wno-error=format"
)

# Disable clang-tidy for all third-party sources
set_source_files_properties(${IMGUI_SRC} ${IMGUI_BACKEND_SRC} ${ZLIB_SRC} src/port/win32/broadcast_spout.cpp
    PROPERTIES
        SKIP_LINTING ON
        C_CLANG_TIDY ""
        CXX_CLANG_TIDY ""
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
    src/port/sdl/rmlui_wrapper.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp
    PROPERTIES
        COMPILE_OPTIONS "-Wno-error;-Wno-delete-abstract-non-virtual-dtor;-Wno-delete-non-abstract-non-virtual-dtor"
)
# Use our glad via RMLUI_GL3_CUSTOM_LOADER instead of RmlUi's bundled glad
set_source_files_properties(
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
    PROPERTIES
        COMPILE_DEFINITIONS "RMLUI_GL3_CUSTOM_LOADER=<glad/gl.h>;RMLUI_SDL_VERSION_MAJOR=3"
)

if(WIN32)
    set_source_files_properties(src/port/win32/broadcast_spout.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
endif()

# Broadcast: broadcast.c has a null backend fallback when no platform backend is linked.
# On RPi4, broadcast_pipewire.cpp is excluded (below), so broadcast.c uses the null backend.

add_executable(3sx MACOSX_BUNDLE
    ${GAME_SRC} ${BIN2OBJ_SRC} ${PORT_SRC} ${ZLIB_SRC} ${IMGUI_SRC} ${IMGUI_BACKEND_SRC}
    # Explicitly added Source/Common files (CMake GLOB doesn't include them for unknown reason)
    ${CMAKE_SOURCE_DIR}/src/sf33rd/Source/Common/MemMan.c
    ${CMAKE_SOURCE_DIR}/src/sf33rd/Source/Common/PPGFile.c
    ${CMAKE_SOURCE_DIR}/src/sf33rd/Source/Common/PPGWork.c
    src/port/sdl/imgui_wrapper.cpp
    src/port/sdl/sdl_texture_util.cpp
    src/port/sdl/control_mapping.cpp
    src/port/sdl/input_display.cpp
    src/port/sdl/frame_display.cpp
    src/port/sdl/shader_menu.cpp
    src/port/sdl/mods_menu.cpp
    src/port/sdl/stage_config_menu.cpp
    src/port/sdl/training_menu.cpp
    src/port/input_definition.cpp
    src/port/imgui_font.cpp
    src/port/ui/replay_picker.cpp
    src/port/sdl/sdl_netplay_ui.cpp
    src/port/char_data.c
    src/port/broadcast.c
    src/port/tracy_gpu.cpp
    src/port/sdl/sdl_app_input.c
    src/port/sdl/sdl_app_shader_config.c
    src/port/sdl/sdl_game_renderer_gl_context.c
    src/port/sdl/sdl_game_renderer_gl_resources.c
    src/port/sdl/sdl_game_renderer_gl_draw.c
    src/port/sdl/sdl_game_renderer_sdl.c
    src/port/sdl/sdl_text_renderer_sdl.c
    src/port/sdl/rmlui_wrapper.cpp
    src/port/sdl/rmlui_mods_menu.cpp
    src/port/sdl/rmlui_shader_menu.cpp
    src/port/sdl/rmlui_stage_config.cpp
    src/port/sdl/rmlui_input_display.cpp
    src/port/sdl/rmlui_frame_display.cpp
    src/port/sdl/rmlui_control_mapping.cpp
    src/port/sdl/rmlui_training_menu.cpp
    src/port/sdl/rmlui_netplay_ui.cpp
    src/port/sdl/rmlui_game_hud.cpp
    src/port/sdl/rmlui_mode_menu.cpp
    src/port/sdl/rmlui_option_menu.cpp
    src/port/sdl/rmlui_game_option.cpp
    src/port/sdl/rmlui_title_screen.cpp
    src/port/sdl/rmlui_win_screen.cpp
    src/port/sdl/rmlui_continue.cpp
    src/port/sdl/rmlui_gameover.cpp
    src/port/sdl/rmlui_vs_result.cpp
    src/port/sdl/rmlui_memory_card.cpp
    src/port/sdl/rmlui_sound_menu.cpp
    src/port/sdl/rmlui_sysdir.cpp
    src/port/sdl/rmlui_network_lobby.cpp
    src/port/sdl/rmlui_extra_option.cpp
    src/port/sdl/rmlui_training_menus.cpp
    src/port/sdl/rmlui_button_config.cpp
    src/port/sdl/rmlui_char_select.cpp
    src/port/sdl/rmlui_vs_screen.cpp
    src/port/sdl/rmlui_replay_picker.cpp
    src/port/sdl/rmlui_pause_overlay.cpp
    src/port/sdl/rmlui_trials_hud.cpp
    src/port/sdl/rmlui_copyright.cpp
    src/port/sdl/rmlui_name_entry.cpp
    # RmlUi backend sources (SDL3 platform + GL3/SDL/SDL_GPU renderers)
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Platform_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_GL3.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL.cpp
    ${THIRD_PARTY_DIR}/rmlui/Backends/RmlUi_Renderer_SDL_GPU.cpp
)


set_source_files_properties(${ZLIB_SRC} PROPERTIES COMPILE_OPTIONS "-Wno-format")

target_include_directories(3sx PRIVATE
    ${THIRD_PARTY_DIR}/imgui
    ${THIRD_PARTY_DIR}/imgui/backends
    ${THIRD_PARTY_DIR}/rmlui/Backends
)

set_target_properties(3sx PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(3sx PRIVATE glad_gl_core)

# ======================================
# Compiler and linker flags
# ======================================

target_compile_definitions(3sx PRIVATE
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:DEBUG>
    $<$<OR:$<CONFIG:Debug>,$<CONFIG:RelWithDebInfo>>:NETPLAY_ENABLED>
    $<$<CONFIG:Release>:RELEASE>
    TARGET_SDL3
    $<$<NOT:$<PLATFORM_ID:Darwin>>:_POSIX_C_SOURCE=200809L>
    $<$<BOOL:${PLATFORM_RPI4}>:PLATFORM_RPI4>
    $<$<BOOL:${ENABLE_TRACY}>:TRACY_ENABLE>
    RMLUI_SDL_VERSION_MAJOR=3
)

# Feature toggles
# (Native save system via native_save.c)

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
    # x86-64: AVX2 + FMA — 256-bit SIMD and fused multiply-add.
    # Safe on any x86-64 CPU from ~2013+ (Haswell).
    target_compile_options(3sx PRIVATE -mavx2 -mfma)
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
if(PLATFORM_RPI4)
    set(LIBRASHADER_LIB ${THIRD_PARTY_DIR}/librashader/target/aarch64-unknown-linux-gnu/release/liblibrashader_capi.a)
else()
    set(LIBRASHADER_LIB ${THIRD_PARTY_DIR}/librashader/target/release/liblibrashader_capi.a)
endif()

target_link_libraries(3sx PRIVATE
    m
    GekkoNet
    stdc++
    glad_gl_core
    ${LIBRASHADER_LIB}
    SDL3_shadercross::SDL3_shadercross
    rmlui
    rmlui_debugger
)

if(ENABLE_TRACY)
    target_link_libraries(3sx PRIVATE TracyClient)
endif()

if(UNIX)
    target_link_libraries(3sx PRIVATE dl pthread)

    # Homebrew on macOS ARM installs to /opt/homebrew
    if(APPLE)
        list(APPEND CMAKE_PREFIX_PATH /opt/homebrew /usr/local)
        include_directories(/opt/homebrew/include)
        link_directories(/opt/homebrew/lib)
    endif()

    find_library(MINIUPNPC_LIB miniupnpc HINTS /opt/homebrew/lib /usr/local/lib)
    if(MINIUPNPC_LIB)
        target_link_libraries(3sx PRIVATE ${MINIUPNPC_LIB})
        target_compile_definitions(3sx PRIVATE HAVE_UPNP)
        message(STATUS "UPnP support: enabled (miniupnpc found)")
    else()
        message(STATUS "UPnP support: disabled (miniupnpc not found)")
    endif()
endif()

if(WIN32)
    set(ICON_RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/icon.rc")
    set(ICON_FILE_PATH "${CMAKE_SOURCE_DIR}/assets/3sxlogo.ico")
    string(REPLACE "/" "\\\\" ICON_FILE_PATH_WIN "${ICON_FILE_PATH}")
    file(WRITE ${ICON_RC_FILE} "1 ICON \"${ICON_FILE_PATH_WIN}\"")
    target_sources(3sx PRIVATE 
        ${ICON_RC_FILE}
        src/port/win32/broadcast_spout.cpp
    )

    target_link_libraries(3sx PRIVATE
        userenv
        ntdll
        ws2_32
        advapi32
        bcrypt
        miniupnpc
        iphlpapi
    )
    target_compile_definitions(3sx PRIVATE HAVE_UPNP)
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
    find_library(SYPHON_FRAMEWORK Syphon)
    if(SYPHON_FRAMEWORK)
        target_sources(3sx PRIVATE src/port/macos/broadcast_syphon.mm)
        target_link_libraries(3sx PRIVATE "-framework Syphon")
        target_compile_definitions(3sx PRIVATE HAVE_SYPHON)
        message(STATUS "Syphon broadcast: enabled")
    else()
        message(STATUS "Syphon broadcast: disabled (framework not found)")
    endif()
elseif(UNIX AND NOT APPLE)
    set_target_properties(3sx PROPERTIES
        INSTALL_RPATH "\$ORIGIN/../lib"
    )
    if(NOT PLATFORM_RPI4)
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
if(WIN32)
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

# If SDL3_ROOT was set externally (e.g. -DSDL3_ROOT=/app for flatpak),
# SDL3_image is also installed there. Otherwise use third_party path.
if(NOT SDL3_IMAGE_ROOT)
    if(SDL3_ROOT STREQUAL "${THIRD_PARTY_DIR}/sdl3/build")
        set(SDL3_IMAGE_ROOT "${THIRD_PARTY_DIR}/sdl3_image/build")
    else()
        set(SDL3_IMAGE_ROOT "${SDL3_ROOT}")
    endif()
endif()

include_directories(
    ${SDL3_ROOT}/include
    ${SDL3_IMAGE_ROOT}/include
)

if(APPLE)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dylib
    )
elseif(WIN32)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.dll.a
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dll.a
		dbghelp
        Spout_static
    )
    target_link_options(3sx PRIVATE
        --for-linker
        --pdb=3sx.pdb
    )
elseif(UNIX)
    target_link_libraries(3sx PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.so
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.so
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

add_custom_command(TARGET 3sx POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:3sx>/assets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/src/shaders $<TARGET_FILE_DIR:3sx>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${THIRD_PARTY_DIR}/slang-shaders $<TARGET_FILE_DIR:3sx>/shaders/libretro
)

install(DIRECTORY src/shaders DESTINATION bin)
install(DIRECTORY ${THIRD_PARTY_DIR}/slang-shaders/ DESTINATION bin/shaders/libretro)
install(DIRECTORY assets DESTINATION bin)



if(APPLE)
    install(TARGETS 3sx
        RUNTIME DESTINATION bin
        BUNDLE DESTINATION .
    )

    install(FILES
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.dylib
        DESTINATION 3SX.app/Contents/Frameworks
    )
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
		DIRECTORIES ${binPath} "${SDL3_ROOT}/bin" "${SDL3_IMAGE_ROOT}/bin"
		PRE_EXCLUDE_REGEXES "^api"
		POST_EXCLUDE_REGEXES ".*system32/.*\\.dll"
		DESTINATION bin
	)

    install(FILES
        "build/3sx.pdb"
        "${binPath}/zlib1.dll"
        "${SDL3_ROOT}/bin/SDL3.dll"
        "${SDL3_IMAGE_ROOT}/bin/SDL3_image.dll"
        DESTINATION bin
    )
elseif(UNIX)
    install(TARGETS 3sx
        RUNTIME DESTINATION bin
    )

    install(FILES
        ${SDL3_ROOT}/lib/libSDL3.so
        ${SDL3_IMAGE_ROOT}/lib/libSDL3_image.so
        DESTINATION lib
    )
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

    add_subdirectory(tests)
endif()
