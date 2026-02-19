import { describe, it, expect } from 'vitest';
import {
  decodeClientMsg,
  encodeRelayBatch,
  encodeMatchStart,
  isValidInput,
  CLIENT_INPUT_MSG_SIZE,
} from '../src/relay-server/protocol.js';
import { MsgType } from '../src/shared/types.js';

describe('Protocol encode/decode', () => {
  describe('CLIENT_INPUT', () => {
    it('decodes a valid CLIENT_INPUT message', () => {
      const buf = Buffer.allocUnsafe(CLIENT_INPUT_MSG_SIZE);
      buf.writeUInt8(MsgType.CLIENT_INPUT, 0);
      buf.writeUInt16LE(42,   1);   // seq
      buf.writeUInt32LE(1234, 3);   // frame
      buf.writeUInt16LE(0b0000_0001_0001_0000, 7);  // input: up + light_punch

      const msg = decodeClientMsg(buf);
      expect(msg).not.toBeNull();
      expect(msg!.type).toBe(MsgType.CLIENT_INPUT);
      if (msg!.type === MsgType.CLIENT_INPUT) {
        expect(msg.seq).toBe(42);
        expect(msg.frame).toBe(1234);
        expect(msg.input).toBe(0b0000_0001_0001_0000);
      }
    });

    it('returns null for truncated CLIENT_INPUT', () => {
      const buf = Buffer.allocUnsafe(4);
      buf.writeUInt8(MsgType.CLIENT_INPUT, 0);
      expect(decodeClientMsg(buf)).toBeNull();
    });
  });

  describe('CLIENT_JOIN', () => {
    it('decodes a valid CLIENT_JOIN JSON message', () => {
      const payload = { roomId: 'abc', playerId: 'p1', token: 'tok', slot: 0 };
      const json    = Buffer.from(JSON.stringify(payload), 'utf8');
      const buf     = Buffer.allocUnsafe(1 + json.length);
      buf.writeUInt8(MsgType.CLIENT_JOIN, 0);
      json.copy(buf, 1);

      const msg = decodeClientMsg(buf);
      expect(msg).not.toBeNull();
      expect(msg!.type).toBe(MsgType.CLIENT_JOIN);
    });
  });

  describe('encodeRelayBatch', () => {
    it('produces correct byte layout', () => {
      const frames = [{ frame: 100, input: 0x0F }, { frame: 101, input: 0x10 }];
      const buf = encodeRelayBatch(7, frames);

      expect(buf[0]).toBe(MsgType.SERVER_RELAY);
      expect(buf.readUInt16LE(1)).toBe(7);       // seq
      expect(buf.readUInt8(3)).toBe(2);           // count
      expect(buf.readUInt32LE(4)).toBe(100);      // frame 0
      expect(buf.readUInt16LE(8)).toBe(0x0F);     // input 0
      expect(buf.readUInt32LE(10)).toBe(101);     // frame 1
      expect(buf.readUInt16LE(14)).toBe(0x10);    // input 1
    });
  });

  describe('encodeMatchStart', () => {
    it('starts with SERVER_START byte', () => {
      const buf = encodeMatchStart({ yourSlot: 0, p1Id: 'a', p2Id: 'b', p1Fighter: 0, p2Fighter: 1, roomId: 'r1' });
      expect(buf[0]).toBe(MsgType.SERVER_START);
      const payload = JSON.parse(buf.subarray(1).toString('utf8'));
      expect(payload.yourSlot).toBe(0);
      expect(payload.roomId).toBe('r1');
    });
  });

  describe('isValidInput', () => {
    it('accepts normal directional input', () => {
      expect(isValidInput(0b0001)).toBe(true);   // up only
      expect(isValidInput(0b0100)).toBe(true);   // left only
    });

    it('rejects up+down simultaneously', () => {
      expect(isValidInput(0b0011)).toBe(false);
    });

    it('rejects left+right simultaneously', () => {
      expect(isValidInput(0b1100)).toBe(false);
    });

    it('accepts zero (no input)', () => {
      expect(isValidInput(0)).toBe(true);
    });
  });
});
