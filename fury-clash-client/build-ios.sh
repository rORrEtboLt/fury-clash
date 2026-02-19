#!/usr/bin/env bash
# build-ios.sh — Build Fury Clash for iPhone on macOS.
#
# Prerequisites (run once on your Mac):
#   brew install cmake
#   # SDL3 is built automatically the first time
#
# Usage:
#   ./build-ios.sh [--server ws://YOUR_LINUX_IP:3002] [--open-xcode]
#
# Output: build-ios/FuryClash.ipa  (unsigned, ready for AltStore / Sideloadly)

set -euo pipefail
cd "$(dirname "$0")"

SERVER_URL="ws://192.168.1.100:3002"
OPEN_XCODE=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --server)   SERVER_URL="$2"; shift 2 ;;
        --open-xcode) OPEN_XCODE=1; shift ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

echo "[fury-clash] Building iOS IPA"
echo "[fury-clash] Relay server URL: $SERVER_URL"
echo ""

SCRIPT_DIR="$(pwd)"
SDL3_IOS_PREFIX="${SCRIPT_DIR}/../SDL3-ios"
BUILD_DIR="${SCRIPT_DIR}/../build-fury-clash-ios"

# ── Step 1: Build SDL3 for iOS if not already done ───────────────────────
if [ ! -d "$SDL3_IOS_PREFIX" ]; then
    echo "[1/4] Building SDL3 for iOS..."
    SDL3_SRC="${SCRIPT_DIR}/../SDL3-src"

    if [ ! -d "$SDL3_SRC" ]; then
        echo "  Cloning SDL3..."
        git clone --depth 1 https://github.com/libsdl-org/SDL.git "$SDL3_SRC"
    fi

    cmake -S "$SDL3_SRC" -B "${SDL3_SRC}-build" \
        -DCMAKE_SYSTEM_NAME=iOS \
        -DCMAKE_OSX_SYSROOT=iphoneos \
        -DCMAKE_OSX_ARCHITECTURES=arm64 \
        -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
        -DCMAKE_BUILD_TYPE=Release \
        -DSDL_SHARED=OFF \
        -DSDL_STATIC=ON \
        -DSDL_TEST_LIBRARY=OFF \
        -DSDL_TESTS=OFF \
        -G Xcode

    cmake --build "${SDL3_SRC}-build" --config Release -- -sdk iphoneos
    cmake --install "${SDL3_SRC}-build" --config Release --prefix "$SDL3_IOS_PREFIX"
    echo "  SDL3 built OK"
else
    echo "[1/4] SDL3 iOS already built — skipping"
fi

# ── Step 2: Configure Fury Clash for iOS ─────────────────────────────────
echo "[2/4] Configuring Fury Clash for iOS..."
cmake -S "$SCRIPT_DIR" -B "$BUILD_DIR" \
    -DCMAKE_SYSTEM_NAME=iOS \
    -DCMAKE_OSX_SYSROOT=iphoneos \
    -DCMAKE_OSX_ARCHITECTURES=arm64 \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
    "-DSDL3_DIR=${SDL3_IOS_PREFIX}/lib/cmake/SDL3" \
    "-DFC_IOS_SERVER_URL=${SERVER_URL}" \
    -DCMAKE_BUILD_TYPE=Release \
    -G Xcode

echo "  Configured OK"

# ── Step 3: Open Xcode (optional) for signing setup ──────────────────────
if [ "$OPEN_XCODE" -eq 1 ]; then
    echo ""
    echo "Opening Xcode for signing configuration..."
    echo "  1. Select your Team in Signing & Capabilities"
    echo "  2. Change bundle ID to something unique: e.g. com.yourname.furyclash"
    echo "  3. Connect your iPhone"
    echo "  4. Select your device in the scheme dropdown"
    echo "  5. Click Run (▶)"
    open "${BUILD_DIR}/FuryClash.xcodeproj"
    exit 0
fi

# ── Step 4: Build and package unsigned IPA ───────────────────────────────
echo "[3/4] Building Fury Clash..."
xcodebuild \
    -project "${BUILD_DIR}/FuryClash.xcodeproj" \
    -scheme fury-clash \
    -sdk iphoneos \
    -configuration Release \
    -arch arm64 \
    CODE_SIGNING_REQUIRED=NO \
    CODE_SIGNING_ALLOWED=NO \
    CODE_SIGN_IDENTITY="" \
    ONLY_ACTIVE_ARCH=YES \
    build \
    SYMROOT="${BUILD_DIR}/output"

APP_PATH=$(find "${BUILD_DIR}/output" -name "fury-clash.app" | head -1)
if [ -z "$APP_PATH" ]; then
    echo "ERROR: fury-clash.app not found in build output"
    exit 1
fi
echo "  App built: $APP_PATH"

echo "[4/4] Packaging IPA..."
IPA_DIR="${SCRIPT_DIR}/../build-ios"
mkdir -p "${IPA_DIR}/Payload"
cp -r "$APP_PATH" "${IPA_DIR}/Payload/fury-clash.app"

IPA_PATH="${SCRIPT_DIR}/../FuryClash.ipa"
(cd "$IPA_DIR" && zip -qr "$IPA_PATH" Payload)

echo ""
echo "=========================================================="
echo "  IPA ready: $(realpath "$IPA_PATH")"
echo "  Size: $(du -sh "$IPA_PATH" | cut -f1)"
echo "=========================================================="
echo ""
echo "  Install on iPhone from your Linux laptop:"
echo ""
echo "  Option A — AltServer-Linux (recommended):"
echo "    pip3 install pyipautil"
echo "    git clone https://github.com/NyaMisty/AltServer-Linux && cd AltServer-Linux"
echo "    # Follow README to set up Apple ID credentials"
echo "    ./AltServer-Linux install-ipa FuryClash.ipa"
echo ""
echo "  Option B — pymobiledevice3:"
echo "    pip3 install pymobiledevice3"
echo "    pymobiledevice3 usbmux install FuryClash.ipa"
echo ""
echo "  Option C — SideStore (no PC required after setup):"
echo "    https://sidestore.io  — then share IPA to your iPhone"
echo ""
echo "  After install, run create-room.sh on your Linux laptop,"
echo "  open Fury Clash on your iPhone, enter the Room ID, tap Join as P2."
echo "=========================================================="
