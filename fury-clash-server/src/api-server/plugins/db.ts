import { PrismaClient } from '@prisma/client';
import { childLogger } from '../../shared/logger.js';

const log = childLogger('db');

export const prisma = new PrismaClient({
  log: [
    { emit: 'event', level: 'query' },
    { emit: 'event', level: 'error' },
  ],
});

prisma.$on('error', (e) => log.error(e, 'Prisma error'));

export async function connectDb(): Promise<void> {
  await prisma.$connect();
  log.info('Database connected');
}

export async function disconnectDb(): Promise<void> {
  await prisma.$disconnect();
  log.info('Database disconnected');
}
