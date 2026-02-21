import type WebSocket from 'ws';
import { InputLog } from './input-log.js';
import {
  decodeClientMsg,
  encodeRelayBatch,
  encodeMatchStart,
  encodeMatchEnd,
  encodeOpponentDisconnect,
  encodePing,
  encodeWaiting,
  encodeLoadout,
  isValidInput,
} from './protocol.js';
import { MsgType, type RoomState, type MatchEndPayload } from '../shared/types.js';
import { MAX_RELAY_BATCH } from '../shared/constants.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('room');

interface SlotInfo {
  ws           : WebSocket;
  playerId     : string;
  fighterId    : number;
  seq          : number;
  pendingFrames: Array<{ frame: number; input: number }>;
}

export interface RoomConfig {
  roomId        : string;
  p1Id          : string;
  p2Id          : string;
  p1Fighter     : number;
  p2Fighter     : number;
  pingIntervalMs: number;
  onEnd         : (room: RelayRoom, payload: MatchEndPayload) => void;
}

export class RelayRoom {
  readonly roomId   : string;
  state             : RoomState = 'waiting';
  readonly inputLog : InputLog  = new InputLog();
  readonly cfg      : RoomConfig;   // exposed for RoomManager

  private slots      : [SlotInfo | null, SlotInfo | null] = [null, null];
  private pingTimer  : NodeJS.Timeout | null = null;
  private frameCount  = 0;
  private startedAt   = 0;

  constructor(cfg: RoomConfig) {
    this.roomId = cfg.roomId;
    this.cfg    = cfg;
  }

  attach(slot: 0 | 1, ws: WebSocket, playerId: string, fighterId: number): boolean {
    if (this.slots[slot] !== null) return false;

    const info: SlotInfo = { ws, playerId, fighterId, seq: 0, pendingFrames: [] };
    this.slots[slot] = info;

    ws.on('message', (data) => this.handleMessage(slot, data as Buffer));
    ws.on('close',   () => this.handleClose(slot));
    ws.on('error',   (err) => log.warn({ roomId: this.roomId, slot, err }, 'WS error'));

    log.info({ roomId: this.roomId, slot, playerId }, 'Slot attached');
    ws.send(encodeWaiting());

    if (this.slots[0] && this.slots[1]) this.startMatch();
    return true;
  }

  private startMatch(): void {
    this.state     = 'active';
    this.startedAt = Date.now();

    for (const slot of [0, 1] as const) {
      this.slots[slot]!.ws.send(encodeMatchStart({
        yourSlot  : slot,
        p1Id      : this.cfg.p1Id,
        p2Id      : this.cfg.p2Id,
        p1Fighter : this.cfg.p1Fighter,
        p2Fighter : this.cfg.p2Fighter,
        roomId    : this.roomId,
      }));
    }

    this.pingTimer = setInterval(() => this.sendPing(), this.cfg.pingIntervalMs);
    log.info({ roomId: this.roomId }, 'Match started');
  }

  private handleMessage(slot: 0 | 1, data: Buffer): void {
    if (this.state !== 'active') return;

    const msg = decodeClientMsg(data);
    if (!msg) return;

    if (msg.type === MsgType.CLIENT_INPUT) {
      if (!isValidInput(msg.input)) {
        log.warn({ roomId: this.roomId, slot, input: msg.input }, 'Invalid input rejected');
        return;
      }

      if (slot === 0) this.inputLog.recordP1(msg.frame, msg.input);
      else            this.inputLog.recordP2(msg.frame, msg.input);

      this.frameCount++;

      const opponentSlot = (slot ^ 1) as 0 | 1;
      const opponent = this.slots[opponentSlot];
      if (opponent && opponent.ws.readyState === 1 /* OPEN */) {
        opponent.pendingFrames.push({ frame: msg.frame, input: msg.input });
        this.flush(opponentSlot);
      }
    } else if (msg.type === MsgType.CLIENT_LOADOUT) {
      /* Forward loadout choice to opponent as SERVER_LOADOUT */
      const opponentSlot = (slot ^ 1) as 0 | 1;
      const opponent = this.slots[opponentSlot];
      if (opponent?.ws.readyState === 1 /* OPEN */) {
        opponent.ws.send(encodeLoadout(msg.loadout));
      }
    }
  }

  private flush(slot: 0 | 1): void {
    const s = this.slots[slot];
    if (!s || s.pendingFrames.length === 0) return;

    while (s.pendingFrames.length > 0) {
      const batch = s.pendingFrames.splice(0, MAX_RELAY_BATCH);
      s.ws.send(encodeRelayBatch(s.seq++, batch));
    }
  }

  private handleClose(slot: 0 | 1): void {
    if (this.state === 'closed') return;
    log.warn({ roomId: this.roomId, slot }, 'Slot disconnected');

    const opponentSlot = (slot ^ 1) as 0 | 1;
    const opponent = this.slots[opponentSlot];
    if (opponent?.ws.readyState === 1) {
      opponent.ws.send(encodeOpponentDisconnect(this.cfg.pingIntervalMs / 1000));
    }

    const winnerSlot = opponentSlot;
    this.end({
      winnerSlot,
      reason        : 'disconnect',
      p1RoundsWon   : slot === 0 ? 0 : 2,
      p2RoundsWon   : slot === 1 ? 0 : 2,
      durationFrames: this.frameCount,
    });
  }

  end(payload: MatchEndPayload): void {
    if (this.state === 'closed') return;
    this.state = 'ending';

    for (const slot of [0, 1] as const) {
      const s = this.slots[slot];
      if (s?.ws.readyState === 1) s.ws.send(encodeMatchEnd(payload));
    }

    if (this.pingTimer) { clearInterval(this.pingTimer); this.pingTimer = null; }

    this.state = 'closed';
    this.cfg.onEnd(this, payload);
    log.info({ roomId: this.roomId, payload }, 'Match ended');
  }

  private sendPing(): void {
    const ts = Date.now();
    for (const slot of [0, 1] as const) {
      const s = this.slots[slot];
      if (s?.ws.readyState === 1) s.ws.send(encodePing(ts));
    }
  }

  getSlotInfo(slot: 0 | 1): SlotInfo | null { return this.slots[slot]; }
  durationSec(): number { return this.startedAt ? Math.floor((Date.now() - this.startedAt) / 1000) : 0; }
}
