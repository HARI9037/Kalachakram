#!/usr/bin/env python3
"""Audit Kalachakram's trigger-aware AUTO and TOUCH message engines."""

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
TOUCH_SETUP_COUNT = 3
TOUCH_REACTIONS_PER_VIBE = 4
TOUCH_CAPACITY = 12


def extract_strings(source: str, array_name: str) -> list[str]:
    pattern = re.compile(
        rf"const char {array_name}\[\]\[17\] PROGMEM = \{{(.*?)\n\}};",
        re.DOTALL,
    )
    match = pattern.search(source)
    if not match:
        raise ValueError(f"Could not find {array_name}")
    return re.findall(r'"([^"]*)"', match.group(1))


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


def auto_multiplier(capacity: int) -> int:
    return {120: 43, 100: 37, 80: 27}[capacity]


def auto_permuted(pool_index: int, counter: int, capacity: int) -> int:
    offset = (pool_index * 17 + 11) % capacity
    return (auto_multiplier(capacity) * counter + offset) % capacity


def touch_permuted(pool_index: int, counter: int) -> int:
    offset = (pool_index * 7 + 3) % TOUCH_CAPACITY
    return (5 * counter + offset) % TOUCH_CAPACITY


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


def pair_from_auto_index(
    pool_index: int,
    combination: int,
    configs: list[tuple[int, int, int]],
    setups: list[str],
    reactions: list[str],
) -> tuple[str, str]:
    offset, setup_count, _ = configs[pool_index]
    return (
        setups[offset + combination % setup_count],
        reactions[combination // setup_count],
    )


def pair_from_touch_index(
    pool_index: int,
    combination: int,
    setups: list[str],
    reactions: list[str],
) -> tuple[str, str]:
    offset = pool_index * TOUCH_SETUP_COUNT
    reaction_offset = (pool_index // 3) * TOUCH_REACTIONS_PER_VIBE
    return (
        setups[offset + combination % TOUCH_SETUP_COUNT],
        reactions[reaction_offset + combination // TOUCH_SETUP_COUNT],
    )


def simulate_automatic_day(
    start_minute: int,
    start_second: int,
    configs: list[tuple[int, int, int]],
    setups: list[str],
    reactions: list[str],
    inject_touches: bool,
) -> list[tuple[str, str]]:
    auto_counters = [0] * 24
    auto_initialized = [False] * 24
    touch_counters = [0] * 24
    outputs: list[tuple[str, str]] = []
    elapsed = 0
    start_wall_second = start_minute * 60 + start_second

    while elapsed < 86400:
        wall_second = (start_wall_second + elapsed) % 86400
        wall_minute = wall_second // 60
        vibe_index, phase_index, context_minute = context_for_minute(wall_minute)
        pool_index = vibe_index * 3 + phase_index
        _, setup_count, _ = configs[pool_index]
        capacity = setup_count * len(reactions)

        if not auto_initialized[pool_index]:
            auto_counters[pool_index] = context_minute % capacity
            auto_initialized[pool_index] = True

        combination = auto_permuted(
            pool_index,
            auto_counters[pool_index],
            capacity,
        )
        auto_counters[pool_index] = (auto_counters[pool_index] + 1) % capacity
        outputs.append(
            pair_from_auto_index(
                pool_index,
                combination,
                configs,
                setups,
                reactions,
            )
        )

        # Exercise independent TOUCH progression during the same simulated day.
        # It must never consume or rewrite an automatic counter.
        if inject_touches and len(outputs) % 17 == 0:
            touch_counters[pool_index] = (
                touch_counters[pool_index] + 2
            ) % TOUCH_CAPACITY

        elapsed += min(60, next_context_boundary_delta(wall_second))

    return outputs


def outputs_by_pool(
    configs: list[tuple[int, int, int]],
    auto_setups: list[str],
    auto_reactions: list[str],
    touch_setups: list[str],
    touch_reactions: list[str],
) -> tuple[list[list[tuple[str, str]]], list[list[tuple[str, str]]]]:
    auto_pools: list[list[tuple[str, str]]] = []
    touch_pools: list[list[tuple[str, str]]] = []
    for pool_index, (offset, setup_count, _) in enumerate(configs):
        auto_pools.append(
            [
                (line1, line2)
                for line1 in auto_setups[offset : offset + setup_count]
                for line2 in auto_reactions
            ]
        )
        touch_offset = pool_index * TOUCH_SETUP_COUNT
        touch_reaction_offset = (
            (pool_index // 3) * TOUCH_REACTIONS_PER_VIBE
        )
        touch_pools.append(
            [
                (line1, line2)
                for line1 in touch_setups[
                    touch_offset : touch_offset + TOUCH_SETUP_COUNT
                ]
                for line2 in touch_reactions[
                    touch_reaction_offset :
                    touch_reaction_offset + TOUCH_REACTIONS_PER_VIBE
                ]
            ]
        )
    return auto_pools, touch_pools


def print_samples(label: str, pools: list[list[tuple[str, str]]]) -> None:
    print(f"\n=== {label} REPRESENTATIVE SAMPLES ===")
    for vibe_index, vibe in enumerate(VIBES):
        candidates: list[tuple[str, str]] = []
        for phase in range(3):
            pool = pools[vibe_index * 3 + phase]
            variants_by_setup: dict[str, list[tuple[str, str]]] = {}
            for pair in pool:
                variants_by_setup.setdefault(pair[0], []).append(pair)
            distinct_setups = [
                variants[(phase * 2 + setup_index) % len(variants)]
                for setup_index, variants in enumerate(variants_by_setup.values())
            ]
            candidates.extend(
                distinct_setups[:2] if phase < 2 else distinct_setups[:1]
            )
        print(f"{vibe}:")
        for line1, line2 in candidates[:5]:
            print(f"  {line1} / {line2}")


def main() -> int:
    source = SOURCE.read_text(encoding="utf-8")
    auto_setups = extract_strings(source, "contextLine1Fragments")
    auto_reactions = extract_strings(source, "reactionLine2Fragments")
    touch_setups = extract_strings(source, "touchLine1Fragments")
    touch_reactions = extract_strings(source, "touchReactionLine2Fragments")
    configs = extract_pool_configs(source)
    errors: list[str] = []

    if len(configs) != 24:
        errors.append(f"Expected 24 pool configs, found {len(configs)}")
    if len(touch_setups) != 72:
        errors.append(f"Expected 72 touch setups, found {len(touch_setups)}")
    if len(touch_reactions) != 32:
        errors.append(f"Expected 32 touch reactions, found {len(touch_reactions)}")

    auto_pools, touch_pools = outputs_by_pool(
        configs,
        auto_setups,
        auto_reactions,
        touch_setups,
        touch_reactions,
    )
    auto_outputs = [pair for pool in auto_pools for pair in pool]
    touch_outputs = [pair for pool in touch_pools for pair in pool]

    auto_contexts: dict[tuple[str, str], list[str]] = defaultdict(list)
    touch_contexts: dict[tuple[str, str], list[str]] = defaultdict(list)
    pool_rows: list[tuple[str, str, int, int, int]] = []
    for pool_index, (_, setup_count, duration) in enumerate(configs):
        context = f"{VIBES[pool_index // 3]}/{PHASES[pool_index % 3]}"
        for pair in auto_pools[pool_index]:
            auto_contexts[pair].append(context)
        for pair in touch_pools[pool_index]:
            touch_contexts[pair].append(context)

        auto_capacity = setup_count * len(auto_reactions)
        touch_capacity = len(touch_pools[pool_index])
        pool_rows.append(
            (
                VIBES[pool_index // 3],
                PHASES[pool_index % 3],
                duration,
                auto_capacity,
                touch_capacity,
            )
        )
        if len(set(auto_pools[pool_index])) != auto_capacity:
            errors.append(f"{context}: duplicate AUTO output inside pool")
        if len(set(touch_pools[pool_index])) != touch_capacity:
            errors.append(f"{context}: duplicate TOUCH output inside pool")
        if auto_capacity < duration:
            errors.append(f"{context}: AUTO capacity below phase duration")
        if not 8 <= touch_capacity <= 20:
            errors.append(f"{context}: TOUCH capacity outside target 8..20")
        if len(
            {
                auto_permuted(pool_index, counter, auto_capacity)
                for counter in range(auto_capacity)
            }
        ) != auto_capacity:
            errors.append(f"{context}: AUTO permutation is not full-cycle")
        if len(
            {touch_permuted(pool_index, counter) for counter in range(touch_capacity)}
        ) != touch_capacity:
            errors.append(f"{context}: TOUCH permutation is not full-cycle")

    fragments = auto_setups + auto_reactions + touch_setups + touch_reactions
    overlong = [fragment for fragment in fragments if len(fragment) > 16]
    non_ascii = [fragment for fragment in fragments if not fragment.isascii()]
    empty = [fragment for fragment in fragments if not fragment]
    auto_duplicates = [pair for pair, count in Counter(auto_outputs).items() if count > 1]
    touch_duplicates = [pair for pair, count in Counter(touch_outputs).items() if count > 1]
    overlap = set(auto_outputs) & set(touch_outputs)
    auto_cross_context = [
        pair for pair, contexts in auto_contexts.items() if len(set(contexts)) > 1
    ]
    touch_cross_context = [
        pair for pair, contexts in touch_contexts.items() if len(set(contexts)) > 1
    ]

    flirty_auto_terms = (
        "MISS ME",
        "ENNE NOK",
        "CUTE",
        "ADORABLE",
        "FLIRT",
        "TOUCH",
    )
    flirty_auto = [
        fragment
        for fragment in auto_setups + auto_reactions
        if any(term in fragment for term in flirty_auto_terms)
    ]
    unsafe_touch_terms = ("SEXY", "BODY", "KISS", "HOT BODY", "MINE ONLY")
    unsafe_touch = [
        fragment
        for fragment in touch_setups + touch_reactions
        if any(term in fragment for term in unsafe_touch_terms)
    ]
    awkward_touch = [
        pair for pair in touch_outputs if "MISS" in pair[0] and "MISS" in pair[1]
    ]
    setup_manglish_markers = (
        "AAK", "AANO", "AAY", "ADUTHU", "ALLE", "BAAKI", "CHAYA",
        "CHINTHA", "CHODIK", "CHUMMA", "DHOORAM", "ENTHA", "ETHI",
        "DESKIL", "ENTHINA", "ETHARAYI", "EVDE", "EZHUNNETTU", "INNU", "INI",
        "IPPOZHUM", "IRUNNO", "IRUT", "IVDE", "JAYICHU", "KAATH",
        "KAIVITTU", "KAND", "KANN", "KASHTAM", "KAZHI", "KITT",
        "KAIVIDUNNU", "KULAM", "MADUTHU", "MANGUNNU", "MARANNO", "MATHI", "MAYAKKAM",
        "MOSHAM", "MUDIYO", "NADAK", "NALLA",
        "NALE", "NERATHE", "NIRTHI", "NOK", "ONNUM", "ORMA", "PANI", "PINNE",
        "POK", "POLE", "POLIYUNNU", "POY", "RAAVILE", "RAKSHAYILLA",
        "RATHRI", "READYANO",
        "THEER", "THANNE", "THUDANG", "THURANNU", "UCHAYA", "UCHAYIL", "UNDA", "UNDO", "UNDU",
        "URAKK", "URANG", "VALIYUNNU", "VAR", "VAYAR", "VAYYA", "VEND",
        "VENAM", "VENO", "VIDUNNILLA", "VIDUNNO", "VILIKK", "VISHAPPU", "VISHANNU",
        "VITT", "VANN", "VAIKUNNERAM",
    )
    english_heavy_auto_setups = [
        setup
        for setup in auto_setups
        if not any(marker in setup for marker in setup_manglish_markers)
    ]
    forbidden_phrases = (
        "DEFINITELY PM",
        "WORK DRIVE LEFT",
        "DAY RUNNING OUT",
        "PILLOW VITTALLO",
        "SIX AAKUNNO",
    )
    forbidden_phrase_hits = [
        fragment
        for fragment in fragments
        if any(phrase in fragment for phrase in forbidden_phrases)
    ]
    vee_fragments = [
        fragment for fragment in fragments if re.search(r"\bVEE\b", fragment)
    ]
    exact_time_pattern = re.compile(
        r"(?:\b\d{1,2}\s*(?:AM|PM)\b|\b\d{1,2}:\d{2}\b|\b(?:AM|PM)\b)",
        re.IGNORECASE,
    )
    exact_time_leaks = [
        fragment for fragment in fragments if exact_time_pattern.search(fragment)
    ]
    unique_touch_reactions = len(set(touch_reactions))

    if overlong:
        errors.append(f"Overlong fragments: {overlong}")
    if non_ascii:
        errors.append(f"Non-ASCII fragments: {non_ascii}")
    if empty:
        errors.append(f"Empty fragments: {len(empty)}")
    if auto_duplicates:
        errors.append(f"Duplicate AUTO outputs: {len(auto_duplicates)}")
    if touch_duplicates:
        errors.append(f"Duplicate TOUCH outputs: {len(touch_duplicates)}")
    if auto_cross_context:
        errors.append(f"Cross-context AUTO outputs: {len(auto_cross_context)}")
    if touch_cross_context:
        errors.append(f"Cross-context TOUCH outputs: {len(touch_cross_context)}")
    if overlap:
        errors.append(f"AUTO/TOUCH exact overlaps: {len(overlap)}")
    if flirty_auto:
        errors.append(f"Flirty/touch terms in AUTO corpus: {flirty_auto}")
    if unsafe_touch:
        errors.append(f"Unsafe TOUCH terms: {unsafe_touch}")
    if awkward_touch:
        errors.append(f"Awkward repeated-MISS TOUCH pairs: {awkward_touch}")
    if english_heavy_auto_setups:
        errors.append(
            f"English-heavy AUTO setups: {english_heavy_auto_setups}"
        )
    if forbidden_phrase_hits:
        errors.append(f"Forbidden awkward phrases: {forbidden_phrase_hits}")
    if vee_fragments:
        errors.append(f"Unnatural VEE fragments: {vee_fragments}")
    if exact_time_leaks:
        errors.append(f"Exact-time leaks: {exact_time_leaks}")
    if unique_touch_reactions < 24:
        errors.append(
            f"Insufficient TOUCH reaction variety: {unique_touch_reactions} unique"
        )

    tested_starts = 0
    minimum_day_selections = 65535
    maximum_day_selections = 0
    duplicate_day_runs = 0
    touch_changed_auto_runs = 0
    for start_minute in range(1440):
        for start_second in (0, 1, 30, 59):
            baseline = simulate_automatic_day(
                start_minute,
                start_second,
                configs,
                auto_setups,
                auto_reactions,
                inject_touches=False,
            )
            with_touches = simulate_automatic_day(
                start_minute,
                start_second,
                configs,
                auto_setups,
                auto_reactions,
                inject_touches=True,
            )
            tested_starts += 1
            minimum_day_selections = min(minimum_day_selections, len(baseline))
            maximum_day_selections = max(maximum_day_selections, len(baseline))
            if len(baseline) != len(set(baseline)):
                duplicate_day_runs += 1
            if baseline != with_touches:
                touch_changed_auto_runs += 1

    if duplicate_day_runs:
        errors.append(f"{duplicate_day_runs} automatic day simulations repeated")
    if touch_changed_auto_runs:
        errors.append(
            f"Touch activity changed {touch_changed_auto_runs} automatic sequences"
        )

    # Required exact sequence: A1, T1, T2, A2, A3, T3, A4.
    pool_index = VIBES.index("EVENING") * 3 + PHASES.index("MIDDLE")
    auto_capacity = len(auto_pools[pool_index])
    auto_counter = 7
    touch_counter = 0
    interleaving: list[tuple[str, tuple[str, str]]] = []
    for label in ("A1", "T1", "T2", "A2", "A3", "T3", "A4"):
        if label.startswith("A"):
            index = auto_permuted(pool_index, auto_counter, auto_capacity)
            auto_counter = (auto_counter + 1) % auto_capacity
            pair = pair_from_auto_index(
                pool_index, index, configs, auto_setups, auto_reactions
            )
        else:
            index = touch_permuted(pool_index, touch_counter)
            touch_counter = (touch_counter + 1) % TOUCH_CAPACITY
            pair = pair_from_touch_index(
                pool_index, index, touch_setups, touch_reactions
            )
        interleaving.append((label, pair))

    expected_auto_indexes = [
        auto_permuted(pool_index, counter, auto_capacity)
        for counter in range(7, 11)
    ]
    actual_auto_pairs = [pair for label, pair in interleaving if label.startswith("A")]
    expected_auto_pairs = [
        pair_from_auto_index(pool_index, index, configs, auto_setups, auto_reactions)
        for index in expected_auto_indexes
    ]
    touch_pairs = [pair for label, pair in interleaving if label.startswith("T")]
    if actual_auto_pairs != expected_auto_pairs:
        errors.append("Required trigger interleaving consumed AUTO progression")
    if len(touch_pairs) != len(set(touch_pairs)):
        errors.append("Required trigger interleaving repeated TOUCH output")

    print("VIBE | PHASE | MINUTES | AUTO CAPACITY | TOUCH CAPACITY")
    for row in pool_rows:
        print(" | ".join(str(value) for value in row))

    print()
    print(f"AUTO total outputs: {len(auto_outputs)}")
    print(f"AUTO unique outputs: {len(set(auto_outputs))}")
    print(f"AUTO duplicates: {len(auto_duplicates)}")
    print(f"AUTO cross-context duplicates: {len(auto_cross_context)}")
    print(f"TOUCH total outputs: {len(touch_outputs)}")
    print(f"TOUCH unique outputs: {len(set(touch_outputs))}")
    print(f"TOUCH duplicates: {len(touch_duplicates)}")
    print(f"TOUCH cross-context duplicates: {len(touch_cross_context)}")
    print(f"Exact full-message AUTO/TOUCH overlap: {len(overlap)}")
    print(f"Overlong fragments: {len(overlong)}")
    print(f"Non-ASCII fragments: {len(non_ascii)}")
    print(f"Flirty/touch AUTO fragments: {len(flirty_auto)}")
    print(f"Unsafe TOUCH fragments: {len(unsafe_touch)}")
    print(f"Awkward repeated-MISS TOUCH pairs: {len(awkward_touch)}")
    print(f"English-heavy AUTO setup fragments: {len(english_heavy_auto_setups)}")
    print(f"Forbidden awkward phrase hits: {len(forbidden_phrase_hits)}")
    print(f"Unnatural VEE fragments: {len(vee_fragments)}")
    print(f"Exact-time leaks: {len(exact_time_leaks)}")
    print(f"Unique TOUCH reaction fragments: {unique_touch_reactions}")
    print(f"24-hour boot-time simulations: {tested_starts}")
    print(f"Selections per simulated day: {minimum_day_selections}..{maximum_day_selections}")
    print(f"24-hour simulations with AUTO repeats: {duplicate_day_runs}")
    print(f"AUTO sequences changed by injected touches: {touch_changed_auto_runs}")

    print("\n=== REQUIRED TRIGGER INTERLEAVING ===")
    for label, (line1, line2) in interleaving:
        print(f"{label}: {line1} / {line2}")

    print_samples("AUTO", auto_pools)
    print_samples("TOUCH", touch_pools)

    fixed_phrase_storage = len(fragments) * 17
    metadata_storage = len(configs) * 4
    print()
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
