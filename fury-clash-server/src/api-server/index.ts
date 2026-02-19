import Fastify from 'fastify';
import fastifyJwt from '@fastify/jwt';
import fastifyCors from '@fastify/cors';
import fastifyRateLimit from '@fastify/rate-limit';

import { config } from '../shared/config.js';
import { childLogger } from '../shared/logger.js';
import { isAppError } from '../shared/errors.js';
import { RATE_API_PER_MIN, RATE_AUTH_PER_MIN } from '../shared/constants.js';

import { connectDb, disconnectDb } from './plugins/db.js';
import { redis } from './plugins/redis.js';

import { authRoutes } from './routes/auth.js';
import { profileRoutes } from './routes/profile.js';
import { leaderboardRoutes } from './routes/leaderboard.js';
import { matchmakingRoutes } from './routes/matchmaking.js';
import { shopRoutes } from './routes/shop.js';
import { replayRoutes } from './routes/replay.js';

const log = childLogger('api');

async function build() {
  const app = Fastify({ logger: false });

  /* ── Plugins ──────────────────────────────────────────────────────── */
  await app.register(fastifyCors, { origin: true });
  await app.register(fastifyJwt, { secret: config.jwt.secret });
  await app.register(fastifyRateLimit, {
    max: RATE_API_PER_MIN,
    timeWindow: '1 minute',
    redis,
  });

  /* ── Error handler ────────────────────────────────────────────────── */
  app.setErrorHandler((err, _req, reply) => {
    if (isAppError(err)) {
      reply.code(err.statusCode).send({ error: err.code, message: err.message });
    } else {
      log.error(err, 'Unhandled error');
      reply.code(500).send({ error: 'INTERNAL', message: 'Internal server error' });
    }
  });

  /* ── Routes ───────────────────────────────────────────────────────── */
  await app.register(authRoutes,        { prefix: '/api/auth' });
  await app.register(profileRoutes,     { prefix: '/api/profile' });
  await app.register(leaderboardRoutes, { prefix: '/api/leaderboard' });
  await app.register(matchmakingRoutes, { prefix: '/api/matchmaking' });
  await app.register(shopRoutes,        { prefix: '/api/shop' });
  await app.register(replayRoutes,      { prefix: '/api/replay' });

  app.get('/health', () => ({ status: 'ok', service: 'api' }));

  return app;
}

async function main() {
  await connectDb();
  await redis.connect();

  const app = await build();

  process.on('SIGTERM', async () => {
    log.info('SIGTERM received — shutting down');
    await app.close();
    await disconnectDb();
    await redis.quit();
    process.exit(0);
  });

  await app.listen({ port: config.api.port, host: config.api.host });
  log.info({ port: config.api.port }, 'API server listening');
}

main().catch((err) => {
  log.error(err, 'Fatal startup error');
  process.exit(1);
});
