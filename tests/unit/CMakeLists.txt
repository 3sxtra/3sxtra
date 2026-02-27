# =============================================================================
# Helper functions to reduce platform-specific linking duplication
# =============================================================================

# Link SDL3 only (for tests that don't need GekkoNet)
function(target_link_sdl3 TARGET_NAME)
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ${SDL3_ROOT}/lib/libSDL3.dll.a)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE ${SDL3_ROOT}/lib/libSDL3.so)
    endif()
endfunction()

# Link SDL3 + glad_gl_core (for bezel/OpenGL tests)
function(target_link_sdl3_glad TARGET_NAME)
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE ${SDL3_ROOT}/lib/libSDL3.dll.a glad_gl_core)
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE ${SDL3_ROOT}/lib/libSDL3.so glad_gl_core)
    endif()
endfunction()

# Link GekkoNet + SDL3 + Win32 system libs (for netplay tests)
function(target_link_gekkonet_sdl3 TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE 
        ${PROJECT_SOURCE_DIR}/include 
        ${PROJECT_SOURCE_DIR}/submodules/GekkoNet/GekkoLib/include
    )
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE 
            GekkoNet 
            ${SDL3_ROOT}/lib/libSDL3.dll.a
            ws2_32 userenv ntdll advapi32 bcrypt dbghelp
        )
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE GekkoNet ${SDL3_ROOT}/lib/libSDL3.so)
    endif()
endfunction()

# =============================================================================
# Unit Tests
# =============================================================================

add_unit_test(test_smoke test_smoke.c)

add_unit_test(test_memman 
    test_memman.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Common/MemMan.c
)

add_unit_test(test_charset_poc
    test_charset_poc.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/engine/charset.c
)

add_unit_test(test_renderer_interface
    test_renderer_interface.c
    ${PROJECT_SOURCE_DIR}/src/port/sdl/renderer.c
)

add_unit_test(test_game_state
    test_game_state.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/game_globals.c
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
    mocks_globals.c
)
target_include_directories(test_game_state PRIVATE ${PROJECT_SOURCE_DIR}/include)

# Font rendering conversion test - isolated algorithm test
add_unit_test(test_font_rendering test_font_rendering.c)
file(GLOB IMGUI_SRC ${PROJECT_SOURCE_DIR}/third_party/imgui/*.cpp)

add_unit_test(test_imgui_font
    test_imgui_font.cpp
    ${PROJECT_SOURCE_DIR}/src/port/imgui_font.cpp
    ${IMGUI_SRC}
)
target_include_directories(test_imgui_font PRIVATE 
    ${PROJECT_SOURCE_DIR}/third_party/imgui
    ${PROJECT_SOURCE_DIR}/include
)

# -----------------------------------------------------------------------------
# Netplay tests (use target_link_gekkonet_sdl3)
# -----------------------------------------------------------------------------

add_unit_test(test_netplay_metrics
    test_netplay_metrics.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_metrics)

add_unit_test(test_netplay_events
    test_netplay_events.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_events)

add_unit_test(test_netplay_ui
    test_netplay_ui.c
    ${PROJECT_SOURCE_DIR}/src/port/sdl/sdl_netplay_ui.cpp
    mocks_netplay_ui_deps.c
    mocks_netplay.c
    test_netplay_ui_helper.cpp
    ${IMGUI_SRC}
)
target_include_directories(test_netplay_ui PRIVATE ${PROJECT_SOURCE_DIR}/third_party/imgui)
target_link_gekkonet_sdl3(test_netplay_ui)

add_unit_test(test_netplay_refactor
    test_netplay_refactor.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_refactor)

add_unit_test(test_state_differ
    test_state_differ.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_state_differ)

add_unit_test(test_effect_state_persistence
    test_effect_state_persistence.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_effect_state_persistence)

add_unit_test(test_netplay_oob
    test_netplay_oob.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_oob)

add_unit_test(test_netplay_init
    test_netplay_init.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_compile_definitions(test_netplay_init PRIVATE DEBUG)
target_link_gekkonet_sdl3(test_netplay_init)

add_unit_test(test_netplay_catchup
    test_netplay_catchup.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_catchup)

add_unit_test(test_stun
    test_stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
)
target_link_gekkonet_sdl3(test_stun)

add_unit_test(test_netplay_run
    test_netplay_run.c
    mocks_netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
)
target_link_gekkonet_sdl3(test_netplay_run)

# -----------------------------------------------------------------------------
# SDL3-only tests (use target_link_sdl3)
# -----------------------------------------------------------------------------

add_unit_test(test_paths
    test_paths.c
    ${PROJECT_SOURCE_DIR}/src/port/paths.c
)
target_include_directories(test_paths PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3(test_paths)

add_unit_test(test_config
    test_config.c
    mocks_paths.c
    ${PROJECT_SOURCE_DIR}/src/port/config.c
)
target_include_directories(test_config PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3(test_config)

add_unit_test(test_lobby_server
    test_lobby_server.c
)
target_include_directories(test_lobby_server PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3(test_lobby_server)

# -----------------------------------------------------------------------------
# Bezel tests (use target_link_sdl3_glad)
# -----------------------------------------------------------------------------

add_unit_test(test_bezel_assets
    test_bezel_assets.c
    ${PROJECT_SOURCE_DIR}/src/port/sdl_bezel.c
    ${PROJECT_SOURCE_DIR}/src/port/paths.c
    mocks_imgui_wrapper.c
)
target_include_directories(test_bezel_assets PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3_glad(test_bezel_assets)

add_unit_test(test_bezel_layout
    test_bezel_layout.c
    ${PROJECT_SOURCE_DIR}/src/port/sdl_bezel.c
    ${PROJECT_SOURCE_DIR}/src/port/paths.c
    mocks_imgui_wrapper.c
)
target_include_directories(test_bezel_layout PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3_glad(test_bezel_layout)

# -----------------------------------------------------------------------------
# Other tests (no platform-specific linking needed)
# -----------------------------------------------------------------------------

add_unit_test(test_globals_access
    test_globals_access.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/game_globals.c
)
target_include_directories(test_globals_access PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_game_state_roundtrip
    test_game_state_roundtrip.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/game_globals.c
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
    mocks_globals.c
)
target_include_directories(test_game_state_roundtrip PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_broadcast_config test_broadcast_config.c)
target_include_directories(test_broadcast_config PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_cli test_cli.c ${PROJECT_SOURCE_DIR}/src/port/cli_parser.c)
target_include_directories(test_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
    add_unit_test(test_broadcast_win32 test_broadcast_win32.cpp ${PROJECT_SOURCE_DIR}/src/port/win32/broadcast_spout.cpp)
    target_include_directories(test_broadcast_win32 PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
        ${PROJECT_SOURCE_DIR}/submodules/Spout2/SPOUTSDK/SpoutGL
    )
    target_link_libraries(test_broadcast_win32 PRIVATE Spout_static)
    target_compile_features(test_broadcast_win32 PRIVATE cxx_std_17)
    set_target_properties(test_broadcast_win32 PROPERTIES LINKER_LANGUAGE CXX)
    set_source_files_properties(test_broadcast_win32.cpp ${PROJECT_SOURCE_DIR}/src/port/win32/broadcast_spout.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
endif()

add_unit_test(test_menu_bridge test_menu_bridge.c ${PROJECT_SOURCE_DIR}/src/port/menu_bridge.c)
add_unit_test(test_trials test_trials.c ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/training/trials.c)
target_include_directories(test_trials PRIVATE ${PROJECT_SOURCE_DIR}/include)
