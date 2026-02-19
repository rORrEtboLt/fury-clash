import { Worker } from 'bullmq';
import { bullmqConnection } from '../shared/config.js';
import { QUEUE_ANALYTICS } from '../shared/constants.js';
import type { AnalyticsJobData } from '../shared/types.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('analytics-worker');

const worker = new Worker<AnalyticsJobData>(
  QUEUE_ANALYTICS,
  async (job) => {
    const { event, payload, timestamp } = job.data;
    log.info({ event, payload, timestamp }, 'Analytics event');
  },
  { connection: bullmqConnection() },
);

worker.on('failed', (job, err) => log.error({ jobId: job?.id, err }, 'Analytics job failed'));

process.on('SIGTERM', async () => {
  await worker.close();
  process.exit(0);
});
