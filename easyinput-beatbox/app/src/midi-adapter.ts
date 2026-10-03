/**
 * Transport-neutral MIDI adapter.
 * Domain events <-> standard MIDI bytes (Clock 24 PPQN, Ch.10 drums).
 */

import {
  NOTE_CHH,
  NOTE_CLAP,
  NOTE_CLICK_ACCENT,
  NOTE_CLICK_NORMAL,
  NOTE_KICK,
  NOTE_OHH,
  NOTE_SNARE,
  PPQN_INTERNAL,
  PPQN_MIDI,
  TICKS_PER_16TH,
} from "./protocol";

export const MIDI_CLOCK = 0xf8;
export const MIDI_START = 0xfa;
export const MIDI_CONTINUE = 0xfb;
export const MIDI_STOP = 0xfc;
export const MIDI_SPP = 0xf2;
export const MIDI_CH10_NOTE_ON = 0x99;
export const MIDI_CH10_NOTE_OFF = 0x89;

export type DomainEvent =
  | { type: "start" }
  | { type: "continue" }
  | { type: "stop" }
  | { type: "clock" }
  | { type: "spp"; midiBeat: number }
  | { type: "noteOn"; note: number; velocity: number }
  | { type: "noteOff"; note: number; velocity?: number };

/** 1 internal tick = 1/96 quarter; MIDI clock every 4 internal ticks. */
export function midiClocksFromInternalTicks(ticks: number): number {
  return Math.floor(ticks / (PPQN_INTERNAL / PPQN_MIDI));
}

export function internalTicksFromMidiClocks(clocks: number): number {
  return clocks * (PPQN_INTERNAL / PPQN_MIDI);
}

/** Song Position Pointer unit: 1 MIDI beat = one 16th note = 6 MIDI clocks. */
export function sppFromStep(bar: number, step: number): number {
  return bar * 16 + (step & 0x0f);
}

export function stepFromSpp(midiBeat: number): { bar: number; step: number } {
  const beat = Math.max(0, midiBeat | 0);
  return { bar: Math.floor(beat / 16), step: beat % 16 };
}

export function ticksPerMidiClock(): number {
  return PPQN_INTERNAL / PPQN_MIDI;
}

export function ticksPerSixteenth(): number {
  return TICKS_PER_16TH;
}

export function encodeDomainEvent(event: DomainEvent): number[] {
  switch (event.type) {
    case "start":
      return [MIDI_START];
    case "continue":
      return [MIDI_CONTINUE];
    case "stop":
      return [MIDI_STOP];
    case "clock":
      return [MIDI_CLOCK];
    case "spp": {
      const value = Math.max(0, Math.min(0x3fff, event.midiBeat | 0));
      return [MIDI_SPP, value & 0x7f, (value >> 7) & 0x7f];
    }
    case "noteOn":
      return [MIDI_CH10_NOTE_ON, event.note & 0x7f, Math.max(1, event.velocity & 0x7f)];
    case "noteOff":
      return [MIDI_CH10_NOTE_OFF, event.note & 0x7f, (event.velocity ?? 0) & 0x7f];
    default:
      return [];
  }
}

export function decodeMidiMessage(bytes: number[]): DomainEvent | null {
  if (!bytes.length) return null;
  const status = bytes[0];
  if (status === MIDI_CLOCK) return { type: "clock" };
  if (status === MIDI_START) return { type: "start" };
  if (status === MIDI_CONTINUE) return { type: "continue" };
  if (status === MIDI_STOP) return { type: "stop" };
  if (status === MIDI_SPP && bytes.length >= 3) {
    const midiBeat = (bytes[1] & 0x7f) | ((bytes[2] & 0x7f) << 7);
    return { type: "spp", midiBeat };
  }
  if ((status & 0xf0) === 0x90 && bytes.length >= 3) {
    const velocity = bytes[2] & 0x7f;
    if (velocity === 0) return { type: "noteOff", note: bytes[1] & 0x7f, velocity: 0 };
    return { type: "noteOn", note: bytes[1] & 0x7f, velocity };
  }
  if ((status & 0xf0) === 0x80 && bytes.length >= 3) {
    return { type: "noteOff", note: bytes[1] & 0x7f, velocity: bytes[2] & 0x7f };
  }
  return null;
}

/** Emit 24 MIDI clocks for one quarter note from internal ticks. */
export function clocksForQuarter(): number {
  return PPQN_MIDI;
}

/** Emit 6 MIDI clocks for one sixteenth. */
export function clocksForSixteenth(): number {
  return PPQN_MIDI / 4;
}

export const DRUM_NOTES = {
  kick: NOTE_KICK,
  snare: NOTE_SNARE,
  chh: NOTE_CHH,
  ohh: NOTE_OHH,
  clap: NOTE_CLAP,
  clickAccent: NOTE_CLICK_ACCENT,
  clickNormal: NOTE_CLICK_NORMAL,
} as const;

/**
 * Given an increasing internal tick counter while running, return how many
 * MIDI clocks should be emitted since the previous tick count.
 */
export function midiClocksToEmit(prevTick: number, nextTick: number): number {
  if (nextTick <= prevTick) return 0;
  return midiClocksFromInternalTicks(nextTick) - midiClocksFromInternalTicks(prevTick);
}
