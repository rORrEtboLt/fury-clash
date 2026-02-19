import type { RelayRoom } from './relay-room.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('disconnect');

export type DisconnectSlot = 0 | 1;

interface DisconnectState {
  slot       : DisconnectSlot;
  startedAt  : number;
  timer      : NodeJS.Timeout;
}

/**
 * Manages graceful disconnect handling per room.
 *
 * Timeline:
 *   0-5s   → opponent sees "Reconnecting…"
 *   5-10s  → AI takes over (not implemented — server just stalls)
 *   10s+   → disconnected player forfeits
 */
export class DisconnectHandler {
  private active: Map<string, DisconnectState> = new Map();

  constructor(
    private readonly graceSec: number,
    private readonly onForfeit: (room: RelayRoom, loserSlot: DisconnectSlot) => void,
  ) {}

  start(room: RelayRoom, slot: DisconnectSlot): void {
    const key = `${room.roomId}:${slot}`;
    if (this.active.has(key)) return;   // already tracking

    log.info({ roomId: room.roomId, slot }, 'Disconnect timer started');

    const timer = setTimeout(() => {
      log.warn({ roomId: room.roomId, slot }, 'Disconnect forfeit triggered');
      this.active.delete(key);
      this.onForfeit(room, slot);
    }, this.graceSec * 1000);

    this.active.set(key, { slot, startedAt: Date.now(), timer });
  }

  cancel(room: RelayRoom, slot: DisconnectSlot): void {
    const key = `${room.roomId}:${slot}`;
    const state = this.active.get(key);
    if (!state) return;

    clearTimeout(state.timer);
    this.active.delete(key);
    log.info({ roomId: room.roomId, slot }, 'Disconnect timer cancelled (reconnected)');
  }

  secondsElapsed(room: RelayRoom, slot: DisconnectSlot): number {
    const key = `${room.roomId}:${slot}`;
    const state = this.active.get(key);
    if (!state) return 0;
    return Math.floor((Date.now() - state.startedAt) / 1000);
  }

  isDisconnected(room: RelayRoom, slot: DisconnectSlot): boolean {
    return this.active.has(`${room.roomId}:${slot}`);
  }

  clear(room: RelayRoom): void {
    for (const slot of [0, 1] as const) {
      this.cancel(room, slot);
    }
  }
}
