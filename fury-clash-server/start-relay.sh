#!/usr/bin/env bash
# start-relay.sh — Start the fury-clash relay server in standalone mode (no DB/Redis required).
#
# Usage:
#   ./start-relay.sh           # dev mode via tsx (auto-reloads on file changes)
#   ./start-relay.sh --prod    # compile TypeScript first, then run compiled JS
#
# Environment:
#   RELAY_PORT  (default: 3002)  — port to listen on

set -euo pipefail
cd "$(dirname "$0")"

export RELAY_SKIP_DB=1
export RELAY_PORT="${RELAY_PORT:-3002}"

echo "[fury-clash] Starting relay in standalone mode (RELAY_SKIP_DB=1) on port ${RELAY_PORT}"
echo "[fury-clash] No PostgreSQL or Redis required."
echo ""

MODE="${1:-dev}"

if [ "$MODE" = "--prod" ]; then
    echo "[fury-clash] Building TypeScript..."
    npx tsc
    echo "[fury-clash] Starting compiled relay..."
    node dist/relay-server/index.js
else
    # Dev mode: tsx runs TypeScript directly, no build step needed
    npx tsx src/relay-server/index.ts
fi
