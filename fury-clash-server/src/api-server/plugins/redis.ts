import { Redis } from 'ioredis';
import { config } from '../../shared/config.js';
import { childLogger } from '../../shared/logger.js';

const log = childLogger('redis');

export function createRedisClient(name = 'default'): Redis {
  const client = new Redis(config.redis.url, {
    lazyConnect: true,
    maxRetriesPerRequest: 3,
  });

  client.on('connect', () => log.info({ name }, 'Redis connected'));
  client.on('error', (err: Error) => log.error({ name, err }, 'Redis error'));
  client.on('reconnecting', () => log.warn({ name }, 'Redis reconnecting'));

  return client;
}

export const redis = createRedisClient('api');
