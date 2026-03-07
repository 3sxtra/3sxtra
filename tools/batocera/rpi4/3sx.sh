#!/usr/bin/env bash
# 3sx.sh - Launcher for 3SX on Batocera
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"

# RPi4 V3D driver supports GL 3.3 features but only advertises GL 3.1.
# Override so Mesa accepts #version 330 shaders (needed for --renderer gl fallback).
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330

# Use OpenGL — profiling shows GL render pipe is ~4ms vs SDL_GPU's 13ms
# driver stall in SDL_SubmitGPUCommandBuffer on VideoCore VI.
# Override with: --renderer gpu  or  --renderer sdl2d
exec "$SCRIPT_DIR/3sx" --renderer gl --ui rmlui "$@"
