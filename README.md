# Fury Clash

A 1v1 multiplayer fighting game prototype built with **C99 + SDL3** (client) and **Node.js + TypeScript** (server). Features rollback netcode, a custom WebSocket client, ELO matchmaking, and a CI pipeline that produces unsigned iOS builds.

```
fury-clash-client/    C99 + SDL3 game client (Linux, macOS, iOS)
fury-clash-server/    Node.js + TypeScript backend
├── api-server/       REST API — auth, profiles, leaderboards  (port 3000)
├── relay-server/     WebSocket input relay                    (port 3002)
└── matchmaker/       ELO-based queue                          (port 3001)
```

## Features

- **Rollback netcode** — frame-perfect inputs with near-zero perceived latency
- **Custom WebSocket client** — RFC 6455 over raw POSIX sockets, zero external C dependencies
- **Dumb relay server** — forwards inputs only; all game logic stays on the clients
- **Full backend** — JWT auth, ELO matchmaking, PostgreSQL + Redis, replay storage
- **iOS CI build** — unsigned `.ipa` artifact produced by GitHub Actions on every push
- **Compact binary protocol** — 9 bytes per input frame, ~540 bytes/sec at 60 FPS

## Quick Start

### Standalone relay (no database required)

```bash
cd fury-clash-server
npm install
./start-relay.sh          # relay starts on :3002

# In a second terminal
./create-room.sh          # prints two ./fury-clash launch commands
```

### Client (Linux / macOS)

```bash
cd fury-clash-client
cmake -S . -B build && cmake --build build

./build/fury-clash                                      # local 2-player on same machine
./build/fury-clash ws://HOST:3002 ROOM_ID P_ID SLOT    # network mode
```

### Full stack (Docker)

```bash
cd fury-clash-server
./start.sh    # PostgreSQL + Redis + API + Relay + Matchmaker
```

## Prerequisites

| Component  | Requirement                            |
|------------|----------------------------------------|
| Client     | CMake 3.20+, GCC or Clang, SDL3        |
| Server     | Node.js 20+, npm                       |
| Full stack | Docker + Docker Compose                |

**Install SDL3 on Ubuntu** (builds from source into `/usr/local`):

```bash
./build.sh
```

## Repository Layout

```
sdl3-claude-prototype/
├── fury-clash-client/
│   ├── src/
│   │   ├── gameplay/       Fighting game logic (must stay deterministic)
│   │   ├── network/        Rollback engine + WebSocket client
│   │   ├── render/         SDL3 GPU rendering
│   │   ├── input/          Keyboard / gamepad / touch
│   │   └── platform/       Desktop, iOS, Android bridges
│   ├── assets/             Sprites, audio, stage art
│   ├── tests/              Unit tests (rollback, hitbox, serializer)
│   └── platform/ios/       Xcode project + CI toolchain
│
├── fury-clash-server/
│   ├── src/
│   │   ├── api-server/     Fastify REST API
│   │   ├── relay-server/   WebSocket input relay
│   │   └── matchmaker/     ELO matchmaking
│   ├── prisma/             PostgreSQL schema + migrations
│   └── deploy/             Kubernetes + Terraform configs
│
├── .github/workflows/      iOS unsigned IPA via CI
├── FURY_CLASH_ARCHITECTURE.md
└── build.sh                SDL3 installer for Ubuntu
```

## Network Protocol

Each client sends **9 bytes per frame** over a masked WebSocket binary frame:

```
[0x02][seq — 16-bit LE][frame — 32-bit LE][input — 16-bit LE]
```

The relay server echoes the packet to the opponent unchanged. Both clients run the same deterministic simulation and roll back when a late input arrives.

See [FURY_CLASH_ARCHITECTURE.md](FURY_CLASH_ARCHITECTURE.md) for the complete design document including frame data, ELO algorithm, and deployment runbook.

## iOS Build (CI)

The GitHub Actions workflow ([`.github/workflows/ios-build.yml`](.github/workflows/ios-build.yml)) builds an unsigned `.ipa` for every push to `main`/`master`:

1. Open **Actions → iOS Build** in this repository.
2. Download the `FuryClash-iOS-unsigned.ipa` artifact.
3. Sideload with [AltStore](https://altstore.io) or [pymobiledevice3](https://github.com/doronz88/pymobiledevice3).

The server URL is injected at build time via the `server_url` workflow input (default: `ws://192.168.1.100:3002`).

## Testing Two Players Locally

```bash
# Terminal 1 — relay
cd fury-clash-server && ./start-relay.sh

# Terminal 2 — create room, get commands
./create-room.sh localhost 3002

# Run each printed command in a separate terminal / machine
./fury-clash-client/build/fury-clash ws://HOST:3002 ROOM_ID P1_ID 0
./fury-clash-client/build/fury-clash ws://HOST:3002 ROOM_ID P2_ID 1
```

## License

[MIT](LICENSE)
