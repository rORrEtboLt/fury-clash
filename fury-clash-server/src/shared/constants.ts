// ── Game Constants ────────────────────────────────────────────────────────────

export const TICK_RATE            = 60;
export const ROUND_TIME_SEC       = 99;
export const ROUNDS_TO_WIN        = 2;
export const MAX_FIGHTERS         = 4;   // total characters available
export const MAX_HEALTH           = 1000;

// ── Protocol ─────────────────────────────────────────────────────────────────

/** Byte sizes for wire format */
export const CLIENT_INPUT_MSG_SIZE   = 9;   // 1+2+4+2
export const SERVER_RELAY_HEADER     = 4;   // 1+2+1
export const RELAY_FRAME_ENTRY_SIZE  = 6;   // 4+2

/** Max frames batched per relay message */
export const MAX_RELAY_BATCH         = 8;

/** Input validation: these direction combos are impossible on a real controller */
export const INVALID_INPUT_MASKS = [
  0b0000_0000_0000_0011,  // up + down simultaneously
  0b0000_0000_0000_1100,  // left + right simultaneously
];

// ── ELO ───────────────────────────────────────────────────────────────────────

export const ELO_DEFAULT             = 1000;
export const ELO_K_NEW_PLAYER        = 32;   // < 30 games
export const ELO_K_ESTABLISHED       = 16;   // >= 30 games
export const ELO_NEW_PLAYER_THRESHOLD = 30;

// ── Redis Key Patterns ────────────────────────────────────────────────────────

export const KEY_MATCH_QUEUE         = 'match:queue:ranked';
export const KEY_MATCH_QUEUE_REGION  = (r: string) => `match:queue:${r}`;
export const KEY_MATCH_ROOM          = (id: string) => `match:room:${id}`;
export const KEY_PLAYER_SESSION      = (id: string) => `player:session:${id}`;
export const KEY_PLAYER_ONLINE       = (id: string) => `player:online:${id}`;
export const KEY_RELAY_ROOMS         = (serverId: string) => `relay:rooms:${serverId}`;
export const KEY_PLAYER_QUEUE_META   = (id: string) => `player:queue:${id}`;

// ── Queue Channels ────────────────────────────────────────────────────────────

export const CHANNEL_MATCH_FOUND     = 'match:found';
export const CHANNEL_ROOM_CLOSE      = 'room:close';

// ── BullMQ Queue Names ────────────────────────────────────────────────────────

export const QUEUE_ELO               = 'elo';
export const QUEUE_REPLAY            = 'replay';
export const QUEUE_ANALYTICS         = 'analytics';

// ── Rate Limits ───────────────────────────────────────────────────────────────

export const RATE_AUTH_PER_MIN       = 10;
export const RATE_API_PER_MIN        = 120;

// ── Disconnect / Anti-cheat ───────────────────────────────────────────────────

export const RAGEQUIT_WINDOW_SEC     = 3600;   // 1 hour
export const RAGEQUIT_LIMIT          = 3;
export const RAGEQUIT_BAN_MIN        = 15;

export const MAX_INPUT_ADVANCE_FRAMES = 10;    // reject inputs too far ahead
