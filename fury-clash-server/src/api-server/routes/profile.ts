import type { FastifyInstance } from 'fastify';
import { z } from 'zod';
import { prisma } from '../plugins/db.js';
import { requireAuth, playerId } from '../middleware/auth.js';
import { validate } from '../middleware/validation.js';
import { AppError, ErrorCode } from '../../shared/errors.js';

const updateSchema = z.object({
  username: z.string().min(3).max(20).regex(/^[a-zA-Z0-9_]+$/).optional(),
}).strict();

export async function profileRoutes(app: FastifyInstance): Promise<void> {
  /** GET /api/profile/me */
  app.get('/me', { preHandler: requireAuth }, async (req) => {
    const id = playerId(req);
    const player = await prisma.player.findUniqueOrThrow({ where: { id } });
    return { player: safe(player) };
  });

  /** GET /api/profile/:username */
  app.get<{ Params: { username: string } }>('/:username', async (req) => {
    const player = await prisma.player.findUnique({
      where: { username: req.params.username },
    });
    if (!player) throw new AppError(ErrorCode.NOT_FOUND, 'Player not found', 404);
    return { player: safe(player) };
  });

  /** PATCH /api/profile/me */
  app.patch('/me', { preHandler: requireAuth }, async (req) => {
    const id = playerId(req);
    const body = validate(updateSchema, req.body);

    if (body.username) {
      const exists = await prisma.player.findUnique({ where: { username: body.username } });
      if (exists && exists.id !== id) {
        throw new AppError(ErrorCode.AUTH_USERNAME_TAKEN, 'Username already taken');
      }
    }

    const updated = await prisma.player.update({ where: { id }, data: body });
    return { player: safe(updated) };
  });

  /** GET /api/profile/me/matches */
  app.get('/me/matches', { preHandler: requireAuth }, async (req) => {
    const id = playerId(req);
    const matches = await prisma.match.findMany({
      where: { OR: [{ player1Id: id }, { player2Id: id }] },
      orderBy: { createdAt: 'desc' },
      take: 20,
      include: {
        player1: { select: { username: true } },
        player2: { select: { username: true } },
      },
    });
    return { matches };
  });
}

function safe(p: { id: string; username: string; elo: number; wins: number; losses: number; draws: number; totalGames: number; coins: number; createdAt: Date }) {
  return { id: p.id, username: p.username, elo: p.elo, wins: p.wins, losses: p.losses, draws: p.draws, totalGames: p.totalGames, coins: p.coins, createdAt: p.createdAt };
}
