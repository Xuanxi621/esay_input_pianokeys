#!/usr/bin/env python3
"""Static sanity checks for embedded 32 kHz mono s16le drum one-shots."""

from pathlib import Path
from array import array
import math
import sys

ROOT = Path(__file__).resolve().parents[1]
SAMPLE_DIR = ROOT / "firmware" / "main" / "audio" / "samples"
SAMPLE_RATE = 32_000
EXPECTED = {
    "kick.raw",
    "snare.raw",
    "hihat_closed.raw",
    "hihat_open.raw",
    "clap.raw",
    "rim.raw",
}


def inspect(path: Path) -> tuple[float, int, float]:
    raw = path.read_bytes()
    if len(raw) < 2 or len(raw) % 2:
        raise AssertionError(f"{path.name}: invalid s16le byte count")

    samples = array("h")
    samples.frombytes(raw)
    if sys.byteorder != "little":
        samples.byteswap()

    peak = max(abs(value) for value in samples)
    rms = math.sqrt(sum(value * value for value in samples) / len(samples))
    duration_ms = len(samples) * 1000 / SAMPLE_RATE

    if peak < 4_000:
        raise AssertionError(f"{path.name}: peak too quiet ({peak})")
    if rms < 500:
        raise AssertionError(f"{path.name}: RMS too quiet ({rms:.0f})")
    if not 40 <= duration_ms <= 400:
        raise AssertionError(f"{path.name}: unexpected duration ({duration_ms:.1f} ms)")
    return duration_ms, peak, rms


def main() -> None:
    found = {path.name for path in SAMPLE_DIR.glob("*.raw")}
    if found != EXPECTED:
        raise AssertionError(f"sample set mismatch: expected {EXPECTED}, got {found}")

    for name in sorted(EXPECTED):
        duration, peak, rms = inspect(SAMPLE_DIR / name)
        print(f"{name:18} {duration:6.1f} ms  peak={peak:5d}  rms={rms:7.1f}")
    print("PASS: all six embedded drum samples are non-empty and audible-range")


if __name__ == "__main__":
    main()
