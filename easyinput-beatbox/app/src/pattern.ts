import {
  TRACKS,
  STEPS,
  decodePatternHex,
  emptyTracks,
  encodePatternHex,
} from "./protocol";

export type PatternBanks = {
  a: number[][];
  b: number[][];
  fill: number[][];
  revision: number;
};

export type PatternDraft = PatternBanks & {
  dirty: boolean;
  conflict: boolean;
};

export function createEmptyBanks(revision = 1): PatternBanks {
  return {
    a: emptyTracks(),
    b: emptyTracks(),
    fill: emptyTracks(),
    revision,
  };
}

export function cloneTracks(tracks: number[][]): number[][] {
  return tracks.map((row) => row.slice());
}

export function cloneBanks(banks: PatternBanks): PatternBanks {
  return {
    a: cloneTracks(banks.a),
    b: cloneTracks(banks.b),
    fill: cloneTracks(banks.fill),
    revision: banks.revision,
  };
}

export function banksFromDump(msg: {
  rev: number;
  a: string;
  b: string;
  f: string;
}): PatternBanks | null {
  const a = decodePatternHex(msg.a);
  const b = decodePatternHex(msg.b);
  const fill = decodePatternHex(msg.f);
  if (!a || !b || !fill) return null;
  return { a, b, fill, revision: msg.rev };
}

export function bankHex(banks: PatternBanks, bank: 0 | 1 | 2): string {
  const tracks = bank === 0 ? banks.a : bank === 1 ? banks.b : banks.fill;
  return encodePatternHex(tracks);
}

export function toggleStep(tracks: number[][], track: number, step: number, velocity = 110): number[][] {
  const next = cloneTracks(tracks);
  if (track < 0 || track >= TRACKS || step < 0 || step >= STEPS) return next;
  next[track][step] = next[track][step] > 0 ? 0 : velocity;
  return next;
}

/** Overdub: write a hit without clearing other steps/tracks. */
export function overdubStep(
  tracks: number[][],
  track: number,
  step: number,
  velocity = 110,
): number[][] {
  const next = cloneTracks(tracks);
  if (track < 0 || track >= TRACKS || step < 0 || step >= STEPS) return next;
  const v = Math.max(1, Math.min(127, Math.round(velocity)));
  next[track][step] = v;
  return next;
}

export function setStepVelocity(
  tracks: number[][],
  track: number,
  step: number,
  velocity: number,
): number[][] {
  const next = cloneTracks(tracks);
  if (track < 0 || track >= TRACKS || step < 0 || step >= STEPS) return next;
  next[track][step] = Math.max(0, Math.min(127, Math.round(velocity)));
  return next;
}

export function clearTrack(tracks: number[][], track: number): number[][] {
  const next = cloneTracks(tracks);
  if (track < 0 || track >= TRACKS) return next;
  next[track] = Array(STEPS).fill(0);
  return next;
}

export function clearPattern(tracks: number[][]): number[][] {
  return emptyTracks();
}

export function copyPattern(from: number[][]): number[][] {
  return cloneTracks(from);
}
