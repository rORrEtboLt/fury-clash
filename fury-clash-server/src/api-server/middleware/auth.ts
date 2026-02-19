import type { FastifyRequest, FastifyReply } from 'fastify';
import { AppError, ErrorCode } from '../../shared/errors.js';

export async function requireAuth(
  request: FastifyRequest,
  reply: FastifyReply,
): Promise<void> {
  try {
    await request.jwtVerify();
  } catch {
    throw new AppError(ErrorCode.AUTH_TOKEN_INVALID, 'Invalid or missing token', 401);
  }
}

/** Attach player ID from JWT to request for convenience */
export function playerId(request: FastifyRequest): string {
  const payload = request.user as { sub: string };
  return payload.sub;
}
