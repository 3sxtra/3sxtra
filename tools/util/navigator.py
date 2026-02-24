import time
from .bridge_state import GameScreen, INPUT_START, INPUT_LK, INPUT_RIGHT, INPUT_UP


class MenuNavigator:
    """
    Closed-loop menu automation for 3SX.
    Reads game state from shared memory and injects inputs.
    Utilizes the 'Check-Before-Move' pattern for reliable navigation.
    """

    def __init__(self, state):
        self.state = state
        self.input_hold_s = 0.05  # 50ms - safer hold duration
        self.input_cooldown_s = 0.1  # 100ms - delay between inputs
        self.settle_time_s = 0.5  # 500ms - wait after screen change

    def get_current_screen(self):
        """Identify current game screen from G_No state machine."""
        return GameScreen.from_gno(self.state.nav_G_No[0], self.state.nav_G_No[1])

    def send_p1_input(self, bitmask):
        """Inject a P1 input bitmask for a controlled duration."""
        self.state.menu_input_active = 1
        self.state.p1_input = bitmask
        time.sleep(self.input_hold_s)
        self.state.p1_input = 0
        time.sleep(self.input_cooldown_s)

    def send_p2_input(self, bitmask):
        """Inject a P2 input bitmask for a controlled duration."""
        self.state.menu_input_active = 1
        self.state.p2_input = bitmask
        time.sleep(self.input_hold_s)
        self.state.p2_input = 0
        time.sleep(self.input_cooldown_s)

    def wait_for_screen_change(self, from_screen, timeout_s=5.0):
        """Block until the screen changes from the current state."""
        start = time.time()
        while time.time() - start < timeout_s:
            if self.get_current_screen() != from_screen:
                time.sleep(self.settle_time_s)
                return True
            time.sleep(0.05)
        return False

    def navigate_title_to_menu(self):
        """Handle Title Screen -> Main Menu transition."""
        print("[Navigator] Attempting to reach Main Menu...")

        start_time = time.time()
        while time.time() - start_time < 15.0:
            current = self.get_current_screen()

            if current == GameScreen.MAIN_MENU:
                print("[Navigator] Main Menu reached!")
                return True

            if current == GameScreen.WAIT_AUTO_LOAD:
                # Initial boot loading — just wait
                time.sleep(0.5)
            elif current in (
                GameScreen.TITLE_SCREEN,
                GameScreen.BOOT_SPLASH,
                GameScreen.TITLE_TRANSITION,
                GameScreen.PRE_FIGHT_INIT,
            ):
                print(f"[Navigator] At {current}. Pressing START...")
                self.send_p1_input(INPUT_START)
                time.sleep(1.0)  # Wait for animation
            elif current == GameScreen.UNKNOWN:
                # Transitional state — wait for it to settle
                print(
                    f"[Navigator] At UNKNOWN (G_No: {self.state.nav_G_No[0]}, {self.state.nav_G_No[1]}). Waiting..."
                )
                time.sleep(0.5)
            else:
                print(
                    f"[Navigator] At {current} (G_No: {self.state.nav_G_No[0]}, {self.state.nav_G_No[1]}). Waiting..."
                )
                time.sleep(0.5)

        print("[Navigator] Failed to reach Main Menu.")
        return False

    def navigate_main_menu_to_network(self):
        """
        Navigate Main Menu to NETWORK option.
        From default cursor position, NETWORK is 3 UP.
        """
        print("[Navigator] At Main Menu. Navigating to NETWORK (3 UP)...")
        for _ in range(3):
            self.send_p1_input(INPUT_UP)

        self.send_p1_input(INPUT_LK)  # Confirm NETWORK
        return self.wait_for_screen_change(GameScreen.MAIN_MENU)

    def select_character(self, player_idx, target_char_id):
        """
        Navigate character selection grid using real-time feedback.
        Uses 'Check-Before-Move' to avoid overshooting.
        Returns True if character was selected and confirmed.
        """
        print(
            f"[Navigator] Player {player_idx + 1} selecting character ID {target_char_id}..."
        )

        MAX_ATTEMPTS = 40  # Total character count ~20, allow 2 loops

        for _attempt in range(MAX_ATTEMPTS):
            current_char = self.state.nav_Cursor_Char[player_idx]

            if current_char == target_char_id:
                print(f"[Navigator] P{player_idx + 1} cursor on target! Confirming...")
                if player_idx == 0:
                    self.send_p1_input(INPUT_LK)
                else:
                    self.send_p2_input(INPUT_LK)

                # Wait and verify selection was registered
                time.sleep(0.2)
                if self._wait_for_char_confirm(player_idx, target_char_id):
                    print(
                        f"[Navigator] P{player_idx + 1} character {target_char_id} confirmed!"
                    )
                    return True
                print(
                    f"[Navigator] P{player_idx + 1} confirmation not detected, retrying..."
                )
                continue

            # Not found yet, move cursor (looping right)
            if player_idx == 0:
                self.send_p1_input(INPUT_RIGHT)
            else:
                self.send_p2_input(INPUT_RIGHT)

        print(f"[Navigator] FAILED to find character {target_char_id}")
        return False

    def _wait_for_char_confirm(self, player_idx, expected_char_id, timeout_s=1.0):
        """Wait for nav_My_char to reflect the confirmed character selection."""
        start = time.time()
        while time.time() - start < timeout_s:
            confirmed_char = self.state.nav_My_char[player_idx]
            if confirmed_char == expected_char_id:
                return True
            time.sleep(0.05)
        return False

    def select_super_art(self, player_idx, _target_sa_id):
        """Select specific Super Art (1, 2, or 3)."""
        # Simplification: Wait for sub-state change or just press LK
        # 3rd Strike confirms SA with LK.
        # Navigation is usually UP/DOWN.
        # For now, just confirming default is a good start.
        print(f"[Navigator] P{player_idx + 1} confirming Super Art...")
        if player_idx == 0:
            self.send_p1_input(INPUT_LK)
        else:
            self.send_p2_input(INPUT_LK)
        return True

    def release_control(self):
        """Disable input injection, returning control to the user."""
        self.state.menu_input_active = 0

    def process_step(self):
        """Identify current state and perform one logical navigation action."""
        screen = self.get_current_screen()

        if screen == GameScreen.BOOT_SPLASH:
            print("[Navigator] At Boot/Splash. Pressing START to skip...")
            self.send_p1_input(INPUT_START)
            self.wait_for_screen_change(GameScreen.BOOT_SPLASH)
        elif screen == GameScreen.TITLE_SCREEN:
            self.navigate_title_to_menu()
        elif screen == GameScreen.MAIN_MENU:
            # Default action: Enter NETWORK
            self.navigate_main_menu_to_network()
        elif screen == GameScreen.PLAYER_ENTRY:
            # Network player entry screen - press START to join
            print("[Navigator] At Player Entry. Pressing START to join...")
            self.send_p1_input(INPUT_START)
            time.sleep(0.5)
        elif screen == GameScreen.CHARACTER_SELECT:
            # This is complex as it requires coordinated P1/P2 selection
            # and sub-state tracking. Handled by higher level runner.
            pass
        elif screen == GameScreen.GAMEPLAY:
            # We are done!
            self.state.menu_input_active = 0
            return True

        return False
