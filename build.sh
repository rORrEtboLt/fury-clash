#!/bin/bash
set -e

# ============================================================
#  build.sh — Build "Neon Dodge" SDL3 game on Ubuntu
#
#  Usage:
#    chmod +x build.sh
#    ./build.sh          # full build (installs SDL3 if needed)
#    ./build.sh run      # build + run
#    ./build.sh clean    # remove build artifacts
# ============================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
SDL3_VERSION="release-3.2.8"   # latest stable tag — bump as needed
SDL3_SRC="/tmp/SDL3-src"
SDL3_PREFIX="/usr/local"

RED='\033[0;31m'; GREEN='\033[0;32m'; CYAN='\033[0;36m'; NC='\033[0m'

info()  { echo -e "${CYAN}[INFO]${NC}  $*"; }
ok()    { echo -e "${GREEN}[ OK ]${NC}  $*"; }
fail()  { echo -e "${RED}[FAIL]${NC}  $*"; exit 1; }

# ------------------------------------------------------------------
# 1. Install system dependencies
# ------------------------------------------------------------------
install_deps() {
    info "Installing build dependencies …"
    sudo apt-get update -qq
    sudo apt-get install -y -qq \
        build-essential cmake git pkg-config \
        libx11-dev libxext-dev libxrandr-dev libxcursor-dev \
        libxi-dev libxss-dev libwayland-dev libxkbcommon-dev \
        libegl-dev libgles-dev libdrm-dev libgbm-dev \
        libasound2-dev libpulse-dev libdbus-1-dev libudev-dev
    ok "System deps installed"
}

# ------------------------------------------------------------------
# 2. Build & install SDL3 from source (if not already present)
# ------------------------------------------------------------------
install_sdl3() {
    if pkg-config --exists sdl3 2>/dev/null; then
        ok "SDL3 already installed ($(pkg-config --modversion sdl3))"
        return
    fi

    info "Cloning SDL3 (${SDL3_VERSION}) …"
    rm -rf "${SDL3_SRC}"
    git clone --depth 1 --branch "${SDL3_VERSION}" \
        https://github.com/libsdl-org/SDL.git "${SDL3_SRC}"

    info "Building SDL3 …"
    cmake -S "${SDL3_SRC}" -B "${SDL3_SRC}/build" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="${SDL3_PREFIX}" \
        -DSDL_TESTS=OFF -DSDL_EXAMPLES=OFF
    cmake --build "${SDL3_SRC}/build" -j"$(nproc)"

    info "Installing SDL3 to ${SDL3_PREFIX} …"
    sudo cmake --install "${SDL3_SRC}/build"
    sudo ldconfig
    ok "SDL3 $(pkg-config --modversion sdl3) installed"
}

# ------------------------------------------------------------------
# 3. Build the game
# ------------------------------------------------------------------
build_game() {
    cd "${SCRIPT_DIR}"
    info "Compiling Neon Dodge …"
    make clean 2>/dev/null || true
    make
    ok "Build complete → ./neon_dodge"
}

# ------------------------------------------------------------------
# Entry point
# ------------------------------------------------------------------
case "${1:-build}" in
    clean)
        cd "${SCRIPT_DIR}"; make clean; ok "Cleaned."
        ;;
    run)
        install_deps; install_sdl3; build_game
        info "Launching …"
        cd "${SCRIPT_DIR}" && ./neon_dodge
        ;;
    build|"")
        install_deps; install_sdl3; build_game
        echo ""
        echo "========================================="
        echo "  Run the game:   ./neon_dodge"
        echo "  Or:             ./build.sh run"
        echo "========================================="
        ;;
    *)
        echo "Usage: $0 [build|run|clean]"
        ;;
esac
