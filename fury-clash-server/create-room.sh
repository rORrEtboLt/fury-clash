#!/usr/bin/env bash
# create-room.sh — Create a relay room and print connection commands for both players.
#
# Usage:
#   ./create-room.sh [relay_host] [relay_port]
#
# Examples:
#   ./create-room.sh                    # localhost:3002
#   ./create-room.sh 192.168.1.10       # remote host, default port
#   ./create-room.sh 192.168.1.10 3002  # remote host + explicit port
#
# The relay server must be running first (./start-relay.sh).
# This script prints two ./fury-clash command lines, one per player.

set -euo pipefail

RELAY_HOST="${1:-localhost}"
RELAY_PORT="${2:-3002}"
RELAY_HTTP="http://${RELAY_HOST}:${RELAY_PORT}"
RELAY_WS="ws://${RELAY_HOST}:${RELAY_PORT}"

# ── Generate UUIDs ──────────────────────────────────────────────────────────
new_uuid() {
    if command -v uuidgen &>/dev/null; then
        uuidgen | tr '[:upper:]' '[:lower:]'
    elif [ -f /proc/sys/kernel/random/uuid ]; then
        cat /proc/sys/kernel/random/uuid
    else
        # Fallback: construct UUID-like string from /dev/urandom
        od -An -tx1 -N16 /dev/urandom | tr -d ' \n' | \
            sed 's/\(.\{8\}\)\(.\{4\}\)\(.\{4\}\)\(.\{4\}\)\(.\{12\}\)/\1-\2-\3-\4-\5/'
    fi
}

ROOM_ID="$(new_uuid)"
P1_ID="$(new_uuid | cut -c1-8)"
P2_ID="$(new_uuid | cut -c1-8)"

# Prefix IDs so they're clearly player identifiers
P1_ID="player-${P1_ID}"
P2_ID="player-${P2_ID}"

# ── Create room via relay HTTP API ──────────────────────────────────────────
PAYLOAD=$(printf '{"roomId":"%s","p1Id":"%s","p2Id":"%s","p1Fighter":0,"p2Fighter":1}' \
    "$ROOM_ID" "$P1_ID" "$P2_ID")

echo "[fury-clash] Creating room at ${RELAY_HTTP}/rooms ..."

RESPONSE=$(curl -sf -X POST \
    -H "Content-Type: application/json" \
    -d "$PAYLOAD" \
    "${RELAY_HTTP}/rooms") || {
    echo ""
    echo "ERROR: Could not connect to relay at ${RELAY_HTTP}"
    echo "Make sure the relay is running: cd fury-clash-server && ./start-relay.sh"
    exit 1
}

echo "[fury-clash] Server response: ${RESPONSE}"

# ── Print connection commands ───────────────────────────────────────────────
echo ""
echo "=========================================="
echo "  Room ID : ${ROOM_ID}"
echo "  Relay   : ${RELAY_WS}"
echo "=========================================="
echo ""
echo "  Player 1 — run this command on machine 1:"
echo "    ./build/fury-clash ${RELAY_WS} ${ROOM_ID} ${P1_ID} 0"
echo ""
echo "  Player 2 — run this command on machine 2:"
echo "    ./build/fury-clash ${RELAY_WS} ${ROOM_ID} ${P2_ID} 1"
echo ""
echo "=========================================="
echo ""
echo "  Both players press ENTER (or just launch) to connect."
echo "  The fight starts automatically when both players join the room."
echo ""
