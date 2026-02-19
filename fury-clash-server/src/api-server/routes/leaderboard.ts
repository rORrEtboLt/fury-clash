import type { FastifyInstance } from 'fastify';
import { prisma } from '../plugins/db.js';

export async function leaderboardRoutes(app: FastifyInstance): Promise<void> {
  /** GET /api/leaderboard?limit=50&offset=0 */
  app.get<{ Querystring: { limit?: string; offset?: string } }>('/', async (req) => {
    const limit  = Math.min(parseInt(req.query.limit  ?? '50'), 100);
    const offset = parseInt(req.query.offset ?? '0');

    const [players, total] = await Promise.all([
      prisma.player.findMany({
        select: { id: true, username: true, elo: true, wins: true, losses: true, totalGames: true },
        orderBy: { elo: 'desc' },
        take: limit,
        skip: offset,
      }),
      prisma.player.count(),
    ]);

    const ranked = players.map((p, i) => ({ rank: offset + i + 1, ...p }));
    return { players: ranked, total, limit, offset };
  });
}
