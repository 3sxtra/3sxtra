"""Validate that all Phase 3 RCSS files have correct body sizing and no oversized panels."""

import re
from pathlib import Path

import pytest

UI_DIR = Path(__file__).resolve().parents[2] / "assets" / "ui"

# Phase 3 game-context RCSS files (must use 384dp × 224dp body)
PHASE3_RCSS = [
    "game_hud.rcss",
    "mode_menu.rcss",
    "option_menu.rcss",
    "game_option.rcss",
    "title.rcss",
    "continue.rcss",
    "win.rcss",
    "gameover.rcss",
    "char_select.rcss",
    "vs_screen.rcss",
    "vs_result.rcss",
    "pause.rcss",
    "copyright.rcss",
    "name_entry.rcss",
    "trials_hud.rcss",
    "attract_overlay.rcss",
    "replay_picker.rcss",
    "exit_confirm.rcss",
    "sound_menu.rcss",
    "button_config.rcss",
    "extra_option.rcss",
    "memory_card.rcss",
    "training_mode.rcss",
]

# Viewport dimensions for the CPS3 game context
VIEWPORT_W = 384
VIEWPORT_H = 224


def _parse_dp(value: str) -> float | None:
    """Extract numeric dp value from a string like '400dp'."""
    m = re.match(r"([\d.]+)dp", value.strip())
    return float(m.group(1)) if m else None


def _read_css(name: str) -> str:
    path = UI_DIR / name
    assert path.exists(), f"Missing RCSS file: {path}"
    return path.read_text(encoding="utf-8")


@pytest.mark.parametrize("rcss_file", PHASE3_RCSS)
def test_body_uses_fixed_dp(rcss_file: str):
    """Phase 3 RCSS body should use 384dp × 224dp, not 100%."""
    css = _read_css(rcss_file)

    # Find all body rules
    body_blocks = re.findall(r"(?:^|\n)\s*body\s*\{([^}]*)\}", css, re.DOTALL)
    assert body_blocks, f"{rcss_file}: no body rule found"

    for block in body_blocks:
        if "100%" in block:
            pytest.fail(
                f"{rcss_file}: body uses percentage sizing (100%); "
                f"Phase 3 screens must use 384dp × 224dp"
            )


@pytest.mark.parametrize("rcss_file", PHASE3_RCSS)
def test_no_oversized_width(rcss_file: str):
    """No element should have a fixed width exceeding 384dp."""
    css = _read_css(rcss_file)

    # Match width declarations like "width: 400dp;"
    for match in re.finditer(r"width\s*:\s*([\d.]+dp)\s*;", css):
        val = _parse_dp(match.group(1))
        if val is not None and val > VIEWPORT_W:
            pytest.fail(
                f"{rcss_file}: width {val}dp exceeds viewport "
                f"({VIEWPORT_W}dp) — {match.group(0)}"
            )
