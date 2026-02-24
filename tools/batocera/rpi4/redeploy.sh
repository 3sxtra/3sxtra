#!/usr/bin/env bash
# redeploy.sh — Retry upload+extract without rebuilding
#
# Skips build & package steps; reuses the existing game_deployment.tar.gz.
# Stops the running 3sx process on the Pi before extracting to avoid
# "Text file busy" errors.
#
# Usage:
#   bash tools/batocera/rpi4/redeploy.sh [PI_IP]
#
# Examples:
#   bash tools/batocera/rpi4/redeploy.sh                  # default IP
#   bash tools/batocera/rpi4/redeploy.sh 192.168.1.100    # custom IP
set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────
PI_IP="${1:-192.168.86.44}"
PI_USER="root"
PI_PASS="linux"
PI_DEST="/userdata/roms/ports/3sx"

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
TARBALL="$ROOT_DIR/game_deployment.tar.gz"

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

SSH_CMD="sshpass -p $PI_PASS ssh -o StrictHostKeyChecking=no $PI_USER@$PI_IP"
SCP_CMD="sshpass -p $PI_PASS scp -o StrictHostKeyChecking=no"

# ── Pre-flight ────────────────────────────────────────────────
command -v sshpass >/dev/null || die "sshpass not found. Run: sudo apt install sshpass"
[ -f "$TARBALL" ] || die "No tarball found at $TARBALL — run a full build first."

SIZE=$(du -h "$TARBALL" | cut -f1)
echo ""
echo -e "${CYAN}┌─────────────────────────────────────────┐${NC}"
echo -e "${CYAN}│  3SX RPi4 Re-deploy (skip build)        │${NC}"
echo -e "${CYAN}├─────────────────────────────────────────┤${NC}"
echo -e "${CYAN}│${NC}  Package:  ${YELLOW}$SIZE${NC}"
echo -e "${CYAN}│${NC}  Target:   ${YELLOW}$PI_USER@$PI_IP${NC}"
echo -e "${CYAN}└─────────────────────────────────────────┘${NC}"

# ── Step 1: Check Pi is reachable ─────────────────────────────
step "Checking Pi4 at $PI_IP..."
ping -c 1 -W 2 "$PI_IP" >/dev/null 2>&1 || die "Pi4 not reachable at $PI_IP"
echo -e "${GREEN}✔ Pi4 is reachable${NC}"

# ── Step 2: Stop running 3sx on Pi ────────────────────────────
step "Stopping 3sx on Pi4 (if running)..."
$SSH_CMD "killall -9 3sx 2>/dev/null; sleep 0.5; true"
echo -e "${GREEN}✔ Process stopped (or wasn't running)${NC}"

# ── Step 3: Upload ────────────────────────────────────────────
step "Uploading to $PI_USER@$PI_IP:$PI_DEST/..."
$SCP_CMD "$TARBALL" "$PI_USER@$PI_IP:$PI_DEST/" || die "SCP upload failed!"
echo -e "${GREEN}✔ Upload complete${NC}"

# ── Step 4: Extract on Pi ─────────────────────────────────────
step "Extracting on Pi4..."
$SSH_CMD \
    "cd $PI_DEST && tar xzf game_deployment.tar.gz --overwrite && chmod +x 3sx 3sx.sh && rm game_deployment.tar.gz" \
    || die "Remote extraction failed!"
echo -e "${GREEN}✔ Deployed successfully!${NC}"

# ── Done ──────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}  ✔ 3SX re-deployed to Pi4 ($PI_IP)${NC}"
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""
echo "Launch on Pi:  ssh root@$PI_IP 'cd $PI_DEST && ./3sx.sh'"
