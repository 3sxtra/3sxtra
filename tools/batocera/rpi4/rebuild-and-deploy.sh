#!/usr/bin/env bash
# rebuild-and-deploy.sh — One-command rebuild + deploy to RPi4
#
# Usage:
#   bash tools/batocera/rpi4/rebuild-and-deploy.sh [options] [PI_IP]
#
# Options:
#   --clean     Wipe build_rpi4 and reconfigure from scratch
#   --tracy     Enable Tracy profiler instrumentation
#   --lto       Enable Link-Time Optimization (even for Debug builds)
#   --release   Build in Release mode (default)
#   --debug     Build in Debug mode
#   -h, --help  Show this help
#
# Examples:
#   bash tools/batocera/rpi4/rebuild-and-deploy.sh                    # incremental build + deploy
#   bash tools/batocera/rpi4/rebuild-and-deploy.sh --clean            # full clean rebuild
#   bash tools/batocera/rpi4/rebuild-and-deploy.sh --tracy --clean    # clean rebuild with Tracy
#   bash tools/batocera/rpi4/rebuild-and-deploy.sh 192.168.1.100      # deploy to different IP
set -euo pipefail

# ── Defaults ──────────────────────────────────────────────────
PI_IP="192.168.86.44"
PI_USER="root"
PI_PASS="linux"
PI_DEST="/userdata/roms/ports/3sx"

DO_CLEAN=false
ENABLE_TRACY=false
ENABLE_LTO=false
BUILD_TYPE="RelWithDebInfo"

ROOT_DIR="$(cd "$(dirname "$0")/../../../" && pwd)"
BUILD_DIR="$ROOT_DIR/build_rpi4"

# ── Argument parsing ──────────────────────────────────────────
for arg in "$@"; do
    case "$arg" in
        --clean)   DO_CLEAN=true ;;
        --tracy)   ENABLE_TRACY=true ;;
        --lto)     ENABLE_LTO=true ;;
        --release) BUILD_TYPE="Release" ;;
        --debug)   BUILD_TYPE="Debug" ;;
        -h|--help)
            head -n 17 "$0" | tail -n +2 | sed 's/^# \?//'
            exit 0
            ;;
        -*)        echo "Unknown option: $arg" >&2; exit 1 ;;
        *)         PI_IP="$arg" ;;
    esac
done

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

# ── Pre-flight checks ─────────────────────────────────────────
command -v aarch64-linux-gnu-gcc >/dev/null || die "Cross-compiler not found. Run: sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu"
command -v cmake                >/dev/null || die "cmake not found. Run: sudo apt install cmake"
command -v sshpass              >/dev/null || die "sshpass not found. Run: sudo apt install sshpass"

# ── Print configuration ───────────────────────────────────────
echo ""
echo -e "${CYAN}┌─────────────────────────────────────────┐${NC}"
echo -e "${CYAN}│  3SX RPi4 Build & Deploy                │${NC}"
echo -e "${CYAN}├─────────────────────────────────────────┤${NC}"
echo -e "${CYAN}│${NC}  Build type:  ${YELLOW}$BUILD_TYPE${NC}"
echo -e "${CYAN}│${NC}  Tracy:       ${YELLOW}$([ "$ENABLE_TRACY" = true ] && echo "ENABLED" || echo "disabled")${NC}"
echo -e "${CYAN}│${NC}  LTO:         ${YELLOW}$([ "$ENABLE_LTO" = true ] && echo "ENABLED" || echo "disabled")${NC}"
echo -e "${CYAN}│${NC}  Clean:       ${YELLOW}$([ "$DO_CLEAN" = true ] && echo "YES" || echo "no (incremental)")${NC}"
echo -e "${CYAN}│${NC}  Target:      ${YELLOW}$PI_USER@$PI_IP${NC}"
echo -e "${CYAN}└─────────────────────────────────────────┘${NC}"

# ── Step 1: Clean (optional) ─────────────────────────────────
if [ "$DO_CLEAN" = true ]; then
    step "Cleaning build directory..."
    rm -rf "$BUILD_DIR"
    info "Removed $BUILD_DIR"
fi

