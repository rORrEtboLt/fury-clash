#!/usr/bin/env bash
# Fury Clash Server — one-shot startup script
# Run from inside fury-clash-server/

set -e
cd "$(dirname "$0")"

echo "==> Building images..."
docker compose build

echo "==> Starting infrastructure (Postgres + Redis)..."
docker compose up -d postgres redis

echo "==> Waiting for Postgres to be healthy..."
until docker compose exec postgres pg_isready -U fury -d fury_clash > /dev/null 2>&1; do
  sleep 1
done
echo "    Postgres ready."

echo "==> Running Prisma migrations..."
DATABASE_URL=postgresql://fury:fury@localhost:5432/fury_clash \
  npx prisma migrate deploy

echo "==> Starting services..."
docker compose up -d api relay matchmaker

echo ""
echo "✓ Fury Clash Server running:"
echo "  API        → http://localhost:3000"
echo "  Relay (WS) → ws://localhost:3002"
echo "  Matchmaker → internal (port 3001)"
echo ""
echo "Logs:  docker compose logs -f"
echo "Stop:  docker compose down"
