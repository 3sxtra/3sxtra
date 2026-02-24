#!/usr/bin/env bash
# deploy.sh - Package the 3SX build into a deployment tarball
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
THIRD_PARTY="$ROOT_DIR/third_party_rpi4"
BUILD_DIR="$ROOT_DIR/build_rpi4"
DEPLOY_DIR="$ROOT_DIR/deploy"

# Clean previous deployment
rm -rf "$DEPLOY_DIR"
mkdir -p "$DEPLOY_DIR/3sx/lib"

# Copy binary
if [ -f "$BUILD_DIR/3sx" ]; then
    cp "$BUILD_DIR/3sx" "$DEPLOY_DIR/3sx/"
else
    echo "ERROR: Binary not found at $BUILD_DIR/3sx"
    exit 1
fi

# Copy shared libraries from third_party builds
find "$THIRD_PARTY" -name "*.so*" -exec cp -P {} "$DEPLOY_DIR/3sx/lib/" \;

# Copy assets
cp -r "$ROOT_DIR/assets" "$DEPLOY_DIR/3sx/"

# Copy shaders
cp -r "$ROOT_DIR/src/shaders" "$DEPLOY_DIR/3sx/"

# Copy libretro shader presets
if [ -d "$THIRD_PARTY/slang-shaders" ]; then
    mkdir -p "$DEPLOY_DIR/3sx/shaders/libretro"
    cp -r "$THIRD_PARTY/slang-shaders/"* "$DEPLOY_DIR/3sx/shaders/libretro/"
fi

# Copy launcher
cp "$ROOT_DIR/tools/batocera/rpi4/3sx.sh" "$DEPLOY_DIR/3sx/"
chmod +x "$DEPLOY_DIR/3sx/3sx.sh"

# Create tarball (files at archive root, no wrapping directory)
cd "$DEPLOY_DIR/3sx"
tar czf "$ROOT_DIR/game_deployment.tar.gz" *

echo "Deployment package created: $ROOT_DIR/game_deployment.tar.gz"
echo "Contents:"
tar tzf "$ROOT_DIR/game_deployment.tar.gz" | head -20 || true
echo "..."
