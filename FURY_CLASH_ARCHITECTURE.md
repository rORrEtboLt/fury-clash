# 🎮 FURY CLASH — Multiplayer Fighting Game
## Architecture, Design & Launch Plan

> **Stack:** SDL3 (C) for game client · Node.js for backend · WebSocket for realtime · Redis for state · PostgreSQL for persistence
> **Platforms:** iOS (iPhone + iPad) · Android (Phone + Tablet) · Desktop (stretch goal)
> **Multiplayer:** 1v1 real-time over network, two separate devices

---

## Table of Contents

1. [High-Level Architecture](#1-high-level-architecture)
2. [Network Model & Multiplayer Design](#2-network-model--multiplayer-design)
3. [Game Client Architecture (SDL3 / C)](#3-game-client-architecture-sdl3--c)
4. [Backend Architecture (Node.js)](#4-backend-architecture-nodejs)
5. [Folder Structure — Client](#5-folder-structure--client)
6. [Folder Structure — Server](#6-folder-structure--server)
7. [Data Models & Schemas](#7-data-models--schemas)
8. [Fighter & Animation System](#8-fighter--animation-system)
9. [Input System & Touch Controls](#9-input-system--touch-controls)
10. [Rendering Pipeline](#10-rendering-pipeline)
11. [Audio Design](#11-audio-design)
12. [Matchmaking & Lobby Flow](#12-matchmaking--lobby-flow)
13. [Anti-Cheat & Security](#13-anti-cheat--security)
14. [Platform-Specific Concerns (iOS / Android)](#14-platform-specific-concerns)
15. [Build & CI/CD Pipeline](#15-build--cicd-pipeline)
16. [Milestone Roadmap](#16-milestone-roadmap)
17. [Risk Register](#17-risk-register)

---

## 1. High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      PLAYER A (Device 1)                    │
│  ┌──────────┐  ┌──────────┐  ┌──────────┐  ┌────────────┐  │
│  │  Touch   │→│  Input   │→│  Game    │→│  Renderer  │  │
│  │  HUD     │  │  Manager │  │  Sim     │  │  (SDL3 GPU)│  │
│  └──────────┘  └────┬─────┘  └────┬─────┘  └────────────┘  │
│                     │             │                          │
│               ┌─────▼─────────────▼──────┐                  │
│               │   Network Client (UDP +  │                  │
│               │   WebSocket fallback)    │                  │
│               └────────────┬─────────────┘                  │
└────────────────────────────┼────────────────────────────────┘
                             │
                    ━━━━━━━━━▼━━━━━━━━━
                    ┃   INTERNET       ┃
                    ━━━━━━━━━┯━━━━━━━━━
                             │
┌────────────────────────────┼────────────────────────────────┐
│                    BACKEND CLUSTER                           │
│                            │                                │
│  ┌─────────────────────────▼────────────────────────────┐   │
│  │              API Gateway / Load Balancer              │   │
│  │              (nginx / Cloudflare)                     │   │
│  └──┬──────────────┬───────────────┬────────────────────┘   │
│     │              │               │                        │
│  ┌──▼────┐   ┌─────▼─────┐  ┌─────▼──────┐                 │
│  │ REST  │   │ Game      │  │ Matchmaker │                  │
│  │ API   │   │ Relay     │  │ Service    │                  │
│  │Server │   │ Server    │  │            │                  │
│  │       │   │ (WS/UDP)  │  │            │                  │
│  └──┬────┘   └─────┬─────┘  └─────┬──────┘                 │
│     │              │               │                        │
│  ┌──▼──────────────▼───────────────▼──────┐                 │
│  │          Redis (match state, pub/sub)  │                 │
│  └──┬─────────────────────────────────────┘                 │
│     │                                                       │
│  ┌──▼──────────────────────────────────────┐                │
│  │       PostgreSQL (accounts, stats,      │                │
│  │       leaderboards, unlocks)            │                │
│  └─────────────────────────────────────────┘                │
└─────────────────────────────────────────────────────────────┘
```

### Key Architectural Decisions

| Decision | Choice | Rationale |
|----------|--------|-----------|
| Language (client) | C99 | Maximum SDL3 compatibility, best mobile perf |
| Build system | CMake | Cross-platform, SDL3 native support |
| Networking primary | WebSocket (ws) | Works through NAT/firewalls on mobile |
| Networking stretch | UDP via ENet | Lower latency for competitive play |
| Serialization | FlatBuffers | Zero-copy, fast on mobile, small wire size |
| State model | Lockstep w/ rollback | Gold standard for fighting games |
| Auth | JWT + device fingerprint | Stateless, mobile-friendly |
| Asset format | Sprite sheets (PNG) + JSON atlas | Simple, GPU-friendly, small download |

---

## 2. Network Model & Multiplayer Design

### 2.1 Why Rollback Netcode?

Fighting games demand frame-precise input. Traditional client-server with interpolation adds 50-100ms of visual delay — unacceptable for a fighting game. Instead:

```
Frame N:
  1. Read local input
  2. PREDICT remote input (repeat last known)
  3. Simulate frame with both inputs
  4. Render immediately (zero visual delay)
  5. Send local input to opponent via relay server

When remote input ACTUALLY arrives:
  If prediction was wrong:
    1. Rewind game state to divergence frame
    2. Re-simulate forward with corrected inputs
    3. Current frame is now correct
  If prediction was right:
    → No correction needed (common case!)
```

### 2.2 State Synchronization

```
┌───────────────────────────────────────────┐
│           Rollback Buffer (128 frames)    │
│                                           │
│  Frame  │ P1 Input │ P2 Input │ Snapshot  │
│  ──────────────────────────────────────── │
│  1042   │ ↓+punch  │ →+block  │ [state]   │
│  1043   │ →        │ →        │ [state]   │
│  1044   │ punch    │ PREDICT  │ [state]   │  ← current
│  1045   │ (future) │ (future) │           │
└───────────────────────────────────────────┘
```

**State snapshot includes:** player positions, velocities, health, combo state, active hitboxes, animation frame index, hitstun/blockstun timers.

**Snapshot size target:** < 512 bytes (easily achievable for 2-player fighter).

### 2.3 Protocol Design

```
CLIENT → SERVER (per frame):
┌──────┬──────┬────────┬──────────┐
│ type │ seq  │ frame# │ input    │
│ 1B   │ 2B   │ 4B     │ 2B       │
└──────┴──────┴────────┴──────────┘
= 9 bytes per frame (540 bytes/sec at 60fps)

SERVER → CLIENT (relay, batched):
┌──────┬──────┬─────────┬────────────────────┐
│ type │ seq  │ count   │ [frame#, input]... │
│ 1B   │ 2B   │ 1B      │ 6B × count         │
└──────┴──────┴─────────┴────────────────────┘

Input bitfield (2 bytes):
  bit 0: up        bit 4: light_punch    bit 8:  special1
  bit 1: down      bit 5: heavy_punch    bit 9:  special2
  bit 2: left      bit 6: light_kick     bit 10: super
  bit 3: right     bit 7: heavy_kick     bit 11: grab
```

### 2.4 Latency Budget

```
Target: Playable up to 150ms one-way (300ms RTT)

0-50ms   → Flawless. 0-3 rollback frames.
50-100ms → Great. 3-6 rollback frames, barely noticeable.
100-150ms→ Acceptable. Occasional visual pops on mispredicts.
150ms+   → Degraded. Show network warning icon.
```

---

## 3. Game Client Architecture (SDL3 / C)

### 3.1 Module Dependency Graph

```
                    ┌──────────┐
                    │  main.c  │
                    └────┬─────┘
                         │
          ┌──────────────┼──────────────┐
          ▼              ▼              ▼
   ┌────────────┐ ┌───────────┐ ┌────────────┐
   │  platform/ │ │   core/   │ │  network/  │
   │  (SDL3     │ │  (game    │ │  (client,  │
   │   init,    │ │   loop,   │ │   proto,   │
   │   window)  │ │   ECS)    │ │   rollback)│
   └─────┬──────┘ └─────┬─────┘ └─────┬──────┘
         │               │             │
         ▼               ▼             ▼
   ┌───────────┐  ┌────────────┐ ┌──────────┐
   │  render/  │  │  gameplay/ │ │  net/     │
   │  (sprites,│  │  (fighter, │ │  serial/  │
   │   fx, hud)│  │   physics, │ │  (flatbuf)│
   │           │  │   hitbox)  │ │           │
   └───────────┘  └────────────┘ └──────────┘
         │               │
         ▼               ▼
   ┌───────────┐  ┌────────────┐
   │  assets/  │  │  audio/    │
   │  (loader, │  │  (mixer,   │
   │   atlas)  │  │   sfx,     │
   └───────────┘  │   music)   │
                  └────────────┘
```

### 3.2 Game Loop (Fixed Timestep)

```c
// core/gameloop.c — simplified
#define TICK_RATE    60
#define TICK_DT      (1.0 / TICK_RATE)

while (running) {
    platform_poll_events();       // SDL3 events → input buffer
    network_receive();            // drain incoming packets

    accumulator += frame_delta;
    while (accumulator >= TICK_DT) {
        // --- SIMULATION TICK (deterministic) ---
        input_local  = input_read_local();
        input_remote = rollback_get_remote_input(current_frame);

        if (rollback_needs_resimulate()) {
            rollback_rewind();
            rollback_resimulate_to(current_frame);
        }

        gameplay_tick(input_local, input_remote, TICK_DT);
        rollback_save_state(current_frame);
        network_send_input(current_frame, input_local);

        current_frame++;
        accumulator -= TICK_DT;
    }

    float alpha = accumulator / TICK_DT;
    render_interpolated(alpha);   // visual-only interpolation
    audio_update();
    platform_present();           // SDL_RenderPresent
}
```

### 3.3 Entity Component System (Lightweight)

Not a full ECS — a **struct-of-arrays** approach tuned for fighting games:

```c
// gameplay/fighter.h
typedef struct {
    // Identity
    int          fighter_id;        // character select index
    int          player_slot;       // 0 = P1, 1 = P2

    // Transform
    float        x, y;
    float        vx, vy;
    int          facing;            // +1 right, -1 left

    // Combat state machine
    FighterState state;             // IDLE, WALK, JUMP, ATTACK, HIT, BLOCK, etc.
    int          state_frame;       // frames in current state
    int          hitstun;
    int          blockstun;
    int          combo_counter;

    // Stats
    int          health;
    int          max_health;
    float        super_meter;       // 0.0 → 1.0

    // Animation
    int          anim_id;
    int          anim_frame;

    // Hitbox / Hurtbox (active during attacks)
    Rect         hitboxes[4];
    int          hitbox_count;
    Rect         hurtbox;
    HitProperties hit_props;        // damage, knockback, hitstun frames
} Fighter;
```

---

## 4. Backend Architecture (Node.js)

### 4.1 Service Decomposition

```
backend/
│
├── api-server/          ← REST API (Express/Fastify)
│   Auth, profiles, leaderboards, shop, unlocks
│   Port 3000
│
├── matchmaker/          ← Matchmaking service
│   Queue management, ELO matching, room creation
│   Communicates via Redis pub/sub
│   Port 3001
│
├── relay-server/        ← Game relay (WebSocket)
│   Receives inputs from both players, relays to opponent
│   Authoritative timer, disconnect detection
│   Port 3002 (WS) / 3003 (UDP optional)
│
├── shared/              ← Shared types, utils, config
│   Validation, constants, error codes
│
└── workers/             ← Background jobs (Bull queues)
    ELO recalculation, replay storage, analytics
```

### 4.2 Relay Server Detail

The relay server is the most latency-critical component:

```
Player A ──ws──→ ┌──────────────┐ ──ws──→ Player B
                 │  Relay Room  │
Player A ←──ws── │              │ ←──ws── Player B
                 │  • Input log │
                 │  • Timer     │
                 │  • Disc. det │
                 └──────────────┘

Relay does NOT simulate the game.
Relay ONLY forwards inputs + maintains:
  - Frame clock (authoritative)
  - Input history (for replay / dispute)
  - Connection quality metrics
  - Timeout / disconnect handling
```

This is intentionally NOT authoritative-server — fighting games need client-side prediction and rollback. The server is a **dumb relay** with bookkeeping.

### 4.3 Matchmaking Flow

```
1. Client → POST /api/matchmaking/queue { fighter_id, elo_range }
2. Server adds to Redis sorted set (score = ELO)
3. Matchmaker worker scans every 500ms:
   - Find pairs within ELO range (expanding over time)
   - Create relay room
   - Notify both clients via WS: { room_id, opponent_info, relay_endpoint }
4. Both clients connect to relay WS endpoint
5. Relay waits for both → sends MATCH_START
6. Game begins at frame 0
```

---

## 5. Folder Structure — Client

```
fury-clash-client/
├── CMakeLists.txt                 # Root build config
├── cmake/
│   ├── FindSDL3.cmake
│   ├── iOS.toolchain.cmake
│   └── Android.toolchain.cmake
│
├── src/
│   ├── main.c                     # Entry point, platform init
│   │
│   ├── platform/                  # SDL3 + OS abstraction
│   │   ├── platform.h             # Cross-platform interface
│   │   ├── platform_sdl3.c        # SDL3 window, events, lifecycle
│   │   ├── platform_ios.m         # iOS-specific (ObjC bridge)
│   │   └── platform_android.c     # Android JNI bridge
│   │
│   ├── core/                      # Engine core
│   │   ├── gameloop.h / .c        # Fixed timestep loop
│   │   ├── state_machine.h / .c   # Game states (menu, fight, results)
│   │   ├── timer.h / .c           # High-res timing
│   │   ├── memory.h / .c          # Arena allocator, pools
│   │   └── config.h               # Compile-time constants
│   │
│   ├── gameplay/                   # Fighting game logic (DETERMINISTIC)
│   │   ├── fighter.h / .c         # Fighter struct, state machine
│   │   ├── fighter_data.h / .c    # Character stats, move tables
│   │   ├── physics.h / .c         # Movement, gravity, push-out
│   │   ├── hitbox.h / .c          # Collision detection, hit resolution
│   │   ├── combo.h / .c           # Combo system, juggle tracking
│   │   ├── special_moves.h / .c   # Input motion detection (QCF, DP, etc.)
│   │   ├── super.h / .c           # Super meter, super attacks
│   │   ├── round.h / .c           # Round management, win conditions
│   │   └── simulation.h / .c      # Top-level sim tick (calls above)
│   │
│   ├── network/                    # Multiplayer
│   │   ├── net_client.h / .c      # WebSocket client (SDL3 sockets or libws)
│   │   ├── protocol.h             # Message types, opcodes
│   │   ├── serializer.h / .c      # FlatBuffer pack/unpack
│   │   ├── rollback.h / .c        # Rollback engine (save/load/resim)
│   │   ├── input_buffer.h / .c    # Ring buffer for remote inputs
│   │   └── net_stats.h / .c       # Ping, jitter, packet loss tracking
│   │
│   ├── input/                      # Input handling
│   │   ├── input.h / .c           # Unified input interface
│   │   ├── input_touch.h / .c     # Virtual joystick + buttons
│   │   ├── input_gamepad.h / .c   # MFi / Bluetooth controller
│   │   ├── input_keyboard.h / .c  # Desktop keyboard (debug/dev)
│   │   └── input_motion.h / .c    # Motion commands (236P = QCF+P)
│   │
│   ├── render/                     # SDL3 GPU rendering
│   │   ├── renderer.h / .c        # Init, present, viewport scaling
│   │   ├── sprite.h / .c          # Sprite + atlas rendering
│   │   ├── anim.h / .c            # Animation playback
│   │   ├── fx.h / .c              # Particles, screen shake, flash
│   │   ├── hud.h / .c             # Health bars, timer, super meter
│   │   ├── background.h / .c      # Parallax stage backgrounds
│   │   └── camera.h / .c          # Dynamic camera (zoom/pan)
│   │
│   ├── ui/                         # Menus and screens
│   │   ├── ui_system.h / .c       # Immediate-mode UI core
│   │   ├── screen_title.h / .c
│   │   ├── screen_charselect.h/.c
│   │   ├── screen_fight.h / .c
│   │   ├── screen_results.h / .c
│   │   ├── screen_lobby.h / .c
│   │   ├── screen_settings.h / .c
│   │   └── screen_training.h / .c
│   │
│   ├── audio/                      # Sound
│   │   ├── audio.h / .c           # SDL3 audio init, mixer
│   │   ├── sfx.h / .c             # Sound effects (hit, block, KO)
│   │   └── music.h / .c           # BGM streaming
│   │
│   └── assets/                     # Asset loading
│       ├── asset_loader.h / .c    # PNG, JSON, WAV loading
│       ├── atlas.h / .c           # Texture atlas parser
│       └── asset_manifest.h       # Enum of all asset IDs
│
├── assets/                         # Raw game assets
│   ├── fighters/
│   │   ├── ryu/                   # Example fighter
│   │   │   ├── idle.png           # Sprite sheet
│   │   │   ├── walk.png
│   │   │   ├── attacks.png
│   │   │   ├── specials.png
│   │   │   ├── atlas.json         # Frame coordinates
│   │   │   └── data.json          # Hitboxes, frame data, moves
│   │   └── ken/
│   │       └── ...
│   ├── stages/
│   │   ├── temple/
│   │   │   ├── bg_layer0.png      # Parallax layers
│   │   │   ├── bg_layer1.png
│   │   │   └── stage.json         # Floor bounds, camera limits
│   │   └── arena/
│   │       └── ...
│   ├── ui/
│   │   ├── hud_atlas.png
│   │   ├── menu_bg.png
│   │   ├── buttons.png
│   │   └── fonts/
│   │       └── main_font.png      # Bitmap font
│   ├── audio/
│   │   ├── sfx/
│   │   │   ├── hit_light.wav
│   │   │   ├── hit_heavy.wav
│   │   │   ├── block.wav
│   │   │   └── ko.wav
│   │   └── music/
│   │       ├── menu.ogg
│   │       ├── fight_temple.ogg
│   │       └── results.ogg
│   └── touch/                      # Virtual controller sprites
│       ├── joystick_base.png
│       ├── joystick_knob.png
│       └── button_atlas.png
│
├── platform/                       # Platform-specific project files
│   ├── ios/
│   │   ├── Info.plist
│   │   ├── LaunchScreen.storyboard
│   │   ├── Assets.xcassets/
│   │   └── fury-clash.xcodeproj/
│   └── android/
│       ├── app/
│       │   ├── src/main/
│       │   │   ├── AndroidManifest.xml
│       │   │   ├── java/.../MainActivity.java
│       │   │   └── res/
│       │   └── build.gradle
│       ├── build.gradle
│       └── settings.gradle
│
├── tools/                          # Dev tools
│   ├── hitbox_editor/             # Visual hitbox editor (Python/Dear ImGui)
│   ├── frame_data_viewer/         # Frame data spreadsheet generator
│   ├── atlas_packer/              # Sprite sheet packing tool
│   └── replay_viewer/             # Replay file player
│
├── tests/
│   ├── test_rollback.c            # Rollback determinism tests
│   ├── test_hitbox.c              # Collision tests
│   ├── test_input_motion.c        # QCF/DP detection tests
│   └── test_serializer.c          # Protocol serialization tests
│
└── docs/
    ├── ARCHITECTURE.md            # This document
    ├── FRAME_DATA.md              # Fighter frame data reference
    ├── NETCODE.md                 # Rollback netcode deep dive
    └── ART_PIPELINE.md            # Sprite sheet workflow
```

---

## 6. Folder Structure — Server

```
fury-clash-server/
├── package.json
├── tsconfig.json
├── .env.example
├── docker-compose.yml              # Redis + Postgres + services
├── Dockerfile
│
├── src/
│   ├── api-server/                 # REST API (Fastify)
│   │   ├── index.ts                # Server bootstrap
│   │   ├── routes/
│   │   │   ├── auth.ts             # Register, login, JWT
│   │   │   ├── profile.ts          # Player profile CRUD
│   │   │   ├── leaderboard.ts      # Rankings
│   │   │   ├── matchmaking.ts      # Queue join/leave
│   │   │   ├── shop.ts             # In-app purchases, unlocks
│   │   │   └── replay.ts           # Replay download
│   │   ├── middleware/
│   │   │   ├── auth.ts             # JWT verification
│   │   │   ├── rate-limit.ts
│   │   │   └── validation.ts
│   │   └── plugins/
│   │       ├── db.ts               # PostgreSQL connection
│   │       └── redis.ts            # Redis connection
│   │
│   ├── relay-server/               # WebSocket game relay
│   │   ├── index.ts                # WS server bootstrap
│   │   ├── relay-room.ts           # Room: 2 players, input relay
│   │   ├── room-manager.ts         # Create/destroy rooms
│   │   ├── protocol.ts             # Binary message encode/decode
│   │   ├── disconnect-handler.ts   # Timeout, reconnect grace
│   │   └── input-log.ts            # Per-match input recording
│   │
│   ├── matchmaker/                 # Matchmaking service
│   │   ├── index.ts                # Service bootstrap
│   │   ├── queue.ts                # Redis sorted set queue
│   │   ├── elo.ts                  # ELO calculation
│   │   ├── matcher.ts              # Pairing algorithm
│   │   └── region.ts               # Region-based routing
│   │
│   ├── workers/                    # Background jobs (BullMQ)
│   │   ├── elo-worker.ts           # Post-match ELO update
│   │   ├── replay-worker.ts        # Compress & store replays
│   │   └── analytics-worker.ts     # Usage metrics
│   │
│   └── shared/                     # Shared across services
│       ├── types.ts                # TypeScript interfaces
│       ├── constants.ts            # Opcodes, limits, config
│       ├── errors.ts               # Error codes
│       ├── logger.ts               # Structured logging (pino)
│       └── config.ts               # Env-based config
│
├── prisma/                         # Database ORM
│   ├── schema.prisma               # DB schema
│   └── migrations/
│
├── tests/
│   ├── relay.test.ts
│   ├── matchmaker.test.ts
│   ├── elo.test.ts
│   └── protocol.test.ts
│
├── deploy/
│   ├── k8s/                        # Kubernetes manifests
│   │   ├── api-deployment.yaml
│   │   ├── relay-deployment.yaml
│   │   ├── matchmaker-deployment.yaml
│   │   └── redis-statefulset.yaml
│   └── terraform/                  # Infrastructure-as-code
│       ├── main.tf
│       └── variables.tf
│
└── docs/
    ├── API.md                      # REST endpoint reference
    ├── WEBSOCKET.md                # WS protocol documentation
    └── DEPLOYMENT.md               # Ops runbook
```

---

## 7. Data Models & Schemas

### 7.1 PostgreSQL (Prisma schema)

```prisma
model Player {
  id            String    @id @default(uuid())
  username      String    @unique
  email         String    @unique
  passwordHash  String
  elo           Int       @default(1000)
  wins          Int       @default(0)
  losses        Int       @default(0)
  coins         Int       @default(0)
  createdAt     DateTime  @default(now())
  updatedAt     DateTime  @updatedAt

  unlocks       Unlock[]
  matchesAsP1   Match[]   @relation("player1")
  matchesAsP2   Match[]   @relation("player2")
}

model Match {
  id            String    @id @default(uuid())
  player1Id     String
  player2Id     String
  player1       Player    @relation("player1", fields: [player1Id], references: [id])
  player2       Player    @relation("player2", fields: [player2Id], references: [id])
  winnerId      String?
  p1Fighter     String
  p2Fighter     String
  p1EloChange   Int
  p2EloChange   Int
  rounds        Json      // [{winner, p1hp, p2hp, duration}]
  replayUrl     String?
  region        String
  duration      Int       // seconds
  createdAt     DateTime  @default(now())
}

model Unlock {
  id            String    @id @default(uuid())
  playerId      String
  player        Player    @relation(fields: [playerId], references: [id])
  itemType      String    // "fighter", "skin", "stage"
  itemId        String
  unlockedAt    DateTime  @default(now())
  @@unique([playerId, itemType, itemId])
}
```

### 7.2 Redis Keys

```
match:queue:ranked        → Sorted Set (score=ELO, member=playerId)
match:room:{roomId}       → Hash { p1_id, p2_id, state, created_at }
player:session:{playerId} → String (relay server endpoint)
player:online:{playerId}  → String (TTL 30s, heartbeat)
relay:rooms:{serverId}    → Set of active room IDs
```

---

## 8. Fighter & Animation System

### 8.1 State Machine

```
                        ┌──────────┐
              ┌────────→│   IDLE   │←────────┐
              │         └────┬─────┘         │
              │              │               │
              │    ┌─────────┼─────────┐     │
              │    ▼         ▼         ▼     │
          ┌───────┐   ┌──────────┐  ┌──────┐│
          │ WALK  │   │  CROUCH  │  │ JUMP ││
          └───┬───┘   └────┬─────┘  └──┬───┘│
              │            │            │    │
              ▼            ▼            ▼    │
         ┌─────────────────────────────────┐│
         │          ATTACK (various)       ││
         │  stand_LP, stand_HP, cr_LK,     ││
         │  air_HK, special_qcf_P, ...     ││
         └──────────┬───────┬──────────────┘│
                    │       │               │
                    ▼       ▼               │
              ┌────────┐ ┌────────┐         │
              │  HIT   │ │ BLOCK  │         │
              │(stun)  │ │(stun)  │─────────┘
              └────┬───┘ └────────┘
                   │
                   ▼
              ┌────────┐
              │KNOCKDOWN│───→ WAKEUP ──→ IDLE
              └────────┘
```

### 8.2 Frame Data Format (per move)

```json
{
  "move_id": "stand_HP",
  "input": "HP",
  "total_frames": 28,
  "startup": 8,           // frames 1-8: no hitbox
  "active": 4,            // frames 9-12: hitbox active
  "recovery": 16,         // frames 13-28: vulnerable
  "damage": 80,
  "chip_damage": 8,
  "hitstun": 18,
  "blockstun": 12,
  "knockback": { "x": 3.5, "y": 0 },
  "cancel_into": ["special", "super"],
  "hitboxes_per_frame": {
    "9":  { "x": 20, "y": -40, "w": 60, "h": 30 },
    "10": { "x": 25, "y": -40, "w": 65, "h": 30 },
    "11": { "x": 25, "y": -38, "w": 60, "h": 28 },
    "12": { "x": 20, "y": -38, "w": 55, "h": 28 }
  }
}
```

### 8.3 Special Move Input Detection

```
Motion commands stored as directional history (last 30 frames):

QCF (Quarter Circle Forward) = ↓, ↓→, → + Button
  Pattern: [2, 3, 6] within 12-frame window

DP (Dragon Punch) = →, ↓, ↓→ + Button
  Pattern: [6, 2, 3] within 15-frame window

Charge = Hold ← for 40+ frames, then → + Button
  Pattern: [4 held 40f], [6]

360 = Full rotation + Button
  Pattern: [6,3,2,1,4] or [4,1,2,3,6] within 20 frames
```

---

## 9. Input System & Touch Controls

### 9.1 Touch Layout (iPhone)

```
┌──────────────────────────────────────────────────┐
│                                                  │
│           [GAME VIEWPORT - 16:9 area]            │
│                                                  │
│                                                  │
│                                                  │
├──────────────────────────────────────────────────┤
│                                                  │
│   ┌───┐                           [LP]  [HP]    │
│   │   │  ← Virtual                               │
│   │ ● │    Joystick          [LK]  [HK]         │
│   │   │  (floating,                               │
│   └───┘   follows thumb)     [SP1]  [SP2]        │
│                                                  │
└──────────────────────────────────────────────────┘
```

### 9.2 Touch Layout (iPad)

Larger screen = more comfortable spacing + optional gesture shortcuts:

```
┌────────────────────────────────────────────────────────────┐
│                                                            │
│                  [GAME VIEWPORT - centered]                │
│                                                            │
│                                                            │
│                                                            │
│                                                            │
├────────────────────────────────────────────────────────────┤
│                                                            │
│    ┌─────┐                                 [LP]    [HP]   │
│    │     │                                                 │
│    │  ●  │  Virtual Joystick          [LK]    [HK]        │
│    │     │  (larger dead zone                              │
│    └─────┘   for iPad)                [SP1]   [SUPER]     │
│                                                            │
│                              [GRAB shortcut: LP+LK macro] │
└────────────────────────────────────────────────────────────┘
```

### 9.3 Adaptive Layout System

```c
typedef struct {
    float screen_w, screen_h;
    float safe_left, safe_right;   // notch / home indicator
    float safe_top, safe_bottom;
    float dpi_scale;

    // Computed
    Rect  viewport;        // 16:9 game area
    Rect  joystick_zone;   // left third
    Rect  button_zone;     // right third
    float button_radius;   // DPI-adjusted
    float joystick_radius;
} TouchLayout;

void touch_layout_compute(TouchLayout *layout) {
    float aspect = 16.0f / 9.0f;
    // Fit viewport, compute button sizes based on DPI
    // iPad gets 20% larger buttons, wider spacing
    layout->button_radius = (layout->dpi_scale > 2.0f) ? 38 : 32;
    // ... etc
}
```

---

## 10. Rendering Pipeline

### 10.1 SDL3 GPU Renderer Strategy

```
Frame render order (back to front):

1. BACKGROUND
   - Stage parallax layers (2-3 layers, scroll at different rates)
   - Stage floor / ground plane

2. SHADOWS
   - Simple oval shadows beneath fighters

3. FIGHTERS (sorted by Y for depth)
   - Sprite sheet frame lookup from animation state
   - Flip horizontally based on facing direction
   - Color flash on hit (white overlay for 3 frames)

4. EFFECTS
   - Hit sparks (particle system)
   - Projectiles
   - Super move VFX (screen-wide flash, streaks)

5. HUD (screen space, not affected by camera)
   - Health bars (P1 left, P2 right)
   - Round timer (center top)
   - Super meter (bottom)
   - Combo counter (near attacker)
   - Round indicators (dots)

6. TOUCH OVERLAY (topmost, semi-transparent)
   - Virtual joystick
   - Attack buttons
   - Only on mobile

7. POST-PROCESS
   - Screen shake (offset render target)
   - Hit freeze (pause rendering 3-6 frames on heavy hits)
   - Super flash (white fade overlay)
```

### 10.2 Camera System

```c
typedef struct {
    float x, y;          // center point
    float zoom;          // 1.0 = default, <1 = zoom out
    float shake_x, shake_y;
    int   freeze_frames; // "hit stop" — pause everything
} Camera;

void camera_update(Camera *cam, Fighter *p1, Fighter *p2) {
    // Center between players
    float cx = (p1->x + p2->x) / 2.0f;
    float cy = (p1->y + p2->y) / 2.0f - 30.0f; // bias upward

    // Zoom out when players are far apart
    float dist = fabsf(p1->x - p2->x);
    float target_zoom = fmaxf(0.7f, fminf(1.0f, 500.0f / dist));

    // Smooth follow
    cam->x += (cx - cam->x) * 0.1f;
    cam->y += (cy - cam->y) * 0.1f;
    cam->zoom += (target_zoom - cam->zoom) * 0.08f;

    // Decay shake
    cam->shake_x *= 0.85f;
    cam->shake_y *= 0.85f;
}
```

---

## 11. Audio Design

### 11.1 Sound Categories

| Category | Format | Examples | Notes |
|----------|--------|----------|-------|
| Hit SFX | WAV 44.1kHz | light_hit, heavy_hit, block, whiff | < 100ms, low latency |
| Voice | OGG | character callouts, KO screams | Per-fighter |
| UI SFX | WAV | menu select, countdown beep | Instant playback |
| Music | OGG stream | fight BGM, menu BGM, results | Loop points in metadata |
| Ambient | OGG | stage-specific background sounds | Looping |

### 11.2 SDL3 Audio Architecture

```c
// Use SDL3 audio streams
SDL_AudioSpec spec = { SDL_AUDIO_S16LE, 2, 44100 };
SDL_AudioStream *sfx_stream;
SDL_AudioStream *music_stream;

// Separate volume control
float volume_sfx   = 0.8f;
float volume_music  = 0.5f;
float volume_voice  = 0.9f;

// Hit-freeze: skip audio advance during freeze frames
// to keep sounds in sync with visual freeze
```

---

## 12. Matchmaking & Lobby Flow

```
┌─────────┐     ┌──────────┐     ┌───────────┐     ┌─────────┐
│  TITLE   │────→│  MAIN    │────→│ CHARACTER │────→│ MATCH   │
│  SCREEN  │     │  MENU    │     │  SELECT   │     │ SEARCH  │
└─────────┘     └──────────┘     └───────────┘     └────┬────┘
                     │                                   │
                     │                                   ▼
                     │                              ┌─────────┐
                     ├──→ Training Mode             │ LOADING │
                     ├──→ Local VS (same device)    │ (sync)  │
                     ├──→ Leaderboard               └────┬────┘
                     ├──→ Shop / Unlocks                 │
                     └──→ Settings                       ▼
                                                   ┌──────────┐
                                                   │  FIGHT!  │
                                                   │ (3 rounds)│
                                                   └────┬─────┘
                                                        │
                                                        ▼
                                                   ┌──────────┐
                                                   │ RESULTS  │
                                                   │ +ELO     │
                                                   │ +Coins   │
                                                   └──────────┘
```

---

## 13. Anti-Cheat & Security

### 13.1 Threats & Mitigations

| Threat | Mitigation |
|--------|------------|
| Speed hacks | Server-side frame clock; reject inputs arriving too fast |
| Input injection | Input validation (no impossible simultaneous directions) |
| Stat tampering | Server is source of truth for ELO, coins, unlocks |
| Replay manipulation | Server-side input log; client replay is convenience only |
| Rage-quit abuse | Disconnecter forfeits; grace period for genuine drops |
| Impersonation | JWT auth + device fingerprint binding |
| Packet sniffing | TLS for WebSocket (wss://) |

### 13.2 Disconnect Handling

```
Player disconnects:
  0-5 seconds:   Pause match, show "Reconnecting..."
  5-10 seconds:  Continue match, AI takes over for disconnected player
  10+ seconds:   Disconnected player forfeits
  Intentional:   3 rage-quits in 1 hour → 15-min matchmaking ban
```

---

## 14. Platform-Specific Concerns

### 14.1 iOS

```
- SDL3 runs in a UIWindow via SDL_uikit
- Touch events via SDL3 touch API
- App lifecycle: handle SDL_EVENT_DID_ENTER_BACKGROUND
  → Save state, pause game, disconnect gracefully
- MFi controller support via SDL3 gamepad API
- App Store: no loot boxes (Apple policy), IAP via StoreKit
- iPad multitasking: handle resize events, lock to landscape
- Minimum target: iOS 15+
- Notch/Dynamic Island: respect safe area insets
```

### 14.2 Android

```
- SDL3 via NativeActivity / SDL_android
- Touch + back button handling
- Lifecycle: handle onPause/onResume → same as iOS
- Controller: SDL3 gamepad for Bluetooth controllers
- Google Play billing for IAP
- Handle varied screen sizes: phone → tablet → foldable
- Minimum target: API 26 (Android 8.0)
- Cutout/punch-hole: respect display cutout insets
```

### 14.3 Cross-Platform Abstraction

```c
// platform/platform.h
typedef struct {
    float safe_area_top, safe_area_bottom;
    float safe_area_left, safe_area_right;
    float screen_dpi;
    float screen_w, screen_h;
    int   is_tablet;      // iPad or Android tablet
    int   has_notch;
    const char *platform; // "ios", "android", "desktop"
} PlatformInfo;

void platform_init(PlatformInfo *info);
void platform_open_url(const char *url);
void platform_haptic_light(void);   // taptic on hit
void platform_haptic_heavy(void);   // taptic on KO
```

---

## 15. Build & CI/CD Pipeline

```
┌─────────────┐     ┌──────────────┐     ┌──────────────┐
│   GitHub     │────→│  GitHub      │────→│  Artifacts   │
│   Push/PR    │     │  Actions     │     │              │
└─────────────┘     └──────┬───────┘     │ • iOS .ipa   │
                           │             │ • Android.apk│
                    ┌──────┴──────┐      │ • Linux bin  │
                    │             │       └──────┬───────┘
              ┌─────▼─────┐ ┌────▼─────┐        │
              │ Build     │ │ Build    │  ┌─────▼──────┐
              │ Client    │ │ Server   │  │ TestFlight │
              │ (cmake)   │ │ (docker) │  │ Play Store │
              │           │ │          │  │ (internal) │
              └─────┬─────┘ └────┬─────┘  └────────────┘
                    │            │
              ┌─────▼─────┐ ┌───▼──────┐
              │ Unit Tests│ │ Deploy   │
              │ Rollback  │ │ to K8s   │
              │ Determin. │ │ staging  │
              └───────────┘ └──────────┘
```

### CI Jobs

```yaml
# .github/workflows/ci.yml highlights:
client-build:
  matrix: [linux-x64, ios-arm64, android-arm64]
  steps:
    - cmake -B build -DCMAKE_TOOLCHAIN_FILE=...
    - cmake --build build
    - ctest --test-dir build  # determinism tests

server-build:
  steps:
    - npm ci
    - npm run lint
    - npm test
    - docker build -t fury-clash-server .

deploy-staging:
  needs: [client-build, server-build]
  steps:
    - kubectl apply -f deploy/k8s/ --context staging
```

---

## 16. Milestone Roadmap

### Phase 1 — Foundation (Weeks 1-4)
```
[x] SDL3 window, render loop, fixed timestep
[x] Sprite loading, atlas rendering
[x] Single fighter: idle, walk, jump, crouch
[x] Basic physics: gravity, ground, walls
[x] Touch input: virtual joystick + 4 buttons
[ ] Two local fighters on same device (debug)
```

### Phase 2 — Combat Core (Weeks 5-8)
```
[ ] Attack state machine: startup, active, recovery
[ ] Hitbox/hurtbox collision
[ ] Hit reaction: hitstun, knockback, blockstun
[ ] 4 normal attacks per fighter (LP, HP, LK, HK)
[ ] Special move input detection (QCF, DP)
[ ] 2 special moves per fighter
[ ] Combo system (cancel windows, juggle limit)
[ ] Health bar, round system, KO
```

### Phase 3 — Multiplayer (Weeks 9-12)
```
[ ] Node.js relay server (WebSocket)
[ ] Client networking: connect, send/receive inputs
[ ] Rollback netcode: save, restore, resimulate
[ ] Matchmaking queue (Redis + ELO)
[ ] Lobby flow: search → connect → fight → results
[ ] Disconnect handling
[ ] Network stats overlay (ping, rollback frames)
```

### Phase 4 — Content & Polish (Weeks 13-16)
```
[ ] 4 unique fighters with full movesets
[ ] 3 stages with parallax backgrounds
[ ] Complete HUD: health, timer, super, combo counter
[ ] Particle effects: hit sparks, super flash
[ ] Camera: dynamic zoom, screen shake, hit freeze
[ ] Audio: SFX, BGM, announcer
[ ] Character select screen
[ ] Training mode (vs dummy)
```

### Phase 5 — Mobile Polish & Launch (Weeks 17-20)
```
[ ] iOS build, TestFlight
[ ] Android build, internal testing
[ ] iPad layout optimization
[ ] Haptic feedback on hits
[ ] Performance profiling (60fps target on iPhone 12+)
[ ] App Store / Play Store assets
[ ] Server deployment (AWS/GCP)
[ ] Soft launch → feedback → fixes → public launch
```

---

## 17. Risk Register

| Risk | Impact | Likelihood | Mitigation |
|------|--------|------------|------------|
| Rollback netcode too complex | High | Medium | Start simple (input delay first), add rollback incrementally; reference GGPO |
| SDL3 mobile maturity issues | Medium | Medium | Pin to stable release; contribute upstream fixes; have SDL2 fallback plan |
| Touch controls feel bad | High | High | Extensive playtesting; configurable layout; support external controllers |
| App Store rejection | High | Low | Follow guidelines strictly; no emulated violence gore; proper age rating |
| Server costs at scale | Medium | Medium | Relay server is lightweight; horizontal scale; consider peer-to-peer for casual |
| Cheating in ranked | Medium | Medium | Server-side input validation; statistical anomaly detection |
| Content pipeline bottleneck | High | Medium | Hire/contract pixel artist early; establish sprite sheet spec from day 1 |
| 60fps on older devices | Medium | High | Profile early; sprite batching; reduce particles on low-end; LOD system |

---

## Appendix A: Key Library Choices

| Purpose | Library | Why |
|---------|---------|-----|
| Graphics + Input + Audio | SDL3 | Cross-platform, mobile-ready, C-native, GPU renderer |
| Serialization | FlatBuffers | Zero-copy, tiny binary, C + JS support |
| WebSocket (client) | libwebsockets or SDL_net | Lightweight, C-compatible |
| WebSocket (server) | ws (Node.js) | Fast, battle-tested |
| HTTP API | Fastify | Fastest Node.js framework |
| Database | Prisma + PostgreSQL | Type-safe, migrations, good DX |
| Cache/Pubsub | Redis | Matchmaking queue, session state |
| Job Queue | BullMQ | Reliable background processing |
| Build (client) | CMake | SDL3 native, cross-compile support |
| Build (server) | Docker + esbuild | Fast builds, containerized deploy |

---

*This document is the living blueprint. Update as decisions evolve.*
