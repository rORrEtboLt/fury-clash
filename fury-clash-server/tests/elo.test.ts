import { describe, it, expect } from 'vitest';
import { calculateElo } from '../src/matchmaker/elo.js';

describe('ELO calculation', () => {
  it('gives rating gain to winner and loss to loser', () => {
    const result = calculateElo(1000, 1000, 50, 50, '0');
    expect(result.p1Delta).toBeGreaterThan(0);
    expect(result.p2Delta).toBeLessThan(0);
    expect(result.p1NewElo).toBeGreaterThan(1000);
    expect(result.p2NewElo).toBeLessThan(1000);
  });

  it('zero sum: gains equal losses for equal K-factor', () => {
    const result = calculateElo(1000, 1000, 50, 50, '0');
    expect(result.p1Delta + result.p2Delta).toBe(0);
  });

  it('higher K for new players (< 30 games)', () => {
    const veteran = calculateElo(1000, 1000, 50, 50, '0');
    const newbie  = calculateElo(1000, 1000, 5,  5,  '0');
    expect(Math.abs(newbie.p1Delta)).toBeGreaterThan(Math.abs(veteran.p1Delta));
  });

  it('upset win gives more ELO', () => {
    const upset   = calculateElo(800,  1200, 50, 50, '0');   // underdog wins
    const expected= calculateElo(1200, 800,  50, 50, '0');   // favourite wins
    expect(upset.p1Delta).toBeGreaterThan(expected.p1Delta);
  });

  it('draw gives fractional change near equal ELO', () => {
    const result = calculateElo(1000, 1000, 50, 50, 'draw');
    expect(result.p1Delta).toBe(0);
    expect(result.p2Delta).toBe(0);
  });

  it('ELO floor is 100', () => {
    const result = calculateElo(105, 2000, 50, 50, '1');
    expect(result.p1NewElo).toBeGreaterThanOrEqual(100);
  });
});
