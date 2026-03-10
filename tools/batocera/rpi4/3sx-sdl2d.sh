#!/usr/bin/env bash
# 3sx-sdl2d.sh - Launch 3SX with SDL 2D software renderer
#
# Pure SDL2 software rendering path. CPU-bound — no GPU acceleration.
# Useful as a fallback when GL/GPU drivers are unavailable or broken.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"

exec "$SCRIPT_DIR/3sx" --renderer classic "$@"
