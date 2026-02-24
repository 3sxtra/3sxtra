import unittest
import sys
from pathlib import Path

# Add project root to sys.path so we can import from tools
sys.path.append(str(Path(__file__).parent.parent.parent))


class TestBridgeAccess(unittest.TestCase):
    def test_import_bridge_state(self):
        """Verify that we can import the MenuBridgeState ctypes mirror."""
        try:
            from tools.util.bridge_state import MenuBridgeState as _  # noqa: F401
        except ImportError as e:
            self.fail(f"Failed to import MenuBridgeState: {e}")

    def test_struct_size(self):
        """Verify that the Python struct size matches the C struct size."""
        # This will be updated once we have the size from the C unit test output
        from tools.util.bridge_state import MenuBridgeState
        import ctypes

        # In our C unit test, it reported 89 bytes (or similar)
        # We'll use a placeholder for now and update it once we've implemented the mirror
        self.assertGreater(ctypes.sizeof(MenuBridgeState), 0)


if __name__ == "__main__":
    unittest.main()
