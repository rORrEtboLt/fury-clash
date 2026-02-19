import type { FastifyInstance } from 'fastify';
import { prisma } from '../plugins/db.js';
import { requireAuth } from '../middleware/auth.js';
import { AppError, ErrorCode } from '../../shared/errors.js';

export async function replayRoutes(app: FastifyInstance): Promise<void> {
  /** GET /api/replay/:matchId */
  app.get<{ Params: { matchId: string } }>(
    '/:matchId',
    { preHandler: requireAuth },
    async (req) => {
      const match = await prisma.match.findUnique({
        where: { id: req.params.matchId },
        select: { id: true, replayUrl: true, player1Id: true, player2Id: true, createdAt: true },
      });

      if (!match) throw new AppError(ErrorCode.NOT_FOUND, 'Match not found', 404);
      if (!match.replayUrl) throw new AppError(ErrorCode.NOT_FOUND, 'Replay not available', 404);

      return { matchId: match.id, replayUrl: match.replayUrl, createdAt: match.createdAt };
    },
  );
}
