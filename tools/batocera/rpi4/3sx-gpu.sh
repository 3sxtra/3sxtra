#!/usr/bin/env bash
# 3sx-gpu.sh - Launch 3SX with SDL_GPU renderer
#
# SDL_GPU uses the Vulkan-backed SDL3 GPU API.
# On RPi4 this is significantly slower (~13ms due to driver stalls in
# SDL_SubmitGPUCommandBuffer on VideoCore VI). Use for testing only.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"

exec "$SCRIPT_DIR/3sx" --renderer gpu "$@"
