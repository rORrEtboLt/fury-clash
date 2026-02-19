import type { FastifyInstance } from 'fastify';
import bcrypt from 'bcryptjs';
import { z } from 'zod';
import { prisma } from '../plugins/db.js';
import { validate } from '../middleware/validation.js';
import { AppError, ErrorCode } from '../../shared/errors.js';
import { config } from '../../shared/config.js';

const registerSchema = z.object({
  username : z.string().min(3).max(20).regex(/^[a-zA-Z0-9_]+$/),
  email    : z.string().email(),
  password : z.string().min(6).max(72),
});

const loginSchema = z.object({
  username : z.string(),
  password : z.string(),
});

export async function authRoutes(app: FastifyInstance): Promise<void> {
  /** POST /api/auth/register */
  app.post('/register', async (req, reply) => {
    const body = validate(registerSchema, req.body);

    const existing = await prisma.player.findFirst({
      where: { OR: [{ username: body.username }, { email: body.email }] },
    });
    if (existing?.username === body.username) {
      throw new AppError(ErrorCode.AUTH_USERNAME_TAKEN, 'Username already taken');
    }
    if (existing?.email === body.email) {
      throw new AppError(ErrorCode.AUTH_EMAIL_TAKEN, 'Email already registered');
    }

    const passwordHash = await bcrypt.hash(body.password, config.bcrypt.rounds);
    const player = await prisma.player.create({
      data: { username: body.username, email: body.email, passwordHash },
    });

    const token = app.jwt.sign(
      { sub: player.id, username: player.username },
      { expiresIn: config.jwt.expiresIn },
    );

    return reply.code(201).send({ token, player: safePlayer(player) });
  });

  /** POST /api/auth/login */
  app.post('/login', async (req, reply) => {
    const body = validate(loginSchema, req.body);

    const player = await prisma.player.findUnique({
      where: { username: body.username },
    });
    if (!player) {
      throw new AppError(ErrorCode.AUTH_INVALID_CREDENTIALS, 'Invalid credentials', 401);
    }

    const valid = await bcrypt.compare(body.password, player.passwordHash);
    if (!valid) {
      throw new AppError(ErrorCode.AUTH_INVALID_CREDENTIALS, 'Invalid credentials', 401);
    }

    const token = app.jwt.sign(
      { sub: player.id, username: player.username },
      { expiresIn: config.jwt.expiresIn },
    );

    return reply.send({ token, player: safePlayer(player) });
  });
}

function safePlayer(p: { id: string; username: string; elo: number; wins: number; losses: number; coins: number; createdAt: Date }) {
  return { id: p.id, username: p.username, elo: p.elo, wins: p.wins, losses: p.losses, coins: p.coins, createdAt: p.createdAt };
}
