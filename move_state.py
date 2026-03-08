import re


def main():
    gs_h_path = "d:/3sxtra/src/include/game_state.h"
    np_c_path = "d:/3sxtra/src/netplay/netplay.c"
    gs_c_path = "d:/3sxtra/src/netplay/game_state.c"
    np_h_path = "d:/3sxtra/src/netplay/netplay.h"

    with open(gs_h_path, "r", encoding="utf-8") as f:
        gs_h = f.read()
    with open(np_c_path, "r", encoding="utf-8") as f:
        np_c = f.read()
    with open(gs_c_path, "r", encoding="utf-8") as f:
        gs_c = f.read()
    with open(np_h_path, "r", encoding="utf-8") as f:
        np_h = f.read()

    # Move structs
    structs_block = ""
    match = re.search(
        r"/\*\*\n \* @brief Effect allocator state.*?} State;\n", np_c, re.DOTALL
    )
    if match:
        structs_block = match.group(0)
        np_c = np_c.replace(structs_block, "")

        if "EffectState" not in gs_h:
            structs_add = (
                '\n#include "sf33rd/Source/Game/effect/effect.h"\n\n' + structs_block
            )
            gs_h = gs_h.replace("} GameState;", "} GameState;" + structs_add)

    # Added to game_state.h
    funcs_txt = """
struct GekkoGameEvent;
int Netplay_GetPlayerHandle(void);
int Netplay_GetBattleStartFrame(void);
void save_state(const struct GekkoGameEvent* event);
void load_state_from_event(const struct GekkoGameEvent* event);
"""
    if "Netplay_GetPlayerHandle" not in gs_h:
        gs_h = gs_h.replace("#endif", funcs_txt + "\n#endif")

    # Added to netplay.h
    if "Netplay_GetPlayerHandle" not in np_h:
        np_h = np_h.replace(
            "int Netplay_GetPlayerNumber(void);",
            "int Netplay_GetPlayerNumber(void);\nint Netplay_GetPlayerHandle(void);\nint Netplay_GetBattleStartFrame(void);",
        )

    # State variables
    state_vars_pattern = r"#if defined\(DEBUG\)\nstatic int battle_start_frame.*?;.*?state_buffer\[STATE_BUFFER_MAX\];\n#endif\n"
    state_vars = ""
    sv_match = re.search(state_vars_pattern, np_c, re.DOTALL)
    if sv_match:
        state_vars = sv_match.group(0)
        np_c = np_c.replace(state_vars, "")

    # Main functions block: line 582 to 984. Let's just find everything from
    # `#if defined(DEBUG)\n// Per-subsystem checksums` to `load_state_from_event(const GekkoGameEvent* event) { ... }`

    start_str = "#if defined(DEBUG)\n// Per-subsystem checksums"
    start_idx = np_c.find(start_str)

    end_str = "} Game GekkoGame"
    # Actually, after `load_state_from_event` is `static bool game_ready_to_run_character_select`
    end_idx = np_c.find("static bool game_ready_to_run_character_select")

    if start_idx != -1 and end_idx != -1:
        extracted = np_c[start_idx:end_idx]
        np_c = np_c[:start_idx] + "\n\n" + np_c[end_idx:]

        # We need to prepend some includes and the static vars to game_state.c
        gs_c_add = ""
        if state_vars:
            gs_c_add += state_vars + "\\n"

        gs_c_add += """
#include "netplay.h"
#define Game GekkoGame
#include "gekkonet.h"
#undef Game
#include "sf33rd/utils/djb2_hash.h"

extern unsigned short g_netplay_port;
"""
        # Replace occurrences
        extracted = extracted.replace("local_port", "g_netplay_port")
        extracted = extracted.replace("player_handle", "Netplay_GetPlayerHandle()")

        # Add accessors back to netplay.c
        accessors = """
int Netplay_GetPlayerHandle(void) {
    return player_handle;
}
int Netplay_GetBattleStartFrame(void) {
#if defined(DEBUG)
    return battle_start_frame;
#else
    return -1;
#endif
}
"""
        np_c += accessors

        # Insert gs_c_add and extracted into game_state.c
        if "#include <SDL3/SDL.h>" in gs_c:
            gs_c = gs_c.replace(
                "#include <SDL3/SDL.h>", "#include <SDL3/SDL.h>\n" + gs_c_add
            )
        else:
            gs_c = gs_c_add + gs_c

        gs_c += "\n" + extracted

    with open(gs_h_path, "w", encoding="utf-8") as f:
        f.write(gs_h)
    with open(np_c_path, "w", encoding="utf-8") as f:
        f.write(np_c)
    with open(gs_c_path, "w", encoding="utf-8") as f:
        f.write(gs_c)
    with open(np_h_path, "w", encoding="utf-8") as f:
        f.write(np_h)


if __name__ == "__main__":
    main()
