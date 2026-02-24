import unittest
from unittest.mock import patch
import sys
from pathlib import Path

# Add project root to sys.path
sys.path.append(str(Path(__file__).parent.parent.parent))

from tools.util.bridge_state import INPUT_START, INPUT_LK, INPUT_RIGHT


class MockState:
    def __init__(self):
        self.nav_G_No = [0, 0, 0, 0]
        self.nav_Cursor_Char = [0, 0]
        self.menu_input_active = 0
        self.p1_input = 0
        self.p2_input = 0
        self.history = []
        self.call_count = 0

    def __setattr__(self, name, value):
        if name == "p1_input" and value != 0:
            self.__dict__.setdefault("history", []).append(value)

        # Simulate feedback during select_character
        if name == "p1_input" and value == INPUT_RIGHT:
            self.__dict__["call_count"] = self.__dict__.get("call_count", 0) + 1
            if self.__dict__["call_count"] >= 2:
                self.nav_Cursor_Char[0] = 3  # Found!

        super().__setattr__(name, value)


class TestNavigatorLogic(unittest.TestCase):
    def setUp(self):
        self.state = MockState()
        from tools.util.navigator import MenuNavigator

        self.nav = MenuNavigator(self.state)

    @patch("time.sleep", return_value=None)
    def test_press_start_on_title(self, mock_sleep):
        """Verify that on Title Screen, it sends START."""
        # TITLE_SCREEN (1, 1)
        self.state.nav_G_No = [1, 1, 0, 0]

        # We need to mock wait_for_screen_change because it loops
        with patch.object(self.nav, "wait_for_screen_change", return_value=True):
            self.nav.process_step()

        # Verify START was sent
        self.assertIn(INPUT_START, self.state.history)
        self.assertEqual(self.state.menu_input_active, 1)

    @patch("time.sleep", return_value=None)
    def test_select_character_feedback(self, mock_sleep):
        """Verify that select_character sends inputs until feedback matches."""
        # Initial cursor ID 0, Target 3
        self.state.nav_Cursor_Char = [0, 0]

        success = self.nav.select_character(0, 3)

        self.assertTrue(success)
        # Should have sent RIGHT at least twice (per my MockState logic)
        self.assertEqual(self.state.history.count(INPUT_RIGHT), 2)
        # Should have sent LK to confirm
        self.assertIn(INPUT_LK, self.state.history)


if __name__ == "__main__":
    unittest.main()