# ── Step 2: Configure ────────────────────────────────────────
if [ ! -f "$BUILD_DIR/build.ninja" ] && [ ! -f "$BUILD_DIR/Makefile" ]; then
    step "Configuring CMake (build_rpi4)..."
    mkdir -p "$BUILD_DIR"
    cd "$BUILD_DIR"

    CMAKE_ARGS=(
        -DPLATFORM_RPI4=ON
        -DENABLE_TESTS=OFF
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE"
        -DCMAKE_SYSTEM_NAME=Linux
        -DCMAKE_SYSTEM_PROCESSOR=aarch64
        -DCMAKE_C_COMPILER=aarch64-linux-gnu-gcc
        -DCMAKE_CXX_COMPILER=aarch64-linux-gnu-g++
        -DFreetype_ROOT="$ROOT_DIR/third_party_rpi4/freetype/build"
    )

    if [ "$ENABLE_TRACY" = true ]; then
        CMAKE_ARGS+=(-DENABLE_TRACY=ON)
        info "Tracy profiler: ENABLED"
    else
        CMAKE_ARGS+=(-DENABLE_TRACY=OFF)
    fi

    if [ "$ENABLE_LTO" = true ]; then
        CMAKE_ARGS+=(-DENABLE_LTO=ON)
        info "LTO: ENABLED"
    fi

    cmake "$ROOT_DIR" "${CMAKE_ARGS[@]}" || die "CMake configure failed!"
    echo -e "${GREEN}✔ Configure succeeded${NC}"
else
    info "Build directory already configured (use --clean to reconfigure)"
    # If Tracy flag changed, reconfigure in-place
    if [ "$ENABLE_TRACY" = true ]; then
        step "Reconfiguring with Tracy enabled..."
        cd "$BUILD_DIR"
        cmake . -DENABLE_TRACY=ON || die "CMake reconfigure failed!"
    fi
fi

# ── Step 3: Build ─────────────────────────────────────────────
step "Building 3SX..."
cd "$BUILD_DIR"
cmake --build . -j"$(nproc)" || die "Build failed!"
echo -e "${GREEN}✔ Build succeeded${NC}"

# ── Step 4: Package ───────────────────────────────────────────
step "Packaging deployment tarball..."
cd "$ROOT_DIR"
bash tools/batocera/rpi4/deploy.sh || die "Deploy packaging failed!"
[ -f "$ROOT_DIR/game_deployment.tar.gz" ] || die "Tarball not created!"
SIZE=$(du -h "$ROOT_DIR/game_deployment.tar.gz" | cut -f1)
echo -e "${GREEN}✔ Package created ($SIZE)${NC}"

# ── Step 5: Check Pi is reachable ─────────────────────────────
step "Checking Pi4 at $PI_IP..."
ping -c 1 -W 2 "$PI_IP" >/dev/null 2>&1 || die "Pi4 not reachable at $PI_IP. Check network or pass IP as argument: $0 <IP>"
echo -e "${GREEN}✔ Pi4 is reachable${NC}"

# ── Step 6: Upload ────────────────────────────────────────────
step "Uploading to $PI_USER@$PI_IP:$PI_DEST/..."
sshpass -p "$PI_PASS" scp -o StrictHostKeyChecking=no \
    "$ROOT_DIR/game_deployment.tar.gz" \
    "$PI_USER@$PI_IP:$PI_DEST/" || die "SCP upload failed!"
echo -e "${GREEN}✔ Upload complete${NC}"

# ── Step 7: Stop running 3sx (avoid "Text file busy") ─────────
step "Stopping 3sx on Pi4 (if running)..."
sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" \
    "killall -9 3sx 2>/dev/null; sleep 0.5; true"
echo -e "${GREEN}✔ Process stopped (or wasn't running)${NC}"

# ── Step 8: Extract on Pi ─────────────────────────────────────
step "Extracting on Pi4..."
sshpass -p "$PI_PASS" ssh -o StrictHostKeyChecking=no "$PI_USER@$PI_IP" \
    "cd $PI_DEST && tar xzf game_deployment.tar.gz --overwrite && chmod +x 3sx 3sx.sh && rm game_deployment.tar.gz" \
    || die "Remote extraction failed!"
echo -e "${GREEN}✔ Deployed successfully!${NC}"

# ── Done ──────────────────────────────────────────────────────
echo ""
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo -e "${GREEN}  ✔ 3SX deployed to Pi4 ($PI_IP)${NC}"
if [ "$ENABLE_TRACY" = true ]; then
echo -e "${YELLOW}  🔬 Tracy profiling ENABLED${NC}"
echo -e "${YELLOW}  Connect Tracy GUI to $PI_IP:8086${NC}"
fi
echo -e "${GREEN}═══════════════════════════════════════${NC}"
echo ""
echo "Launch on Pi:  ssh root@$PI_IP 'cd $PI_DEST && ./3sx.sh'"
