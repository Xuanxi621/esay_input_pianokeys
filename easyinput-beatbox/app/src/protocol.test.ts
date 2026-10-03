import { describe, expect, it } from "vitest";
import {
  createInitialState,
  reduceHostLine,
  reduceHostMessage,
  applyLocalBpmDraft,
  applyLocalPattern,
} from "./device-store";
import { createEmptyBanks, overdubStep } from "./pattern";
import {
  exportPatternFile,
  parsePatternFile,
  patternFileToJson,
} from "./pattern-io";
import {
  clampBpm,
  decodePatternHex,
  encodePatternHex,
  emptyTracks,
  parseHostLine,
} from "./protocol";
import {
  clocksForQuarter,
  clocksForSixteenth,
  decodeMidiMessage,
  encodeDomainEvent,
  midiClocksToEmit,
  sppFromStep,
  stepFromSpp,
  ticksPerMidiClock,
} from "./midi-adapter";

describe("protocol helpers", () => {
  it("clamps bpm to product range", () => {
    expect(clampBpm(40)).toBe(60);
    expect(clampBpm(300)).toBe(240);
    expect(clampBpm(128.4)).toBe(128);
  });

  it("parses valid lines and ignores junk", () => {
    expect(parseHostLine('{"t":"start"}')).toEqual({ t: "start" });
    expect(parseHostLine("not-json")).toBeNull();
    expect(parseHostLine("{bad")).toBeNull();
  });

  it("round-trips pattern hex", () => {
    const tracks = emptyTracks();
    tracks[0][0] = 120;
    tracks[1][4] = 100;
    const hex = encodePatternHex(tracks);
    expect(hex).toHaveLength(192);
    expect(decodePatternHex(hex)).toEqual(tracks);
  });
});

describe("device store", () => {
  it("stays connecting until state arrives", () => {
    let s = createInitialState();
    s = reduceHostLine(s, '{"t":"hello","v":2,"name":"EasyInput Beatbox"}');
    expect(s.sync).toBe("connecting");
    s = reduceHostLine(s, '{"t":"state","bpm":120,"run":0,"beat":0,"rev":3}');
    expect(s.sync).toBe("synced");
    expect(s.revision).toBe(3);
  });

  it("keeps bpm draft while dragging against stale state", () => {
    let s = createInitialState();
    s = applyLocalBpmDraft(s, 140);
    s = reduceHostMessage(s, { t: "state", bpm: 120, run: 0, beat: 0 });
    expect(s.bpm).toBe(140);
    expect(s.bpmDraft).toBe(140);
  });

  it("applies per-bank pattern without conflict UI", () => {
    let s = createInitialState();
    const tracks = emptyTracks();
    tracks[0][0] = 120;
    const hex = encodePatternHex(tracks);
    s = reduceHostMessage(s, { t: "pattern", bank: 0, rev: 4, p: hex });
    expect(s.pattern.a[0][0]).toBe(120);
    expect(s.revision).toBe(4);
    expect(s.sync).toBe("synced");
  });

  it("keeps local draft when dirty and device pattern arrives", () => {
    let s = createInitialState();
    const local = createEmptyBanks(1);
    local.a[0][0] = 127;
    s = applyLocalPattern(s, local);
    const hex = encodePatternHex(emptyTracks());
    s = reduceHostMessage(s, { t: "pattern", bank: 0, rev: 9, p: hex });
    expect(s.pattern.a[0][0]).toBe(127);
    expect(s.revision).toBe(9);
  });
});

describe("midi adapter", () => {
  it("maps transport and clock semantics", () => {
    expect(encodeDomainEvent({ type: "start" })).toEqual([0xfa]);
    expect(encodeDomainEvent({ type: "continue" })).toEqual([0xfb]);
    expect(encodeDomainEvent({ type: "stop" })).toEqual([0xfc]);
    expect(encodeDomainEvent({ type: "clock" })).toEqual([0xf8]);
    expect(clocksForQuarter()).toBe(24);
    expect(clocksForSixteenth()).toBe(6);
    expect(ticksPerMidiClock()).toBe(4);
  });

  it("encodes and decodes SPP around 16th notes", () => {
    const midiBeat = sppFromStep(2, 5);
    expect(encodeDomainEvent({ type: "spp", midiBeat })).toEqual([0xf2, 37, 0]);
    expect(stepFromSpp(midiBeat)).toEqual({ bar: 2, step: 5 });
    expect(decodeMidiMessage([0xf2, 37, 0])).toEqual({ type: "spp", midiBeat: 37 });
  });

  it("emits 24 clocks per quarter from internal ticks", () => {
    expect(midiClocksToEmit(0, 96)).toBe(24);
    expect(midiClocksToEmit(0, 24)).toBe(6);
    expect(midiClocksToEmit(4, 8)).toBe(1);
  });

  it("maps channel 10 drum notes", () => {
    expect(encodeDomainEvent({ type: "noteOn", note: 36, velocity: 100 })).toEqual([
      0x99, 36, 100,
    ]);
    expect(decodeMidiMessage([0x99, 38, 120])).toEqual({
      type: "noteOn",
      note: 38,
      velocity: 120,
    });
  });
});

describe("overdub and pattern io", () => {
  it("overdubs without clearing other steps", () => {
    const tracks = emptyTracks();
    tracks[0][0] = 120;
    const next = overdubStep(tracks, 1, 4, 110);
    expect(next[0][0]).toBe(120);
    expect(next[1][4]).toBe(110);
    expect(tracks[1][4]).toBe(0);
  });

  it("round-trips pattern file json", () => {
    const banks = createEmptyBanks(3);
    banks.a[0][0] = 100;
    banks.b[1][4] = 90;
    banks.fill[2][8] = 80;
    const file = exportPatternFile(banks, { bpm: 128, swing: 58 });
    const parsed = parsePatternFile(patternFileToJson(file));
    expect(parsed.ok).toBe(true);
    if (!parsed.ok) return;
    expect(parsed.banks.a[0][0]).toBe(100);
    expect(parsed.banks.b[1][4]).toBe(90);
    expect(parsed.banks.fill[2][8]).toBe(80);
    expect(parsed.file.bpm).toBe(128);
    expect(parsed.file.swing).toBe(58);
  });
});
