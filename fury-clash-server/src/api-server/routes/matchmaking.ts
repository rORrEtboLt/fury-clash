import type { FastifyInstance } from 'fastify';
import { z } from 'zod';
import { redis } from '../plugins/redis.js';
import { requireAuth, playerId } from '../middleware/auth.js';
import { validate } from '../middleware/validation.js';
import { prisma } from '../plugins/db.js';
import { AppError, ErrorCode } from '../../shared/errors.js';
import {
  KEY_MATCH_QUEUE_REGION,
  KEY_PLAYER_QUEUE_META,
  RAGEQUIT_WINDOW_SEC,
  RAGEQUIT_LIMIT,
} from '../../shared/constants.js';
import type { QueueEntry, Region } from '../../shared/types.js';

const joinSchema = z.object({
  fighterId : z.number().int().min(0).max(3),
  region    : z.enum(['us-east', 'us-west', 'eu-west', 'ap-south', 'ap-east']),
});

export async function matchmakingRoutes(app: FastifyInstance): Promise<void> {
  /** POST /api/matchmaking/queue — join matchmaking */
  app.post('/queue', { preHandler: requireAuth }, async (req, reply) => {
    const id   = playerId(req);
    const body = validate(joinSchema, req.body);

    /* Check ragequit ban */
    const rqKey = `player:ragequits:${id}`;
    const rqCount = await redis.llen(rqKey);
    if (rqCount >= RAGEQUIT_LIMIT) {
      throw new AppError(ErrorCode.MATCH_BANNED, 'You are temporarily banned from matchmaking', 403);
    }

    /* Check not already queued */
    const metaKey = KEY_PLAYER_QUEUE_META(id);
    const already = await redis.exists(metaKey);
    if (already) {
      throw new AppError(ErrorCode.MATCH_ALREADY_QUEUED, 'Already in queue');
    }

    const player = await prisma.player.findUniqueOrThrow({
      where: { id },
      select: { id: true, username: true, elo: true },
    });

    const entry: QueueEntry = {
      playerId  : player.id,
      username  : player.username,
      elo       : player.elo,
      fighterId : body.fighterId,
      region    : body.region as Region,
      queuedAt  : Date.now(),
    };

    const queueKey = KEY_MATCH_QUEUE_REGION(body.region);
    await Promise.all([
      redis.zadd(queueKey, player.elo, player.id),
      redis.set(metaKey, JSON.stringify(entry), 'EX', 120),  // 2-min TTL
    ]);

    return reply.code(202).send({ status: 'queued', position: await redis.zrank(queueKey, player.id) });
  });

  /** DELETE /api/matchmaking/queue — leave queue */
  app.delete('/queue', { preHandler: requireAuth }, async (req, reply) => {
    const id      = playerId(req);
    const metaRaw = await redis.get(KEY_PLAYER_QUEUE_META(id));
    if (!metaRaw) throw new AppError(ErrorCode.MATCH_NOT_IN_QUEUE, 'Not in queue');

    const entry: QueueEntry = JSON.parse(metaRaw);
    const queueKey = KEY_MATCH_QUEUE_REGION(entry.region);

    await Promise.all([
      redis.zrem(queueKey, id),
      redis.del(KEY_PLAYER_QUEUE_META(id)),
    ]);

    return reply.code(204).send();
  });

  /** GET /api/matchmaking/status */
  app.get('/status', { preHandler: requireAuth }, async (req) => {
    const id      = playerId(req);
    const metaRaw = await redis.get(KEY_PLAYER_QUEUE_META(id));
    if (!metaRaw) return { status: 'idle' };

    const entry: QueueEntry = JSON.parse(metaRaw);
    const queueKey = KEY_MATCH_QUEUE_REGION(entry.region);
    const position = await redis.zrank(queueKey, id);

    return { status: 'queued', position, queuedAt: entry.queuedAt };
  });
}
