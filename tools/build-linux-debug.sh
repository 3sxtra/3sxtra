#!/usr/bin/env bash
# ──────────────────────────────────────────────────────────────────
# build-linux-debug.sh
# Build 3SX (Debug, Linux x86_64) and package a shareable zip.
#
# Uses third_party_linux/ so that Linux-built deps don't conflict
# with Windows-built deps in third_party/.
#
# Usage:
#   ./tools/build-linux-debug.sh               # build + package
#   ./tools/build-linux-debug.sh --install-deps # also install system packages
# ──────────────────────────────────────────────────────────────────
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build-linux"
THIRD_PARTY="$ROOT_DIR/third_party_linux"
ARTIFACT_NAME="3sx-linux-x86_64-debug"
ZIP_PATH="$ROOT_DIR/${ARTIFACT_NAME}.zip"

export CC=clang
export CXX=clang++

# ─── Optional: install system dependencies ────────────────────────
if [[ "${1:-}" == "--install-deps" ]]; then
    echo "==> Installing system build dependencies..."
    sudo apt-get update
    sudo apt-get install -y $(cat "$ROOT_DIR/tools/requirements-ubuntu-sdl3.txt")
    sudo apt-get install -y clang curl python3-jinja2 zip
    shift
fi

# ─── Check toolchain ──────────────────────────────────────────────
for tool in cmake clang clang++ cargo; do
    if ! command -v "$tool" &>/dev/null; then
        echo "ERROR: $tool not found. Install it or run with --install-deps."
        exit 1
    fi
done

# ─── Build third-party dependencies into third_party_linux/ ──────
echo "==> Building third-party dependencies into $THIRD_PARTY..."
export THIRD_PARTY
bash "$ROOT_DIR/build-deps.sh"

# ─── Configure CMake ──────────────────────────────────────────────
echo "==> Configuring CMake (Debug, tests disabled)..."
cmake -B "$BUILD_DIR" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DTHIRD_PARTY_DIR="$THIRD_PARTY" \
    -DENABLE_TESTS=OFF

# ─── Build ────────────────────────────────────────────────────────
echo "==> Building 3SX..."
cmake --build "$BUILD_DIR" --parallel "$(nproc)"

# ─── Package ──────────────────────────────────────────────────────
echo "==> Packaging $ARTIFACT_NAME..."
PKG="$ROOT_DIR/pkg-linux"
rm -rf "$PKG"
mkdir -p "$PKG/bin" "$PKG/lib"

# Binary
cp "$BUILD_DIR/3sx" "$PKG/bin/"
chmod +x "$PKG/bin/3sx"

# Fix RPATH so the binary finds bundled libs at ../lib relative to itself.
# The build binary has absolute paths baked in; we need $ORIGIN/../lib instead.
if command -v patchelf &>/dev/null; then
    patchelf --set-rpath '$ORIGIN/../lib' "$PKG/bin/3sx"
    echo "    RPATH patched to \$ORIGIN/../lib"
else
    echo "    WARNING: patchelf not found — relying on launcher LD_LIBRARY_PATH"
fi

# Shared libraries — auto-bundle ALL non-system .so deps from ldd
# This catches SDL3, SDL3_image, spirv-cross, and any future additions.
echo "    Bundling shared libraries..."
ldd "$BUILD_DIR/3sx" | awk '/=>/ {print $3}' | while read -r lib; do
    case "$lib" in
        /lib/*|/usr/lib/*) ;;  # skip system libs
        *) echo "      + $(basename "$lib")"
           cp -L "$lib" "$PKG/lib/" ;;
    esac
done

# Assets (fonts, bezels, controller images, stage mods)
cp -r "$ROOT_DIR/assets" "$PKG/bin/"

# Shaders (compiled SPIR-V + GLSL sources from the build)
cp -r "$BUILD_DIR/shaders" "$PKG/bin/"

# Libretro slang-shader presets
cp -r "$THIRD_PARTY/slang-shaders" "$PKG/bin/shaders/libretro"

# Launcher script — sets LD_LIBRARY_PATH so the binary finds bundled .so files
cat > "$PKG/3sx.sh" << 'LAUNCHER'
#!/bin/sh
# 3SX Launcher — finds the binary and libs relative to this script
DIR="$(cd "$(dirname "$0")" && pwd)"
export LD_LIBRARY_PATH="$DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
cd "$DIR" && exec ./bin/3sx "$@"
LAUNCHER
chmod +x "$PKG/3sx.sh"

# Create zip
rm -f "$ZIP_PATH"
cd "$PKG"
zip -r "$ZIP_PATH" .
cd "$ROOT_DIR"

# Clean up staging dir
rm -rf "$PKG"

echo ""
echo "════════════════════════════════════════════════════════"
echo "  ✅  Debug build packaged: $ZIP_PATH"
echo "════════════════════════════════════════════════════════"
