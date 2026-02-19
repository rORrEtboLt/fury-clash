import { ZodSchema, ZodError } from 'zod';
import { AppError, ErrorCode } from '../../shared/errors.js';

export function validate<T>(schema: ZodSchema<T>, data: unknown): T {
  try {
    return schema.parse(data);
  } catch (e) {
    if (e instanceof ZodError) {
      const msg = e.errors.map((x) => `${x.path.join('.')}: ${x.message}`).join('; ');
      throw new AppError(ErrorCode.VALIDATION, msg, 400);
    }
    throw e;
  }
}
