import { Redis } from 'ioredis';
import { Matcher } from './matcher.js';
import { config } from '../shared/config.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('matchmaker');

async function main() {
  const redis = new Redis(config.redis.url, { lazyConnect: true });
  await redis.connect();
  log.info('Redis connected');

  const matcher = new Matcher(redis);

  const interval = setInterval(async () => {
    try {
      await matcher.scan();
    } catch (err) {
      log.error(err, 'Matchmaker scan error');
    }
  }, config.matchmaker.scanIntervalMs);

  process.on('SIGTERM', async () => {
    log.info('SIGTERM — shutting down matchmaker');
    clearInterval(interval);
    await redis.quit();
    process.exit(0);
  });

  log.info({ scanIntervalMs: config.matchmaker.scanIntervalMs }, 'Matchmaker running');
}

main().catch((err) => {
  log.error(err, 'Fatal matchmaker error');
  process.exit(1);
});
