add_library(renderer_hd SHARED
    renderer_hd.c
)

# Inherit THIRD_PARTY_DIR and SDL3_ROOT from parent scope
if(NOT THIRD_PARTY_DIR)
    set(THIRD_PARTY_DIR "${CMAKE_SOURCE_DIR}/third_party")
endif()
if(NOT SDL3_ROOT)
    set(SDL3_ROOT "${THIRD_PARTY_DIR}/sdl3/build")
endif()

target_include_directories(renderer_hd PRIVATE
    ${CMAKE_SOURCE_DIR}/src/include
    ${THIRD_PARTY_DIR}/stb
    ${SDL3_ROOT}/include
)

if(WIN32)
    target_link_libraries(renderer_hd PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.dll.a
    )
elseif(APPLE)
    target_link_libraries(renderer_hd PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.0.dylib
    )
elseif(UNIX)
    target_link_libraries(renderer_hd PRIVATE
        ${SDL3_ROOT}/lib/libSDL3.so
    )
endif()

# Copy the DLL next to the main executable after build
add_custom_command(TARGET renderer_hd POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:renderer_hd>
        $<TARGET_FILE_DIR:3sx>
)
