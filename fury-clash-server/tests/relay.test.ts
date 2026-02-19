import { describe, it, expect } from 'vitest';
import { InputLog } from '../src/relay-server/input-log.js';

describe('InputLog', () => {
  it('records P1 inputs', () => {
    const log = new InputLog();
    log.recordP1(1, 0b0001);
    log.recordP1(2, 0b0010);
    const arr = log.toArray();
    expect(arr).toHaveLength(2);
    expect(arr[0]).toEqual({ frame: 1, p1: 0b0001, p2: 0 });
    expect(arr[1]).toEqual({ frame: 2, p1: 0b0010, p2: 0 });
  });

  it('records P2 inputs', () => {
    const log = new InputLog();
    log.recordP2(1, 0xFF);
    const arr = log.toArray();
    expect(arr[0].p2).toBe(0xFF);
  });

  it('merges P1 and P2 on same frame', () => {
    const log = new InputLog();
    log.recordP1(5, 0b0001);
    log.recordP2(5, 0b0010);
    const arr = log.toArray();
    expect(arr).toHaveLength(1);
    expect(arr[0]).toEqual({ frame: 5, p1: 0b0001, p2: 0b0010 });
  });

  it('uses last known input for missing side on new frame', () => {
    const log = new InputLog();
    log.recordP1(1, 0b1111);
    log.recordP2(2, 0b0001);
    const arr = log.toArray();
    // Frame 2 P1 should carry forward last known P1 input
    expect(arr[1].p1).toBe(0b1111);
    expect(arr[1].p2).toBe(0b0001);
  });

  it('frameCount returns number of entries', () => {
    const log = new InputLog();
    log.recordP1(1, 0);
    log.recordP1(2, 0);
    expect(log.frameCount()).toBe(2);
  });
});
