set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(PS3 ON)

# SNC PATH
set(PS3_SDK_ROOT "C:/usr/local/cell")
set(SN_ROOT "${PS3_SDK_ROOT}/host-win32/sn")

# Mark compilers as working (skip ABI detection tests)
set(CMAKE_C_COMPILER_WORKS 1)
set(CMAKE_CXX_COMPILER_WORKS 1)

# Compilers
set(CMAKE_C_COMPILER "${SN_ROOT}/bin/ps3ppusnc.exe")
set(CMAKE_CXX_COMPILER "${SN_ROOT}/bin/ps3ppusnc.exe")
set(CMAKE_AR "${SN_ROOT}/bin/ps3snarl.exe" CACHE FILEPATH "Archiver")
set(CMAKE_LINKER "${SN_ROOT}/bin/ps3ppuld.exe" CACHE FILEPATH "Linker")

# Flags
# --c99 is essential for SNC C compilation
set(CMAKE_C_FLAGS "--c99" CACHE STRING "SNC C Flags" FORCE)
set(CMAKE_CXX_FLAGS "" CACHE STRING "SNC C++ Flags" FORCE)

# Override the entire compile command to prevent CMake from injecting
# GNU-style dependency tracking flags (-MD -MT -MF) that SNC chokes on.
# <CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>
set(CMAKE_C_COMPILE_OBJECT "<CMAKE_C_COMPILER> <DEFINES> <INCLUDES> <FLAGS> -o <OBJECT> -c <SOURCE>")

# Also disable dep tracking so Ninja doesn't expect .d files
set(CMAKE_C_STANDARD_COMPUTED_DEFAULT OFF)
set(CMAKE_DEPFILE_FLAGS_C "" CACHE STRING "" FORCE)
set(CMAKE_C_DEPENDS_USE_COMPILER FALSE)
set(CMAKE_C_DEPFILE_FORMAT "")
set(CMAKE_DEPENDS_USE_COMPILER FALSE)

# Includes
include_directories(
    "${PROJECT_SOURCE_DIR}/src/include"
    "${PROJECT_SOURCE_DIR}/src/port/ps3/include"
    "${PS3_SDK_ROOT}/target/ppu/include"
    "${PS3_SDK_ROOT}/target/common/include"
)

# Standard libs
link_directories(
    "${PS3_SDK_ROOT}/target/ppu/lib"
)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Map=3sx.map")
