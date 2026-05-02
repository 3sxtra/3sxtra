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
if(ENABLE_NETPLAY)
function(target_link_gekkonet_sdl3 TARGET_NAME)
    target_include_directories(${TARGET_NAME} PRIVATE 
        ${PROJECT_SOURCE_DIR}/include 
        ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include
        ${SDL3_NET_DIR}/include
    )
    if(WIN32)
        target_link_libraries(${TARGET_NAME} PRIVATE 
            GekkoNet 
            ${SDL3_ROOT}/lib/libSDL3.dll.a
            ${SDL3_NET_DIR}/lib/libSDL3_net.dll.a
            ws2_32 userenv ntdll advapi32 bcrypt dbghelp
        )
    else()
        target_link_libraries(${TARGET_NAME} PRIVATE GekkoNet ${SDL3_ROOT}/lib/libSDL3.so ${SDL3_NET_DIR}/lib/libSDL3_net.so)
    endif()
endfunction()
endif() # ENABLE_NETPLAY

# =============================================================================
# Decomposed game globals (was game_globals.c, now split into domain files)
# =============================================================================
set(GAME_GLOBALS_SOURCES
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/globals/player_globals.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/globals/timer_hud_globals.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/globals/score_globals.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/globals/match_globals.c
    ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/globals/combo_stage_globals.c
)

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
    mocks_globals.c
)

add_unit_test(test_renderer_interface
    test_renderer_interface.c
    ${PROJECT_SOURCE_DIR}/src/port/sdl/renderer/renderer.c
)

if(ENABLE_NETPLAY)
add_unit_test(test_game_state
    test_game_state.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
    mocks_globals.c
)
target_include_directories(test_game_state PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include ${SDL3_NET_DIR}/include)
target_compile_definitions(test_game_state PRIVATE MOCK_SUPPRESS_CONFLICTS)
target_link_sdl3(test_game_state)

# Font rendering conversion test - isolated algorithm test
add_unit_test(test_font_rendering test_font_rendering.c)

add_unit_test(test_netplay_metrics
    test_netplay_metrics.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_metrics PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_metrics)

add_unit_test(test_netplay_events
    test_netplay_events.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_events PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_events)



add_unit_test(test_netplay_refactor
    test_netplay_refactor.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_refactor PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_refactor)

add_unit_test(test_state_differ
    test_state_differ.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_state_differ PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_state_differ)

add_unit_test(test_effect_state_persistence
    test_effect_state_persistence.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
)
target_compile_definitions(test_effect_state_persistence PRIVATE MOCK_SUPPRESS_CONFLICTS)
target_include_directories(test_effect_state_persistence PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include ${SDL3_NET_DIR}/include)
target_link_sdl3(test_effect_state_persistence)

add_unit_test(test_netplay_oob
    test_netplay_oob.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_oob PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_oob)

add_unit_test(test_netplay_init
    test_netplay_init.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_init PRIVATE DEBUG)
target_compile_definitions(test_netplay_init PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_init)

add_unit_test(test_netplay_catchup
    test_netplay_catchup.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_catchup PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_catchup)

add_unit_test(test_stun
    test_stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
)
target_link_gekkonet_sdl3(test_stun)

add_unit_test(test_netplay_run
    test_netplay_run.c
    mocks_netplay.c
    mocks_globals.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/netplay.c
    ${PROJECT_SOURCE_DIR}/src/netplay/stun.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sdl_net_adapter.c
)
target_compile_definitions(test_netplay_run PRIVATE MOCK_SUPPRESS_CONFLICTS MOCK_HAS_NETPLAY)
target_link_gekkonet_sdl3(test_netplay_run)
endif() # ENABLE_NETPLAY

# -----------------------------------------------------------------------------
# SDL3-only tests (use target_link_sdl3)
# -----------------------------------------------------------------------------

add_unit_test(test_paths
    test_paths.c
    ${PROJECT_SOURCE_DIR}/src/port/config/paths.c
)
target_include_directories(test_paths PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${SDL3_ROOT}/include)
target_link_sdl3(test_paths)

