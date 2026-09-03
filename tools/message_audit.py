#!/usr/bin/env python3
"""Enumerate and validate Kalachakram's compositional message engine."""

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "messages.cpp"

VIBES = (
    "CURSED_HOURS",
    "TOO_EARLY",
    "MORNING",
    "LUNCH_LOADING",
    "AFTERNOON",
    "DAY_IS_DYING",
    "EVENING",
    "GO_TO_BED",
)
PHASES = ("EARLY", "MIDDLE", "LATE")
VIBE_WINDOWS = (
    (0, 300),
    (300, 180),
    (480, 180),
    (660, 120),
    (780, 180),
    (960, 120),
    (1080, 180),
    (1260, 180),
)
CONTEXT_BOUNDARIES_SECONDS = tuple(
    sorted(
        {
            (start + phase * (duration // 3)) * 60
            for start, duration in VIBE_WINDOWS
            for phase in range(3)
        }
    )
)


def extract_strings(source: str, array_name: str) -> list[str]:
    pattern = re.compile(
        rf"const char {array_name}\[\]\[17\] PROGMEM = \{{(.*?)\n\}};",
        re.DOTALL,
    )
    match = pattern.search(source)
    if not match:
        raise ValueError(f"Could not find {array_name}")
    return re.findall(r'^\s*"([^"]*)"\s*,?\s*$', match.group(1), re.MULTILINE)


def extract_pool_configs(source: str) -> list[tuple[int, int, int]]:
    block_match = re.search(
        r"const ContextPoolConfig contextPools\[8\]\[3\] PROGMEM = "
        r"\{(.*?)\n\};",
        source,
        re.DOTALL,
    )
    if not block_match:
        raise ValueError("Could not find contextPools")
    return [
        (int(offset), int(count), int(duration))
        for offset, count, duration in re.findall(
            r"\{\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\}",
            block_match.group(1),
        )
    ]


def multiplier_for(capacity: int) -> int:
    return {120: 43, 100: 37, 80: 27}[capacity]


def permuted_index(pool_index: int, counter: int, capacity: int) -> int:
    offset = (pool_index * 17 + 11) % capacity
    return (multiplier_for(capacity) * counter + offset) % capacity


def context_for_minute(minute: int) -> tuple[int, int, int]:
    for vibe_index, (start, duration) in enumerate(VIBE_WINDOWS):
        if start <= minute < start + duration:
            phase_duration = duration // 3
            elapsed = minute - start
            return vibe_index, elapsed // phase_duration, elapsed % phase_duration
    raise ValueError(f"Minute outside day: {minute}")


def next_context_boundary_delta(wall_second: int) -> int:
    for boundary in CONTEXT_BOUNDARIES_SECONDS:
        if boundary > wall_second:
            return boundary - wall_second
    return 86400 - wall_second


def simulate_unattended_day(
    start_minute: int,
    start_second: int,
    configs: list[tuple[int, int, int]],
    line1_fragments: list[str],
    line2_fragments: list[str],
) -> list[tuple[str, str]]:
    counters = [0] * 24
    initialized = [False] * 24
    outputs: list[tuple[str, str]] = []
    elapsed = 0
    start_wall_second = start_minute * 60 + start_second

    while elapsed < 86400:
        wall_second = (start_wall_second + elapsed) % 86400
        wall_minute = wall_second // 60
        vibe_index, phase_index, context_minute = context_for_minute(wall_minute)
        pool_index = vibe_index * 3 + phase_index
        offset, line1_count, _ = configs[pool_index]
        capacity = line1_count * len(line2_fragments)

        if not initialized[pool_index]:
            counters[pool_index] = context_minute % capacity
            initialized[pool_index] = True

        combination = permuted_index(
            pool_index,
            counters[pool_index],
            capacity,
        )
        counters[pool_index] = (counters[pool_index] + 1) % capacity
        outputs.append(
            (
                line1_fragments[offset + combination % line1_count],
                line2_fragments[combination // line1_count],
            )
        )

        timer_delta = 60
        boundary_delta = next_context_boundary_delta(wall_second)
        elapsed += min(timer_delta, boundary_delta)

    return outputs


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8")
    line1_fragments = extract_strings(source, "contextLine1Fragments")
    line2_fragments = extract_strings(source, "reactionLine2Fragments")
    configs = extract_pool_configs(source)
    errors: list[str] = []

    if len(configs) != 24:
        errors.append(f"Expected 24 pool configs, found {len(configs)}")

    all_outputs: list[tuple[str, str]] = []
    output_contexts: dict[tuple[str, str], list[str]] = defaultdict(list)
    pool_rows: list[tuple[str, str, int, int, int, int, int]] = []

    for pool_index, config in enumerate(configs):
        offset, line1_count, duration = config
        vibe = VIBES[pool_index // 3]
        phase = PHASES[pool_index % 3]
        capacity = line1_count * len(line2_fragments)
        pool_outputs: list[tuple[str, str]] = []

        if offset + line1_count > len(line1_fragments):
            errors.append(f"{vibe}/{phase}: fragment range exceeds array")
            continue

        for line1 in line1_fragments[offset : offset + line1_count]:
            for line2 in line2_fragments:
                pair = (line1, line2)
                pool_outputs.append(pair)
                all_outputs.append(pair)
                output_contexts[pair].append(f"{vibe}/{phase}")

        if len(set(pool_outputs)) != capacity:
            errors.append(f"{vibe}/{phase}: duplicate output inside pool")

        sequence = {
            permuted_index(pool_index, counter, capacity)
            for counter in range(capacity)
        }
        if len(sequence) != capacity:
            errors.append(f"{vibe}/{phase}: permutation is not full-cycle")

        pool_rows.append(
            (
                vibe,
                phase,
                duration,
                line1_count,
                len(line2_fragments),
                capacity,
                capacity - duration,
            )
        )

    all_fragments = line1_fragments + line2_fragments
    overlong = [fragment for fragment in all_fragments if len(fragment) > 16]
    empty = [fragment for fragment in all_fragments if not fragment]
    duplicate_line1 = [
        text for text, count in Counter(line1_fragments).items() if count > 1
    ]
    duplicate_outputs = [
        pair for pair, count in Counter(all_outputs).items() if count > 1
    ]
    cross_context_duplicates = [
        pair
        for pair, contexts in output_contexts.items()
        if len(set(contexts)) > 1
    ]

    if overlong:
        errors.append(f"Overlong fragments: {overlong}")
    if empty:
        errors.append(f"Empty fragments: {len(empty)}")
    if duplicate_line1:
        errors.append(f"Duplicate context line 1 fragments: {duplicate_line1}")
    if duplicate_outputs:
        errors.append(f"Duplicate complete outputs: {len(duplicate_outputs)}")
    if cross_context_duplicates:
        errors.append(
            f"Cross-context duplicate outputs: {len(cross_context_duplicates)}"
        )
    if any(capacity < duration for _, _, duration, _, _, capacity, _ in pool_rows):
        errors.append("One or more pools do not cover their phase duration")

    tested_starts = 0
    minimum_day_selections = 65535
    maximum_day_selections = 0
    duplicate_day_runs = 0
    for start_minute in range(1440):
        for start_second in (0, 1, 30, 59):
            automatic_day = simulate_unattended_day(
                start_minute,
                start_second,
                configs,
                line1_fragments,
                line2_fragments,
            )
            tested_starts += 1
            minimum_day_selections = min(minimum_day_selections, len(automatic_day))
            maximum_day_selections = max(maximum_day_selections, len(automatic_day))
            if len(automatic_day) != len(set(automatic_day)):
                duplicate_day_runs += 1

    if duplicate_day_runs:
        errors.append(
            f"{duplicate_day_runs} unattended 24-hour simulations repeated output"
        )

    print("VIBE | PHASE | MINUTES | LINE1 | LINE2 | CAPACITY | SPARE")
    for row in pool_rows:
        print(" | ".join(str(value) for value in row))

    fixed_phrase_storage = len(all_fragments) * 17
    metadata_storage = len(configs) * 4
    print()
    print(f"Line-1 fragments: {len(line1_fragments)}")
    print(f"Line-2 fragments: {len(line2_fragments)}")
    print(f"Total possible generated pairs: {len(all_outputs)}")
    print(f"Unique generated pairs: {len(set(all_outputs))}")
    print(f"Exact duplicate pairs: {len(duplicate_outputs)}")
    print(f"Cross-context duplicate pairs: {len(cross_context_duplicates)}")
    print(f"Overlong fragments: {len(overlong)}")
    print(f"Empty fragments: {len(empty)}")
    print(f"24-hour boot-time simulations: {tested_starts}")
    print(
        "Selections per simulated day: "
        f"{minimum_day_selections}..{maximum_day_selections}"
    )
    print(f"24-hour simulations with repeats: {duplicate_day_runs}")
    print(f"Fixed fragment storage estimate: {fixed_phrase_storage} bytes")
    print(f"Pool metadata estimate: {metadata_storage} bytes")
    print(f"Combined static data estimate: {fixed_phrase_storage + metadata_storage} bytes")

    if errors:
        print("\nAUDIT FAILED", file=sys.stderr)
        for error in errors:
            print(f"- {error}", file=sys.stderr)
        return 1

    print("\nAUDIT PASSED")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
