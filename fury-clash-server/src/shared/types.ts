// ── Player & Auth ─────────────────────────────────────────────────────────────

export interface JwtPayload {
  sub: string;   // playerId
  username: string;
  iat?: number;
  exp?: number;
}

export interface PlayerSession {
  playerId: string;
  username: string;
  elo: number;
}

// ── Matchmaking ───────────────────────────────────────────────────────────────

export type Region = 'us-east' | 'us-west' | 'eu-west' | 'ap-south' | 'ap-east';

export interface QueueEntry {
  playerId: string;
  username: string;
  elo: number;
  fighterId: number;
  region: Region;
  queuedAt: number;   // unix ms
}

export interface MatchResult {
  roomId: string;
  player1: QueueEntry;
  player2: QueueEntry;
  relayEndpoint: string;
}

// ── Relay Protocol ────────────────────────────────────────────────────────────

export const enum MsgType {
  // Client → Server
  CLIENT_JOIN     = 0x01,   // join a relay room
  CLIENT_INPUT    = 0x02,   // per-frame input
  CLIENT_PONG     = 0x03,   // reply to SERVER_PING

  // Server → Client
  SERVER_WAITING  = 0x10,   // waiting for opponent
  SERVER_START    = 0x11,   // both players connected, game begins
  SERVER_RELAY    = 0x12,   // opponent input batch
  SERVER_END      = 0x13,   // match over
  SERVER_PING     = 0x14,   // keep-alive ping
  SERVER_ERROR    = 0x15,   // error message
  SERVER_RECONNECT= 0x16,   // opponent reconnected
  SERVER_OPPONENT_DISCONNECT = 0x17,
}

/** CLIENT_INPUT wire format: 1B type | 2B seq | 4B frame | 2B input = 9 bytes */
export interface ClientInputMsg {
  seq: number;
  frame: number;
  input: number;  // 16-bit bitfield
}

/** SERVER_RELAY wire format: 1B type | 2B seq | 1B count | (4B frame + 2B input)×N */
export interface RelayBatch {
  seq: number;
  frames: Array<{ frame: number; input: number }>;
}

/** CLIENT_JOIN wire format: 1B type | variable JSON payload */
export interface ClientJoinMsg {
  roomId: string;
  playerId: string;
  token: string;    // JWT for auth
  slot: 0 | 1;      // which player slot
}

// ── Input Bitfield ────────────────────────────────────────────────────────────

export const INPUT_UP           = 1 << 0;
export const INPUT_DOWN         = 1 << 1;
export const INPUT_LEFT         = 1 << 2;
export const INPUT_RIGHT        = 1 << 3;
export const INPUT_LIGHT_PUNCH  = 1 << 4;
export const INPUT_HEAVY_PUNCH  = 1 << 5;
export const INPUT_LIGHT_KICK   = 1 << 6;
export const INPUT_HEAVY_KICK   = 1 << 7;
export const INPUT_SPECIAL1     = 1 << 8;
export const INPUT_SPECIAL2     = 1 << 9;
export const INPUT_SUPER        = 1 << 10;
export const INPUT_GRAB         = 1 << 11;

// ── Room State ────────────────────────────────────────────────────────────────

export type RoomState = 'waiting' | 'active' | 'ending' | 'closed';

export interface RoomInfo {
  roomId: string;
  p1Id: string;
  p2Id: string;
  state: RoomState;
  createdAt: number;
  startedAt?: number;
  endedAt?: number;
}

// ── Match End ─────────────────────────────────────────────────────────────────

export interface MatchEndPayload {
  winnerSlot: 0 | 1 | -1;    // -1 = draw / disconnect
  reason: 'ko' | 'timeout' | 'disconnect' | 'forfeit';
  p1RoundsWon: number;
  p2RoundsWon: number;
  durationFrames: number;
}

// ── ELO ───────────────────────────────────────────────────────────────────────

export interface EloChange {
  p1Delta: number;
  p2Delta: number;
  p1NewElo: number;
  p2NewElo: number;
}

// ── Worker Jobs ───────────────────────────────────────────────────────────────

export interface EloJobData {
  matchId: string;
  p1Id: string;
  p2Id: string;
  winnerId: string | null;
  p1Elo: number;
  p2Elo: number;
  p1TotalGames: number;
  p2TotalGames: number;
}

export interface ReplayJobData {
  matchId: string;
  roomId: string;
  inputLog: Array<{ frame: number; p1: number; p2: number }>;
}

export interface AnalyticsJobData {
  event: string;
  payload: Record<string, unknown>;
  timestamp: number;
}
