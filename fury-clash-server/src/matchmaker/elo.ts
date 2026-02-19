import {
  ELO_K_NEW_PLAYER,
  ELO_K_ESTABLISHED,
  ELO_NEW_PLAYER_THRESHOLD,
} from '../shared/constants.js';
import type { EloChange } from '../shared/types.js';

/**
 * Standard ELO calculation.
 * K-factor: 32 for players with < 30 games, 16 for established players.
 *
 * Expected score: E = 1 / (1 + 10^((Rb - Ra) / 400))
 * New rating: R' = R + K * (S - E)
 * S = 1.0 for win, 0.5 for draw, 0.0 for loss
 */
export function calculateElo(
  p1Elo       : number,
  p2Elo       : number,
  p1TotalGames: number,
  p2TotalGames: number,
  winnerId    : '0' | '1' | 'draw',
): EloChange {
  const k1 = p1TotalGames < ELO_NEW_PLAYER_THRESHOLD ? ELO_K_NEW_PLAYER : ELO_K_ESTABLISHED;
  const k2 = p2TotalGames < ELO_NEW_PLAYER_THRESHOLD ? ELO_K_NEW_PLAYER : ELO_K_ESTABLISHED;

  const e1 = expectedScore(p1Elo, p2Elo);
  const e2 = 1 - e1;

  let s1: number, s2: number;
  if (winnerId === '0')       { s1 = 1.0; s2 = 0.0; }
  else if (winnerId === '1')  { s1 = 0.0; s2 = 1.0; }
  else                        { s1 = 0.5; s2 = 0.5; }

  const p1Delta = Math.round(k1 * (s1 - e1));
  const p2Delta = Math.round(k2 * (s2 - e2));

  return {
    p1Delta,
    p2Delta,
    p1NewElo: Math.max(100, p1Elo + p1Delta),
    p2NewElo: Math.max(100, p2Elo + p2Delta),
  };
}

function expectedScore(ra: number, rb: number): number {
  return 1 / (1 + Math.pow(10, (rb - ra) / 400));
}
