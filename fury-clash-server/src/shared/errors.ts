export const ErrorCode = {
  // Auth
  AUTH_INVALID_CREDENTIALS : 'AUTH_001',
  AUTH_USERNAME_TAKEN      : 'AUTH_002',
  AUTH_EMAIL_TAKEN         : 'AUTH_003',
  AUTH_TOKEN_INVALID       : 'AUTH_004',
  AUTH_TOKEN_EXPIRED       : 'AUTH_005',

  // Matchmaking
  MATCH_ALREADY_QUEUED     : 'MATCH_001',
  MATCH_NOT_IN_QUEUE       : 'MATCH_002',
  MATCH_ROOM_NOT_FOUND     : 'MATCH_003',
  MATCH_ROOM_FULL          : 'MATCH_004',
  MATCH_BANNED             : 'MATCH_005',

  // Relay
  RELAY_ROOM_NOT_FOUND     : 'RELAY_001',
  RELAY_ROOM_FULL          : 'RELAY_002',
  RELAY_AUTH_FAILED        : 'RELAY_003',
  RELAY_BAD_SLOT           : 'RELAY_004',
  RELAY_INVALID_INPUT      : 'RELAY_005',

  // Shop
  SHOP_ITEM_NOT_FOUND      : 'SHOP_001',
  SHOP_INSUFFICIENT_COINS  : 'SHOP_002',
  SHOP_ALREADY_OWNED       : 'SHOP_003',

  // General
  NOT_FOUND                : 'GEN_001',
  FORBIDDEN                : 'GEN_002',
  VALIDATION               : 'GEN_003',
  INTERNAL                 : 'GEN_004',
} as const;

export type ErrorCode = typeof ErrorCode[keyof typeof ErrorCode];

export class AppError extends Error {
  constructor(
    public readonly code: ErrorCode,
    message: string,
    public readonly statusCode: number = 400,
  ) {
    super(message);
    this.name = 'AppError';
  }
}

export function isAppError(e: unknown): e is AppError {
  return e instanceof AppError;
}
