import { v4 as uuidv4 } from 'uuid';
import type { Redis } from 'ioredis';
import { MatchQueue } from './queue.js';
import { REGIONS, relayEndpointForRegion } from './region.js';
import { KEY_MATCH_ROOM, CHANNEL_MATCH_FOUND } from '../shared/constants.js';
import type { QueueEntry, MatchResult, Region } from '../shared/types.js';
import { config } from '../shared/config.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('matcher');

export class Matcher {
  private queue: MatchQueue;

  constructor(private readonly redis: Redis) {
    this.queue = new MatchQueue(redis);
  }

  async scan(): Promise<void> {
    for (const region of REGIONS) {
      await this.scanRegion(region);
    }
  }

  private async scanRegion(region: Region): Promise<void> {
    const entries = await this.queue.dequeue(region);
    if (entries.length < 2) return;

    const matched = new Set<string>();

    for (let i = 0; i < entries.length; i++) {
      if (matched.has(entries[i].playerId)) continue;
      const a = entries[i];
      const eloRange = this.eloRange(a);

      for (let j = i + 1; j < entries.length; j++) {
        if (matched.has(entries[j].playerId)) continue;
        const b = entries[j];

        if (Math.abs(a.elo - b.elo) <= eloRange) {
          matched.add(a.playerId);
          matched.add(b.playerId);
          await this.createMatch(a, b, region);
          break;
        }
      }
    }
  }

  private eloRange(entry: QueueEntry): number {
    const waitSec = (Date.now() - entry.queuedAt) / 1000;
    const expansion = Math.floor(waitSec * config.matchmaker.eloRangeExpansionPerSec);
    return Math.min(
      config.matchmaker.eloInitialRange + expansion,
      config.matchmaker.eloMaxRange,
    );
  }

  private async createMatch(p1: QueueEntry, p2: QueueEntry, region: Region): Promise<void> {
    const roomId = uuidv4();
    const relay  = relayEndpointForRegion(region, config.matchmaker.relayEndpoint);

    await Promise.all([this.queue.remove(p1), this.queue.remove(p2)]);

    try {
      const res = await fetch(`${relay.replace('ws', 'http')}/rooms`, {
        method : 'POST',
        headers: { 'Content-Type': 'application/json' },
        body   : JSON.stringify({
          roomId,
          p1Id      : p1.playerId,
          p2Id      : p2.playerId,
          p1Fighter : p1.fighterId,
          p2Fighter : p2.fighterId,
        }),
      });
      if (!res.ok) throw new Error(`Relay returned ${res.status}`);
    } catch (err) {
      log.error({ err, roomId }, 'Failed to create relay room');
      return;
    }

    const result: MatchResult = { roomId, player1: p1, player2: p2, relayEndpoint: relay };

    await this.redis.set(
      KEY_MATCH_ROOM(roomId),
      JSON.stringify({ p1Id: p1.playerId, p2Id: p2.playerId, state: 'waiting', createdAt: Date.now() }),
      'EX', 60,
    );

    await Promise.all([
      this.redis.publish(CHANNEL_MATCH_FOUND, JSON.stringify({ playerId: p1.playerId, result })),
      this.redis.publish(CHANNEL_MATCH_FOUND, JSON.stringify({ playerId: p2.playerId, result })),
    ]);

    log.info({ roomId, p1: p1.playerId, p2: p2.playerId, region }, 'Match created');
  }
}
