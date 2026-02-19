import 'dotenv/config';

function optional(key: string, fallback: string): string {
  return process.env[key] ?? fallback;
}

export const config = {
  env: optional('NODE_ENV', 'development'),
  isProduction: process.env.NODE_ENV === 'production',

  api: {
    port: parseInt(optional('API_PORT', '3000')),
    host: optional('API_HOST', '0.0.0.0'),
  },

  relay: {
    port: parseInt(optional('RELAY_PORT', '3002')),
    host: optional('RELAY_HOST', '0.0.0.0'),
    serverId: optional('RELAY_SERVER_ID', 'relay-1'),
    maxRooms: parseInt(optional('RELAY_MAX_ROOMS', '500')),
    disconnectGraceSec: parseInt(optional('RELAY_DISCONNECT_GRACE_SEC', '10')),
    pingIntervalMs: parseInt(optional('RELAY_PING_INTERVAL_MS', '5000')),
  },

  matchmaker: {
    port: parseInt(optional('MATCHMAKER_PORT', '3001')),
    scanIntervalMs: parseInt(optional('MATCHMAKER_SCAN_MS', '500')),
    eloInitialRange: parseInt(optional('MATCHMAKER_ELO_RANGE_INITIAL', '100')),
    eloRangeExpansionPerSec: parseInt(optional('MATCHMAKER_ELO_EXPANSION_PER_SEC', '20')),
    eloMaxRange: parseInt(optional('MATCHMAKER_ELO_MAX_RANGE', '500')),
    relayEndpoint: optional('RELAY_INTERNAL_ENDPOINT', 'ws://localhost:3002'),
  },

  db: {
    url: optional('DATABASE_URL', 'postgresql://fury:fury@localhost:5432/fury_clash'),
  },

  redis: {
    url: optional('REDIS_URL', 'redis://localhost:6379'),
  },

  jwt: {
    secret: optional('JWT_SECRET', 'dev-secret-change-in-production'),
    expiresIn: optional('JWT_EXPIRES_IN', '7d'),
    refreshExpiresIn: optional('JWT_REFRESH_EXPIRES_IN', '30d'),
  },

  bcrypt: {
    rounds: parseInt(optional('BCRYPT_ROUNDS', '10')),
  },

  session: {
    onlineTtlSec: parseInt(optional('SESSION_ONLINE_TTL_SEC', '30')),
  },
} as const;

/**
 * Plain connection options for BullMQ Queue/Worker constructors.
 * BullMQ bundles its own internal ioredis, so passing a Redis *instance*
 * from our ioredis causes a type conflict. Passing a plain options object avoids this.
 */
export function bullmqConnection(): { host: string; port: number; password?: string; db?: number } {
  const u = new URL(config.redis.url);
  return {
    host    : u.hostname,
    port    : parseInt(u.port || '6379'),
    password: u.password || undefined,
    db      : u.pathname.length > 1 ? parseInt(u.pathname.slice(1)) : undefined,
  };
}
