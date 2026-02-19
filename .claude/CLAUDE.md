# Fury Clash — Claude Code Guide

## Project Overview

1v1 multiplayer fighting game: **C99 + SDL3** client with rollback netcode, **Node.js + TypeScript** backend with relay, matchmaking, and REST API.

## Essential Commands

```bash
# Start relay server (no DB or Redis needed)
cd fury-clash-server && ./start-relay.sh

# Create a room and print two player launch commands
cd fury-clash-server && ./create-room.sh [host] [port]

# Build the client
cmake -S fury-clash-client -B fury-clash-client/build && cmake --build fury-clash-client/build

# Run client unit tests
cd fury-clash-client/build && ./test_rollback && ./test_hitbox && ./test_serializer

# Run server tests
cd fury-clash-server && npm test

# Full stack via Docker
cd fury-clash-server && ./start.sh
```

## Key File Map

| File | Purpose |
|------|---------|
| `fury-clash-client/src/network/net_client.c` | RFC 6455 WebSocket over POSIX sockets |
| `fury-clash-client/src/network/rollback.c` | State save / load / re-simulate |
| `fury-clash-client/src/gameplay/simulation.h` | Top-level deterministic tick |
| `fury-clash-server/src/relay-server/relay-room.ts` | Per-room input relay (no game logic) |
| `fury-clash-server/src/relay-server/room-manager.ts` | Room lifecycle (create / destroy) |
| `fury-clash-server/src/relay-server/protocol.ts` | Binary message encode / decode |
| `fury-clash-server/src/matchmaker/matcher.ts` | ELO-based pairing algorithm |

## Architecture

```
fury-clash-server/src/relay-server/   WebSocket relay (port 3002)
fury-clash-server/src/api-server/     Fastify REST API (port 3000)
fury-clash-server/src/matchmaker/     ELO matchmaking (port 3001)
fury-clash-client/src/network/        Rollback netcode + WS client
fury-clash-client/src/gameplay/       Deterministic fight simulation
```

## Hard Constraints

- **C99 only** — no C++ features; no external C libraries beyond SDL3.
- **Gameplay must be deterministic** — every state transition must reproduce identically from the same inputs on any platform.
- **The relay is dumb** — `relay-room.ts` forwards input packets verbatim; game logic must never run server-side.
- **Binary protocol is frozen** — `CLIENT_INPUT` = 9 bytes LE: `[0x02][seq 16][frame 32][input 16]`.
- **No masking on server frames** — only client→server WS frames are masked (RFC 6455 §5.3).

## Server Environment Variables

| Variable | Default | Effect |
|----------|---------|--------|
| `RELAY_SKIP_DB=1` | off | Run relay without PostgreSQL / Redis |
| `RELAY_PORT` | `3002` | WebSocket + HTTP port |
| `API_PORT` | `3000` | REST API port |
| `NODE_ENV` | `development` | Controls log verbosity |

## Client Launch Modes

```bash
./fury-clash                                      # local 2-player (no network)
./fury-clash ws://HOST:3002 ROOM_ID P_ID SLOT     # network mode (argc == 5)
```

## Common Pitfalls

- Call `net_client_configure()` **before** `net_client_connect(NULL)`.
- The `token` field in `CLIENT_JOIN` is not validated in relay-only mode — any string works.
- Always set `RELAY_SKIP_DB=1` when running the relay without a database.
- WS frames sent by the client **must** be masked; frames sent by the server must **not** be.
- Rollback state snapshots must capture **all** mutable gameplay state — missing a field causes silent desync.
