import { Worker } from 'bullmq';
import { bullmqConnection } from '../shared/config.js';
import { prisma, connectDb } from '../api-server/plugins/db.js';
import { QUEUE_REPLAY } from '../shared/constants.js';
import type { ReplayJobData } from '../shared/types.js';
import { childLogger } from '../shared/logger.js';
import { gzipSync } from 'zlib';

const log = childLogger('replay-worker');

await connectDb();

/**
 * Compress and store replay data.
 * Production would upload to S3/GCS; here we store the URL stub.
 */
const worker = new Worker<ReplayJobData>(
  QUEUE_REPLAY,
  async (job) => {
    const { matchId, roomId, inputLog } = job.data;

    log.info({ matchId, roomId, frames: inputLog.length }, 'Processing replay');

    const raw        = Buffer.from(JSON.stringify(inputLog));
    const compressed = gzipSync(raw);
    const replayUrl  = `replays/${matchId}.json.gz`;

    await prisma.match.update({ where: { id: matchId }, data: { replayUrl } });

    log.info({ matchId, frames: inputLog.length, compressedBytes: compressed.length, replayUrl }, 'Replay stored');
  },
  { connection: bullmqConnection() },
);

worker.on('failed', (job, err) => log.error({ jobId: job?.id, err }, 'Replay job failed'));

process.on('SIGTERM', async () => {
  await worker.close();
  await prisma.$disconnect();
  process.exit(0);
});
