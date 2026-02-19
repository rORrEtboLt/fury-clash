/**
 * Per-match input log — records every frame's inputs for replay and dispute.
 * Kept in memory during the match; flushed to the replay worker on end.
 */

export interface FrameEntry {
  frame : number;
  p1    : number;   // input bitfield
  p2    : number;
}

export class InputLog {
  private entries: FrameEntry[] = [];
  private p1Latest  = 0;
  private p2Latest  = 0;

  recordP1(frame: number, input: number): void {
    this.p1Latest = input;
    this.upsert(frame, input, -1);
  }

  recordP2(frame: number, input: number): void {
    this.p2Latest = input;
    this.upsert(frame, -1, input);
  }

  private upsert(frame: number, p1: number, p2: number): void {
    const last = this.entries[this.entries.length - 1];
    if (last?.frame === frame) {
      if (p1 >= 0) last.p1 = p1;
      if (p2 >= 0) last.p2 = p2;
    } else {
      this.entries.push({
        frame,
        p1: p1 >= 0 ? p1 : this.p1Latest,
        p2: p2 >= 0 ? p2 : this.p2Latest,
      });
    }
  }

  toArray(): FrameEntry[] {
    return this.entries;
  }

  frameCount(): number {
    return this.entries.length;
  }
}
