import ctypes
import mmap
from enum import Enum


class MenuBridgeState(ctypes.Structure):
    """
    ctypes mirror of the MenuBridgeState struct defined in include/menu_bridge.h.
    Must use 1-byte packing to match the C side.
    """

    _pack_ = 1
    _fields_ = [
        # === NAVIGATION STATE ===
        ("nav_G_No", ctypes.c_uint8 * 4),  # [major, sub1, sub2, sub3]
        ("nav_S_No", ctypes.c_uint8 * 4),  # Selection sub-state
        ("nav_Play_Type", ctypes.c_uint8),  # 0=Arcade, 1=Versus, 2=Training
        ("nav_Play_Game", ctypes.c_uint8),  # 0=menus, 1-2=gameplay
        ("nav_My_char", ctypes.c_uint8 * 2),  # Selected characters [P1, P2]
        ("nav_Super_Arts", ctypes.c_uint8 * 2),  # Selected super arts [P1, P2]
        # Real-time cursor feedback
        ("nav_Cursor_X", ctypes.c_int8 * 2),  # [P1, P2]
        ("nav_Cursor_Y", ctypes.c_int8 * 2),  # [P1, P2]
        ("nav_Cursor_Char", ctypes.c_int8 * 2),  # Char ID under cursor [P1, P2]
        # Control flags
        ("menu_input_active", ctypes.c_uint8),  # 1=Python controls inputs
        # Input buffers (injected when menu_input_active=1)
        ("p1_input", ctypes.c_uint16),
        ("p2_input", ctypes.c_uint16),
        # Frame counter (for external tool sync)
        ("frame_count", ctypes.c_uint32),
        # Combat-active flag (1 when round is active, players can move)
        ("allow_battle", ctypes.c_uint8),
        # SA cursor position (0=SA1, 1=SA2, 2=SA3)
        ("nav_Cursor_SA", ctypes.c_int8 * 2),
        # Screen sub-state (for FIGHT banner detection: C_No[0]==1, C_No[1]==4)
        ("nav_C_No", ctypes.c_uint8 * 4),
        # Reserved for future expansion (alignment padding)
        ("_reserved", ctypes.c_uint8 * 53),
    ]


class GameScreen(Enum):
    WAIT_AUTO_LOAD = (0, 0)  # Initial boot loading (G_No[0]=0)
    BOOT_SPLASH = (1, 0)  # Loop_Demo sub 0
    TITLE_SCREEN = (1, 1)  # Loop_Demo sub 1
    TITLE_TRANSITION = (1, 2)  # Loop_Demo sub 2: title-to-menu fade
    PRE_FIGHT_INIT = (2, 0)  # Game2_0: pre-fight setup
    CHARACTER_SELECT = (2, 1)  # Game01: character select
    GAMEPLAY = (2, 2)  # Game02: active fight
    PLAYER_ENTRY = (2, 5)  # Game05: network player entry
    MAIN_MENU = (2, 12)  # Game12: main menu
    UNKNOWN = (-1, -1)

    @classmethod
    def from_gno(cls, g0, g1):
        for screen in cls:
            if screen.value == (g0, g1):
                return screen
        return cls.UNKNOWN


# Shared Memory Constants
MENU_BRIDGE_SHM_NAME = "3SX_MENU_BRIDGE_SHM"


def connect_to_bridge(suffix=None):
    """
    Connect to the 3SX Menu Bridge Shared Memory.
    Returns (state, shm_handle).
    """
    size = ctypes.sizeof(MenuBridgeState)

    name = MENU_BRIDGE_SHM_NAME
    if suffix:
        name = f"{MENU_BRIDGE_SHM_NAME}_{suffix}"

    try:
        # On Windows, mmap.mmap(-1, size, tagname=...) connects to Named Shared Memory
        shm = mmap.mmap(-1, size, tagname=name)
        state = MenuBridgeState.from_buffer(shm)
        return state, shm
    except FileNotFoundError:
        # Shared memory hasn't been created by the game yet
        return None, None
    except Exception as e:
        print(f"[MenuBridge] Error connecting: {e}")
        return None, None


# Input Masks (matching include/menu_bridge.h)
INPUT_UP = 1 << 0
INPUT_DOWN = 1 << 1
INPUT_LEFT = 1 << 2
INPUT_RIGHT = 1 << 3
INPUT_LP = 1 << 4
INPUT_MP = 1 << 5
INPUT_HP = 1 << 6
INPUT_LK = 1 << 8
INPUT_MK = 1 << 9
INPUT_HK = 1 << 10
INPUT_START = 1 << 14
INPUT_SELECT = 1 << 15
