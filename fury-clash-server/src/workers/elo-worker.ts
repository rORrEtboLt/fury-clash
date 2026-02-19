import { Worker } from 'bullmq';
import { bullmqConnection } from '../shared/config.js';
import { prisma, connectDb } from '../api-server/plugins/db.js';
import { calculateElo } from '../matchmaker/elo.js';
import { QUEUE_ELO } from '../shared/constants.js';
import type { EloJobData } from '../shared/types.js';
import { childLogger } from '../shared/logger.js';

const log = childLogger('elo-worker');

await connectDb();

const worker = new Worker<EloJobData>(
  QUEUE_ELO,
  async (job) => {
    const { matchId, p1Id, p2Id, winnerId, p1Elo, p2Elo, p1TotalGames, p2TotalGames } = job.data;

    log.info({ matchId, p1Id, p2Id }, 'Processing ELO update');

    const winner = winnerId === p1Id ? '0' : winnerId === p2Id ? '1' : 'draw';
    const change = calculateElo(p1Elo, p2Elo, p1TotalGames, p2TotalGames, winner as '0' | '1' | 'draw');

    await prisma.$transaction([
      prisma.player.update({
        where: { id: p1Id },
        data: {
          elo       : change.p1NewElo,
          totalGames: { increment: 1 },
          wins      : winner === '0' ? { increment: 1 } : undefined,
          losses    : winner === '1' ? { increment: 1 } : undefined,
          draws     : winner === 'draw' ? { increment: 1 } : undefined,
        },
      }),
      prisma.player.update({
        where: { id: p2Id },
        data: {
          elo       : change.p2NewElo,
          totalGames: { increment: 1 },
          wins      : winner === '1' ? { increment: 1 } : undefined,
          losses    : winner === '0' ? { increment: 1 } : undefined,
          draws     : winner === 'draw' ? { increment: 1 } : undefined,
        },
      }),
      prisma.match.update({
        where: { id: matchId },
        data: {
          p1EloChange: change.p1Delta,
          p2EloChange: change.p2Delta,
          p1EloAfter : change.p1NewElo,
          p2EloAfter : change.p2NewElo,
        },
      }),
    ]);

    log.info({ matchId, p1Delta: change.p1Delta, p2Delta: change.p2Delta }, 'ELO updated');
  },
  { connection: bullmqConnection() },
);

worker.on('failed', (job, err) => log.error({ jobId: job?.id, err }, 'ELO job failed'));

process.on('SIGTERM', async () => {
  await worker.close();
  await prisma.$disconnect();
  process.exit(0);
});
