#!/usr/bin/env python3
"""Verify the firmware's Q32 96-PPQN sample-clock scheduler."""

SAMPLE_RATE = 32_000
Q32 = 1 << 32
PPQN = 96
TICKS_PER_16TH = 24
MIN_BPM = 60
MAX_BPM = 240
TEST_SECONDS = 30 * 60
SWINGS = (50, 66, 75)


def sample_at_tick(tick: int, interval_q32: int) -> int:
    return (tick * interval_q32 + Q32 - 1) // Q32


def swing_delay_ticks(swing: int) -> int:
    swing = max(50, min(75, swing))
    return ((swing - 50) * 12) // 25


def check_bpm(bpm: int) -> tuple[int, float]:
    interval_q32 = (SAMPLE_RATE * 60 * Q32) // (bpm * PPQN)
    tick_count = TEST_SECONDS * bpm * PPQN // 60
    end_sample = sample_at_tick(tick_count, interval_q32)
    ideal_end = round(tick_count * SAMPLE_RATE * 60 / (bpm * PPQN))
    drift = end_sample - ideal_end

    previous = sample_at_tick(0, interval_q32)
    minimum = 1 << 30
    maximum = 0
    for tick in range(1, tick_count + 1):
        current = sample_at_tick(tick, interval_q32)
        span = current - previous
        minimum = min(minimum, span)
        maximum = max(maximum, span)
        previous = current

    if maximum - minimum > 1:
        raise AssertionError(f"{bpm} BPM interval jitter exceeds one sample")
    if abs(drift) > 1:
        raise AssertionError(f"{bpm} BPM 30-minute drift is {drift} samples")
    return drift, maximum - minimum


def check_swing(bpm: int, swing: int) -> None:
    delay = swing_delay_ticks(swing)
    interval_q32 = (SAMPLE_RATE * 60 * Q32) // (bpm * PPQN)
    # Odd 16ths should fire delay ticks later than the straight grid.
    for step in range(16):
        target = step * TICKS_PER_16TH + (delay if step & 1 else 0)
        sample = sample_at_tick(target, interval_q32)
        straight = sample_at_tick(step * TICKS_PER_16TH, interval_q32)
        if step & 1:
            if sample < straight:
                raise AssertionError(f"swing {swing}% delayed odd step earlier than grid")
            expected_delay_samples = sample_at_tick(delay, interval_q32) - sample_at_tick(0, interval_q32)
            actual = sample - straight
            if abs(actual - expected_delay_samples) > 1:
                raise AssertionError(
                    f"swing {swing}% step {step} delay {actual} != {expected_delay_samples}"
                )
        else:
            if sample != straight:
                raise AssertionError(f"swing {swing}% even step moved")


def main() -> None:
    worst_drift = 0
    worst_interval_spread = 0
    for bpm in range(MIN_BPM, MAX_BPM + 1):
        drift, spread = check_bpm(bpm)
        worst_drift = max(worst_drift, abs(drift))
        worst_interval_spread = max(worst_interval_spread, spread)
        for swing in SWINGS:
            check_swing(bpm, swing)

    print(
        "PASS:"
        f" {MIN_BPM}-{MAX_BPM} BPM @ {PPQN} PPQN,"
        f" swing {list(SWINGS)},"
        f" {TEST_SECONDS // 60} min each,"
        f" drift <= {worst_drift} sample,"
        f" interval spread <= {worst_interval_spread} sample"
    )


if __name__ == "__main__":
    main()
