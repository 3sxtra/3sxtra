#!/usr/bin/env bash
# 3sx-gl.sh - Launch 3SX with OpenGL renderer (recommended for RPi4)
#
# OpenGL is the fastest backend on VideoCore VI (~4ms render pipe).
# Requires Mesa GL version override for #version 330 shaders.

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/lib:$LD_LIBRARY_PATH"

# RPi4 V3D driver supports GL 3.3 features but only advertises GL 3.1.
export MESA_GL_VERSION_OVERRIDE=3.3
export MESA_GLSL_VERSION_OVERRIDE=330

exec "$SCRIPT_DIR/3sx" --renderer gl "$@"
