#!/usr/bin/env bash
# sync-deploy.sh — Incremental deploy to RPi4 via rsync
#
# Rsyncs each component directly from its source location to the Pi.
# No staging directory, no tarball — only changed files are transferred.
#
# Usage:
#   bash tools/batocera/rpi4/sync-deploy.sh [PI_IP]
#
# Examples:
#   bash tools/batocera/rpi4/sync-deploy.sh                  # default IP
#   bash tools/batocera/rpi4/sync-deploy.sh 192.168.1.100    # custom IP
set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────
PI_IP="${1:-192.168.86.44}"
PI_USER="root"
PI_PASS="linux"
PI_DEST="/userdata/roms/ports/3sx"

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
THIRD_PARTY="$ROOT_DIR/third_party_rpi4"
BUILD_DIR="$ROOT_DIR/build_rpi4"
SCRIPT_DIR="$ROOT_DIR/tools/batocera/rpi4"

# ── Helpers ────────────────────────────────────────────────────
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m'

step() { echo -e "\n${GREEN}▶ $1${NC}"; }
info() { echo -e "${CYAN}  $1${NC}"; }
warn() { echo -e "${YELLOW}⚠ $1${NC}"; }
die()  { echo -e "${RED}✖ $1${NC}" >&2; exit 1; }

export SSHPASS="$PI_PASS"
RSYNC_RSH="sshpass -e ssh -o StrictHostKeyChecking=no"
SSH_CMD="sshpass -e ssh -o StrictHostKeyChecking=no $PI_USER@$PI_IP"
RSYNC="rsync -avz --checksum -e '$RSYNC_RSH'"

do_rsync() {
    local src="$1" dst="$2"
    rsync -avz --checksum -e "$RSYNC_RSH" "$src" "$PI_USER@$PI_IP:$dst"
}

# ── Pre-flight ────────────────────────────────────────────────
command -v sshpass >/dev/null || die "sshpass not found. Run: sudo apt install sshpass"
command -v rsync   >/dev/null || die "rsync not found. Run: sudo apt install rsync"
[ -f "$BUILD_DIR/3sx" ]      || die "Binary not found at $BUILD_DIR/3sx — build first."

echo ""
echo -e "${CYAN}┌─────────────────────────────────────────┐${NC}"
echo -e "${CYAN}│  3SX RPi4 Incremental Sync Deploy       │${NC}"
echo -e "${CYAN}├─────────────────────────────────────────┤${NC}"
echo -e "${CYAN}│${NC}  Target:   ${YELLOW}$PI_USER@$PI_IP${NC}"
echo -e "${CYAN}│${NC}  Dest:     ${YELLOW}$PI_DEST${NC}"
echo -e "${CYAN}└─────────────────────────────────────────┘${NC}"

# ── Step 1: Check Pi is reachable ─────────────────────────────
step "Checking Pi4 at $PI_IP..."
ping -c 1 -W 2 "$PI_IP" >/dev/null 2>&1 || die "Pi4 not reachable at $PI_IP"
echo -e "${GREEN}✔ Pi4 is reachable${NC}"

# ── Step 2: Stop running 3sx on Pi ────────────────────────────
step "Stopping 3sx on Pi4 (if running)..."
$SSH_CMD "killall -9 3sx 2>/dev/null; sleep 0.5; true"
echo -e "${GREEN}✔ Process stopped (or wasn't running)${NC}"

# ── Step 3: Sync binary ──────────────────────────────────────
step "Syncing binary..."
do_rsync "$BUILD_DIR/3sx" "$PI_DEST/"

# ── Step 4: Sync shared libraries ─────────────────────────────
step "Syncing libraries..."
# Collect .so files into a temp list, rsync from their locations
$SSH_CMD "mkdir -p $PI_DEST/lib"
find "$THIRD_PARTY" -name "*.so*" -print0 | \
    rsync -avz --checksum --from0 --files-from=- -e "$RSYNC_RSH" \
    "/" "$PI_USER@$PI_IP:$PI_DEST/lib/" \
    --no-relative --no-dirs

# ── Step 5: Sync assets ──────────────────────────────────────
step "Syncing assets..."
do_rsync "$ROOT_DIR/assets/" "$PI_DEST/assets/"

# ── Step 6: Sync shaders ─────────────────────────────────────
step "Syncing shaders..."
do_rsync "$ROOT_DIR/src/shaders/" "$PI_DEST/shaders/"

# ── Step 7: Sync libretro shader presets ──────────────────────
if [ -d "$THIRD_PARTY/slang-shaders" ]; then
    step "Syncing libretro shaders..."
    $SSH_CMD "mkdir -p $PI_DEST/shaders/libretro"
    do_rsync "$THIRD_PARTY/slang-shaders/" "$PI_DEST/shaders/libretro/"
fi

# ── Step 8: Sync launcher scripts ────────────────────────────
step "Syncing launcher scripts..."
for launcher in 3sx.sh 3sx-gl.sh 3sx-gpu.sh 3sx-sdl2d.sh; do
    do_rsync "$SCRIPT_DIR/$launcher" "$PI_DEST/"
done

# ── Step 9: Fix permissions ──────────────────────────────────
step "Setting permissions on Pi4..."
$SSH_CMD "chmod +x $PI_DEST/3sx $PI_DEST/3sx.sh $PI_DEST/3sx-gl.sh $PI_DEST/3sx-gpu.sh $PI_DEST/3sx-sdl2d.sh"
echo -e "${GREEN}✔ Permissions set${NC}"

# ── Done ──────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}  ✔ 3SX synced to Pi4 ($PI_IP)${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""
echo "Launch on Pi:  ssh root@$PI_IP 'cd $PI_DEST && ./3sx.sh'"
