import type { FastifyInstance } from 'fastify';
import { z } from 'zod';
import { prisma } from '../plugins/db.js';
import { requireAuth, playerId } from '../middleware/auth.js';
import { validate } from '../middleware/validation.js';
import { AppError, ErrorCode } from '../../shared/errors.js';

const purchaseSchema = z.object({
  itemId: z.string(),
});

export async function shopRoutes(app: FastifyInstance): Promise<void> {
  /** GET /api/shop/items */
  app.get('/items', async () => {
    const items = await prisma.shopItem.findMany({ where: { isActive: true } });
    return { items };
  });

  /** POST /api/shop/purchase */
  app.post('/purchase', { preHandler: requireAuth }, async (req) => {
    const id   = playerId(req);
    const body = validate(purchaseSchema, req.body);

    const item = await prisma.shopItem.findUnique({ where: { id: body.itemId } });
    if (!item || !item.isActive) {
      throw new AppError(ErrorCode.SHOP_ITEM_NOT_FOUND, 'Item not found', 404);
    }

    const alreadyOwned = await prisma.unlock.findUnique({
      where: { playerId_itemType_itemId: { playerId: id, itemType: item.itemType, itemId: item.id } },
    });
    if (alreadyOwned) {
      throw new AppError(ErrorCode.SHOP_ALREADY_OWNED, 'Item already owned');
    }

    /* Atomic deduct coins + create unlock */
    const [player, unlock] = await prisma.$transaction(async (tx) => {
      const p = await tx.player.findUniqueOrThrow({ where: { id } });
      if (p.coins < item.coinCost) {
        throw new AppError(ErrorCode.SHOP_INSUFFICIENT_COINS, 'Not enough coins');
      }
      const updated = await tx.player.update({
        where: { id },
        data: { coins: { decrement: item.coinCost } },
      });
      const u = await tx.unlock.create({
        data: { playerId: id, itemType: item.itemType, itemId: item.id },
      });
      return [updated, u];
    });

    return { success: true, coinsRemaining: player.coins, unlock };
  });

  /** GET /api/shop/inventory */
  app.get('/inventory', { preHandler: requireAuth }, async (req) => {
    const id = playerId(req);
    const unlocks = await prisma.unlock.findMany({ where: { playerId: id } });
    return { unlocks };
  });
}