add_unit_test(test_config
    test_config.c
    mocks_paths.c
    ${PROJECT_SOURCE_DIR}/src/port/config/config.c
    ${PROJECT_SOURCE_DIR}/src/port/random.c
)
target_include_directories(test_config PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3(test_config)
if(WIN32)
    target_link_libraries(test_config PRIVATE bcrypt)
endif()

if(ENABLE_NETPLAY)
add_unit_test(test_lobby_server
    test_lobby_server.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sha256.c
    ${PROJECT_SOURCE_DIR}/third_party/cJSON/cJSON.c
)
target_include_directories(test_lobby_server PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${PROJECT_SOURCE_DIR}/src ${SDL3_ROOT}/include ${PROJECT_SOURCE_DIR}/third_party/cJSON)
target_link_sdl3(test_lobby_server)
if(WIN32)
    target_link_libraries(test_lobby_server PRIVATE ws2_32 bcrypt curl)
else()
    target_link_libraries(test_lobby_server PRIVATE curl)
endif()

add_unit_test(test_identity
    test_identity.c
    ${PROJECT_SOURCE_DIR}/src/netplay/sha256.c
    ${PROJECT_SOURCE_DIR}/src/port/random.c
)
target_include_directories(test_identity PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${PROJECT_SOURCE_DIR}/src ${SDL3_ROOT}/include)
target_link_sdl3(test_identity)
if(WIN32)
    target_link_libraries(test_identity PRIVATE bcrypt)
endif()

add_unit_test(test_bracket
    test_bracket.c
    ${PROJECT_SOURCE_DIR}/src/netplay/bracket.c
)
target_include_directories(test_bracket PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${PROJECT_SOURCE_DIR}/src)
endif() # ENABLE_NETPLAY

add_unit_test(test_char_data
    test_char_data.c
    ${PROJECT_SOURCE_DIR}/src/arcade/arcade_char_data.c
)
target_include_directories(test_char_data PRIVATE ${PROJECT_SOURCE_DIR}/src/include)

add_unit_test(test_native_save
    test_native_save.c
    ${PROJECT_SOURCE_DIR}/src/port/save/native_save.c
)
target_include_directories(test_native_save PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include ${PROJECT_SOURCE_DIR}/src)
target_link_sdl3(test_native_save)

# -----------------------------------------------------------------------------
# Bezel tests (use target_link_sdl3_glad)
# -----------------------------------------------------------------------------

if(ENABLE_BEZEL)
add_unit_test(test_bezel_assets
    test_bezel_assets.c
    ${PROJECT_SOURCE_DIR}/src/port/rendering/sdl_bezel.c
    ${PROJECT_SOURCE_DIR}/src/port/config/paths.c
    mocks_imgui_wrapper.c
)
target_include_directories(test_bezel_assets PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3_glad(test_bezel_assets)

add_unit_test(test_bezel_layout
    test_bezel_layout.c
    ${PROJECT_SOURCE_DIR}/src/port/rendering/sdl_bezel.c
    ${PROJECT_SOURCE_DIR}/src/port/config/paths.c
    mocks_imgui_wrapper.c
)
target_include_directories(test_bezel_layout PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include)
target_link_sdl3_glad(test_bezel_layout)
endif() # ENABLE_BEZEL

# -----------------------------------------------------------------------------
# Other tests (no platform-specific linking needed)
# -----------------------------------------------------------------------------

add_unit_test(test_globals_access
    test_globals_access.c
    ${GAME_GLOBALS_SOURCES}
)
target_include_directories(test_globals_access PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(ENABLE_NETPLAY)
add_unit_test(test_game_state_roundtrip
    test_game_state_roundtrip.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
    mocks_globals.c
)
target_include_directories(test_game_state_roundtrip PRIVATE ${PROJECT_SOURCE_DIR}/include ${SDL3_ROOT}/include ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include ${SDL3_NET_DIR}/include)
target_compile_definitions(test_game_state_roundtrip PRIVATE MOCK_SUPPRESS_CONFLICTS)
target_link_sdl3(test_game_state_roundtrip)

add_unit_test(test_state_snapshot
    test_state_snapshot.c
    ${GAME_GLOBALS_SOURCES}
    ${PROJECT_SOURCE_DIR}/src/netplay/game_state.c
    ${PROJECT_SOURCE_DIR}/src/netplay/state_snapshot.c
    mocks_globals.c
)
target_include_directories(test_state_snapshot PRIVATE ${PROJECT_SOURCE_DIR}/include ${PROJECT_SOURCE_DIR}/src/netplay ${SDL3_ROOT}/include ${THIRD_PARTY_DIR}/GekkoNet/GekkoLib/include ${SDL3_NET_DIR}/include)
target_compile_definitions(test_state_snapshot PRIVATE MOCK_SUPPRESS_CONFLICTS)
target_link_sdl3(test_state_snapshot)
endif() # ENABLE_NETPLAY

if(ENABLE_BROADCAST)
add_unit_test(test_broadcast_config test_broadcast_config.c)
target_include_directories(test_broadcast_config PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_cli test_cli.c ${PROJECT_SOURCE_DIR}/src/port/config/cli_parser.c)
target_include_directories(test_cli PRIVATE ${PROJECT_SOURCE_DIR}/include)

if(WIN32)
    add_unit_test(test_broadcast_win32 test_broadcast_win32.cpp ${PROJECT_SOURCE_DIR}/src/port/win32/broadcast_spout.cpp)
    target_include_directories(test_broadcast_win32 PRIVATE 
        ${PROJECT_SOURCE_DIR}/include
        ${THIRD_PARTY_DIR}/Spout2/SPOUTSDK/SpoutGL
    )
    target_link_libraries(test_broadcast_win32 PRIVATE Spout_static)
    target_compile_features(test_broadcast_win32 PRIVATE cxx_std_17)
    set_target_properties(test_broadcast_win32 PROPERTIES LINKER_LANGUAGE CXX)
    set_source_files_properties(test_broadcast_win32.cpp ${PROJECT_SOURCE_DIR}/src/port/win32/broadcast_spout.cpp PROPERTIES COMPILE_FLAGS "-Wno-error")
endif()
endif() # ENABLE_BROADCAST

add_unit_test(test_menu_bridge
    test_menu_bridge.c
    ${PROJECT_SOURCE_DIR}/src/port/menu_bridge.c
)
target_include_directories(test_menu_bridge PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${SDL3_ROOT}/include)
target_link_sdl3(test_menu_bridge)

add_unit_test(test_menu_screen
    test_menu_screen.c
    ${PROJECT_SOURCE_DIR}/src/port/menu_screen_registry.c
)
target_include_directories(test_menu_screen PRIVATE ${PROJECT_SOURCE_DIR}/src/include ${SDL3_ROOT}/include)
target_link_sdl3(test_menu_screen)

add_unit_test(test_trials test_trials.c ${PROJECT_SOURCE_DIR}/src/sf33rd/Source/Game/training/trials.c mocks_globals.c)
target_include_directories(test_trials PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_radix_sort test_radix_sort.c)
target_include_directories(test_radix_sort PRIVATE ${PROJECT_SOURCE_DIR}/src)

add_unit_test(test_legacy_matrix
    test_legacy_matrix.c
    ${PROJECT_SOURCE_DIR}/src/port/rendering/legacy_matrix.c
)
target_include_directories(test_legacy_matrix PRIVATE ${PROJECT_SOURCE_DIR}/include)

add_unit_test(test_adx_decoder
    test_adx_decoder.c
    ${PROJECT_SOURCE_DIR}/src/port/sound/adx_decoder.c
)
target_include_directories(test_adx_decoder PRIVATE ${PROJECT_SOURCE_DIR}/src)
target_link_libraries(test_adx_decoder PRIVATE m)

if(ENABLE_MODS)
add_unit_test(test_stage_config
    test_stage_config.c
    mocks_stage_config.c
    ${PROJECT_SOURCE_DIR}/src/port/mods/stage_config.c
)
target_include_directories(test_stage_config PRIVATE
    ${PROJECT_SOURCE_DIR}/include
    ${PROJECT_SOURCE_DIR}/src
)
endif() # ENABLE_MODS

add_unit_test(test_afs_validation test_afs_validation.c)
target_include_directories(test_afs_validation PRIVATE ${PROJECT_SOURCE_DIR}/src)

add_unit_test(test_glslp_parser test_glslp_parser.c)
target_include_directories(test_glslp_parser PRIVATE ${PROJECT_SOURCE_DIR}/src ${SDL3_ROOT}/include)
target_link_sdl3(test_glslp_parser)
add_unit_test(test_task_api test_task_api.c ${PROJECT_SOURCE_DIR}/src/port/task_api.c mocks_globals.c)
target_include_directories(test_task_api PRIVATE ${PROJECT_SOURCE_DIR}/src/include)
