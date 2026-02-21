/**
 * Binary protocol encode/decode.
 *
 * CLIENT → SERVER (CLIENT_INPUT):
 *   1B type | 2B seq (LE) | 4B frame (LE) | 2B input (LE)  = 9 bytes
 *
 * SERVER → CLIENT (SERVER_RELAY batch):
 *   1B type | 2B seq (LE) | 1B count | [4B frame + 2B input] × count
 *
 * CLIENT → SERVER (CLIENT_JOIN):
 *   1B type | variable JSON
 *
 * SERVER → CLIENT (SERVER_START / SERVER_END):
 *   1B type | variable JSON
 */

import { MsgType } from '../shared/types.js';

export const CLIENT_INPUT_MSG_SIZE = 9;

// ── Decode ────────────────────────────────────────────────────────────────────

export interface DecodedJoin {
  type: MsgType.CLIENT_JOIN;
  roomId: string;
  playerId: string;
  token: string;
  slot: 0 | 1;
}

export interface DecodedInput {
  type: MsgType.CLIENT_INPUT;
  seq: number;
  frame: number;
  input: number;
}

export interface DecodedPong {
  type: MsgType.CLIENT_PONG;
}

export interface DecodedLoadout {
  type: MsgType.CLIENT_LOADOUT;
  loadout: number;   /* 0=none 1=fire 2=frost */
}

export type DecodedMsg = DecodedJoin | DecodedInput | DecodedPong | DecodedLoadout;

export function decodeClientMsg(data: Buffer): DecodedMsg | null {
  if (data.length < 1) return null;
  const type = data.readUInt8(0);

  switch (type) {
    case MsgType.CLIENT_JOIN: {
      try {
        const json = JSON.parse(data.subarray(1).toString('utf8'));
        return { type: MsgType.CLIENT_JOIN, ...json } as DecodedJoin;
      } catch { return null; }
    }

    case MsgType.CLIENT_INPUT: {
      if (data.length < CLIENT_INPUT_MSG_SIZE) return null;
      return {
        type : MsgType.CLIENT_INPUT,
        seq  : data.readUInt16LE(1),
        frame: data.readUInt32LE(3),
        input: data.readUInt16LE(7),
      };
    }

    case MsgType.CLIENT_PONG:
      return { type: MsgType.CLIENT_PONG };

    case MsgType.CLIENT_LOADOUT:
      if (data.length < 2) return null;
      return { type: MsgType.CLIENT_LOADOUT, loadout: data.readUInt8(1) };

    default:
      return null;
  }
}

// ── Encode ────────────────────────────────────────────────────────────────────

/** SERVER_RELAY: batch of opponent inputs */
export function encodeRelayBatch(
  seq: number,
  frames: Array<{ frame: number; input: number }>,
): Buffer {
  const count = Math.min(frames.length, 255);
  const buf   = Buffer.allocUnsafe(4 + count * 6);
  buf.writeUInt8(MsgType.SERVER_RELAY, 0);
  buf.writeUInt16LE(seq, 1);
  buf.writeUInt8(count, 3);
  for (let i = 0; i < count; i++) {
    buf.writeUInt32LE(frames[i].frame, 4 + i * 6);
    buf.writeUInt16LE(frames[i].input, 8 + i * 6);
  }
  return buf;
}

/** SERVER_START */
export function encodeMatchStart(payload: {
  yourSlot: 0 | 1;
  p1Id: string;
  p2Id: string;
  p1Fighter: number;
  p2Fighter: number;
  roomId: string;
}): Buffer {
  return encodeJson(MsgType.SERVER_START, payload);
}

/** SERVER_END */
export function encodeMatchEnd(payload: {
  winnerSlot: 0 | 1 | -1;
  reason: string;
  p1RoundsWon: number;
  p2RoundsWon: number;
  durationFrames: number;
}): Buffer {
  return encodeJson(MsgType.SERVER_END, payload);
}

/** SERVER_WAITING */
export function encodeWaiting(): Buffer {
  return encodeJson(MsgType.SERVER_WAITING, { status: 'waiting' });
}

/** SERVER_PING */
export function encodePing(timestamp: number): Buffer {
  const buf = Buffer.allocUnsafe(9);
  buf.writeUInt8(MsgType.SERVER_PING, 0);
  buf.writeBigUInt64LE(BigInt(timestamp), 1);
  return buf;
}

/** SERVER_OPPONENT_DISCONNECT */
export function encodeOpponentDisconnect(graceSec: number): Buffer {
  return encodeJson(MsgType.SERVER_OPPONENT_DISCONNECT, { graceSec });
}

/** SERVER_LOADOUT — relay opponent's chosen loadout byte */
export function encodeLoadout(loadoutByte: number): Buffer {
  const buf = Buffer.allocUnsafe(2);
  buf.writeUInt8(MsgType.SERVER_LOADOUT, 0);
  buf.writeUInt8(loadoutByte & 0xFF, 1);
  return buf;
}

/** SERVER_ERROR */
export function encodeError(code: string, message: string): Buffer {
  return encodeJson(MsgType.SERVER_ERROR, { code, message });
}

function encodeJson(type: MsgType, payload: object): Buffer {
  const json = Buffer.from(JSON.stringify(payload), 'utf8');
  const buf  = Buffer.allocUnsafe(1 + json.length);
  buf.writeUInt8(type, 0);
  json.copy(buf, 1);
  return buf;
}

// ── Validation ────────────────────────────────────────────────────────────────

const INVALID_MASKS = [0b0011, 0b1100];   // up+down, left+right

export function isValidInput(input: number): boolean {
  for (const mask of INVALID_MASKS) {
    if ((input & mask) === mask) return false;
  }
  return true;
}
