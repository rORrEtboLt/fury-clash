import type { Redis } from 'ioredis';
import { KEY_MATCH_QUEUE_REGION, KEY_PLAYER_QUEUE_META } from '../shared/constants.js';
import type { QueueEntry, Region } from '../shared/types.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('queue');

export class MatchQueue {
  constructor(private readonly redis: Redis) {}

  async dequeue(region: Region): Promise<QueueEntry[]> {
    const key     = KEY_MATCH_QUEUE_REGION(region);
    const members = await this.redis.zrangebyscore(key, '-inf', '+inf', 'WITHSCORES');

    const entries: QueueEntry[] = [];
    for (let i = 0; i < members.length; i += 2) {
      const playerId = members[i];
      const metaRaw  = await this.redis.get(KEY_PLAYER_QUEUE_META(playerId));
      if (!metaRaw) {
        await this.redis.zrem(key, playerId);
        continue;
      }
      entries.push(JSON.parse(metaRaw) as QueueEntry);
    }
    return entries;
  }

  async remove(entry: QueueEntry): Promise<void> {
    const key = KEY_MATCH_QUEUE_REGION(entry.region);
    await Promise.all([
      this.redis.zrem(key, entry.playerId),
      this.redis.del(KEY_PLAYER_QUEUE_META(entry.playerId)),
    ]);
    log.debug({ playerId: entry.playerId, region: entry.region }, 'Removed from queue');
  }

  async size(region: Region): Promise<number> {
    return this.redis.zcard(KEY_MATCH_QUEUE_REGION(region));
  }
}
