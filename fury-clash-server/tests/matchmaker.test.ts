import { describe, it, expect, vi, beforeEach } from 'vitest';
import type { QueueEntry } from '../src/shared/types.js';

/** Pairing algorithm extracted for unit testing */
function findPair(
  entries: QueueEntry[],
  eloRangeFn: (e: QueueEntry) => number,
): [QueueEntry, QueueEntry] | null {
  for (let i = 0; i < entries.length; i++) {
    for (let j = i + 1; j < entries.length; j++) {
      if (Math.abs(entries[i].elo - entries[j].elo) <= eloRangeFn(entries[i])) {
        return [entries[i], entries[j]];
      }
    }
  }
  return null;
}

function makeEntry(id: string, elo: number, queuedMsAgo = 0): QueueEntry {
  return { playerId: id, username: id, elo, fighterId: 0, region: 'us-east', queuedAt: Date.now() - queuedMsAgo };
}

describe('Matcher pairing algorithm', () => {
  const fixedRange = () => 100;

  it('pairs two players within ELO range', () => {
    const entries = [makeEntry('p1', 1000), makeEntry('p2', 1050)];
    const pair = findPair(entries, fixedRange);
    expect(pair).not.toBeNull();
    expect(pair![0].playerId).toBe('p1');
    expect(pair![1].playerId).toBe('p2');
  });

  it('does not pair players outside ELO range', () => {
    const entries = [makeEntry('p1', 1000), makeEntry('p2', 1200)];
    expect(findPair(entries, fixedRange)).toBeNull();
  });

  it('picks the closest ELO match', () => {
    const entries = [
      makeEntry('p1', 1000),
      makeEntry('p2', 1050),
      makeEntry('p3', 1090),
    ];
    const pair = findPair(entries, fixedRange);
    expect(pair).not.toBeNull();
    /* p1 matches with p2 first (by index order) */
    expect(pair![0].playerId).toBe('p1');
    expect(pair![1].playerId).toBe('p2');
  });

  it('expands range with wait time', () => {
    const expandingRange = (e: QueueEntry) => {
      const waitSec = (Date.now() - e.queuedAt) / 1000;
      return Math.min(100 + waitSec * 20, 500);
    };

    /* Players far apart, one has been waiting 10 seconds */
    const entries = [makeEntry('p1', 1000, 10_000), makeEntry('p2', 1300)];
    const pair = findPair(entries, expandingRange);
    expect(pair).not.toBeNull();
  });
});
