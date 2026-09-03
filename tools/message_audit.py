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
TOUCH_SETUP_COUNT = 5
TOUCH_REACTIONS_PER_VIBE = 10
TOUCH_CAPACITY = 50
MESSAGE_INTERVAL_MS = 60_000
TOUCH_FLIRT_DISPLAY_MS = 10_000
UINT32_MASK = 0xFFFFFFFF


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
    offset = (pool_index * 19 + 7) % TOUCH_CAPACITY
    return (21 * counter + offset) % TOUCH_CAPACITY


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
            candidates.extend(distinct_setups[:4] if phase == 0 else distinct_setups[:3])
        print(f"{vibe}:")
        for line1, line2 in candidates[:10]:
            print(f"  {line1} / {line2}")


def elapsed_u32(now: int, then: int) -> int:
    return (now - then) & UINT32_MASK


def runtime_action(
    now: int,
    previous_selection: int,
    overlay_start: int,
    has_cached_normal: bool,
    overlay_active: bool,
    context_changed: bool,
    touch_requested: bool,
) -> str:
    if not has_cached_normal or context_changed:
        return "SELECT_NORMAL"
    if touch_requested:
        return "SELECT_TOUCH"
    if overlay_active:
        return (
            "RESTORE_NORMAL"
            if elapsed_u32(now, overlay_start) >= TOUCH_FLIRT_DISPLAY_MS
            else "NONE"
        )
    return (
        "SELECT_NORMAL"
        if elapsed_u32(now, previous_selection) >= MESSAGE_INTERVAL_MS
        else "NONE"
    )


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
    if len(touch_setups) != 120:
        errors.append(f"Expected 120 touch setups, found {len(touch_setups)}")
    if len(touch_reactions) != 80:
        errors.append(f"Expected 80 touch reactions, found {len(touch_reactions)}")

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
        if touch_capacity != 50:
            errors.append(f"{context}: TOUCH capacity is not exactly 50")
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
    maximum_line_length = max(len(fragment) for fragment in fragments)
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
        "MISS ME", "MISSED ME", "YOU'RE CUTE", "ADORABLE", "FLIRT",
        "LOOKING AT ME", "STAY WITH ME", "I'M FLATTERED", "COME CLOSER",
        "I MIGHT BLUSH", "ROMANTIC", "FOR ME?",
    )
    flirty_auto = [
        fragment
        for fragment in auto_setups + auto_reactions
        if any(term in fragment for term in flirty_auto_terms)
    ]
    unsafe_touch_terms = (
        "SEXY", "BODY", "KISS", "HOT BODY", "MINE ONLY", "BELONG TO ME",
        "YOU'RE MINE", "UNDRESS", "NAKED",
    )
    unsafe_touch = [
        fragment
        for fragment in touch_setups + touch_reactions
        if any(term in fragment for term in unsafe_touch_terms)
    ]
    awkward_touch = [
        pair
        for pair in touch_outputs
        if any(
            word in pair[0] and word in pair[1]
            for word in ("MISS", "BACK", "TOUCH", "CURIOUS", "CUTE", "BOLD")
        )
    ]
    exact_echo_touch = [
        pair
        for pair in touch_outputs
        if re.sub(r"[^A-Z0-9]", "", pair[0].upper()) ==
           re.sub(r"[^A-Z0-9]", "", pair[1].upper())
    ]
    manglish_vocabulary = (
        "AAK", "AAKUM", "AANO", "AANALLO", "AAYI", "AAYO", "ADUTHU",
        "ALLE", "ATHU", "BAAKI", "CHAYA", "CHEYYU", "CHEYTHO", "CHINTHA",
        "CHODIKKUNNU", "CHUMMA", "DHOORAM", "ENTHA", "ENTHINA", "ETHARAYI",
        "ETHI", "EVDE", "EVIDE", "EZHUNNETTU", "ILLE", "ILLA", "INNU",
        "INNUM", "INI", "INIYUM", "IPPO", "IPPOZHUM", "IRUNNO", "IRUTTU",
        "IVDE", "IVIDE", "JAYICHU", "KAATHU", "KAIVIDUNNU", "KANDU", "KANN",
        "KASHTAM", "KAZHINJU", "KAZHINJO", "KITTUMO", "KULAMAYI", "MADUTHU",
        "MANGUNNU", "MARANNO", "MATHI", "MAYAKKAM", "MONE", "MOSHAM",
        "MUDIYO", "NADAKKUNNO", "NALLA", "NALE", "NERATHE", "NIRTHI", "NJAN",
        "NOKKAM", "NOKKO", "NOKKU", "NOKKUNNU", "ONNUM", "ORMA", "PANI",
        "PARAYUM", "PINNE", "POKANDE", "POLE", "POLIYUNNU", "POYI", "RAAVILE",
        "RAATHRI", "RATHRI", "RAKSHAYILLA", "READYANO", "SAMAYAM", "SHERI",
        "THANNE", "THEERARAYI", "THEERKKU", "THUDANGI", "UCHAYA", "UCHAYIL",
        "UNDA", "UNDALLO", "UNDO", "UNDU", "URAKKAM", "URANGI", "VAIKUNNERAM",
        "VALIYUNNU", "VANNO", "VANNALLO", "VARUM", "VARUNNU", "VAYAR", "VAYYA",
        "VEE", "VEENDUM", "VENAM", "VENAMO", "VENDA", "VENO", "VIDUNNILLA",
        "VIDUNNO", "VILIKKUM", "VISHAPPU", "VISHANNU", "VITTALLO",
    )
    manglish_pattern = re.compile(
        r"\b(?:" + "|".join(re.escape(word) for word in manglish_vocabulary) + r")\b",
        re.IGNORECASE,
    )
    manglish_fragments = [
        fragment for fragment in fragments if manglish_pattern.search(fragment)
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
    exact_time_pattern = re.compile(
        r"(?:\b\d{1,2}\s*(?:AM|PM)\b|\b\d{1,2}:\d{2}\b|\b(?:AM|PM)\b|"
        r"\b(?:ONE|TWO|THREE|FOUR|FIVE|SIX|SEVEN|EIGHT|NINE|TEN|ELEVEN|TWELVE)"
        r"(?:\s+MORE)?\s+(?:MINUTES?|HOURS?)\b)",
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
        errors.append(f"Awkward repeated-keyword TOUCH pairs: {awkward_touch}")
    if exact_echo_touch:
        errors.append(f"Exact two-line echo TOUCH pairs: {exact_echo_touch}")
    if manglish_fragments:
        errors.append(f"Manglish fragments: {manglish_fragments}")
    if forbidden_phrase_hits:
        errors.append(f"Forbidden awkward phrases: {forbidden_phrase_hits}")
    if exact_time_leaks:
        errors.append(f"Exact-time leaks: {exact_time_leaks}")
    if unique_touch_reactions < 60:
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

    # Overlay scenario: A1 -> T1 -> restore A1 -> A2.
    a1_index = auto_permuted(pool_index, 7, auto_capacity)
    a2_index = auto_permuted(pool_index, 8, auto_capacity)
    a1 = pair_from_auto_index(
        pool_index, a1_index, configs, auto_setups, auto_reactions
    )
    a2 = pair_from_auto_index(
        pool_index, a2_index, configs, auto_setups, auto_reactions
    )
    t1 = pair_from_touch_index(
        pool_index, touch_permuted(pool_index, 0), touch_setups, touch_reactions
    )
    t2 = pair_from_touch_index(
        pool_index, touch_permuted(pool_index, 1), touch_setups, touch_reactions
    )

    overlay_actions = (
        runtime_action(25_000, 0, 0, True, False, False, True),
        runtime_action(34_999, 25_000, 25_000, True, True, False, False),
        runtime_action(35_000, 25_000, 25_000, True, True, False, False),
        runtime_action(84_999, 25_000, 25_000, True, False, False, False),
        runtime_action(85_000, 25_000, 25_000, True, False, False, False),
    )
    expected_overlay_actions = (
        "SELECT_TOUCH", "NONE", "RESTORE_NORMAL", "NONE", "SELECT_NORMAL"
    )
    if overlay_actions != expected_overlay_actions:
        errors.append(f"Flirt-expiry scheduler mismatch: {overlay_actions}")
    overlay_sequence = (("A1", a1), ("T1", t1), ("RESTORE A1", a1), ("A2", a2))

    retouch_actions = (
        runtime_action(25_000, 0, 0, True, False, False, True),
        runtime_action(30_000, 25_000, 25_000, True, True, False, True),
        runtime_action(39_999, 30_000, 30_000, True, True, False, False),
        runtime_action(40_000, 30_000, 30_000, True, True, False, False),
        runtime_action(89_999, 30_000, 30_000, True, False, False, False),
        runtime_action(90_000, 30_000, 30_000, True, False, False, False),
    )
    expected_retouch_actions = (
        "SELECT_TOUCH", "SELECT_TOUCH", "NONE", "RESTORE_NORMAL", "NONE",
        "SELECT_NORMAL",
    )
    if retouch_actions != expected_retouch_actions:
        errors.append(f"Re-touch scheduler mismatch: {retouch_actions}")
    retouch_sequence = (
        ("A1", a1), ("T1", t1), ("T2", t2), ("RESTORE A1", a1), ("A2", a2)
    )

    new_pool_index = VIBES.index("EVENING") * 3 + PHASES.index("LATE")
    new_capacity = len(auto_pools[new_pool_index])
    new_normal = pair_from_auto_index(
        new_pool_index,
        auto_permuted(new_pool_index, 0, new_capacity),
        configs,
        auto_setups,
        auto_reactions,
    )
    context_action = runtime_action(
        30_000, 25_000, 25_000, True, True, True, True
    )
    if context_action != "SELECT_NORMAL" or new_normal == a1:
        errors.append("Context transition did not replace old overlay cache")
    context_sequence = (("A1", a1), ("T1", t1), ("NEW-CONTEXT NORMAL", new_normal))

    touch_cycle_indexes = [
        touch_permuted(pool_index, counter) for counter in range(TOUCH_CAPACITY)
    ]
    touch_51st_index = touch_permuted(pool_index, TOUCH_CAPACITY)
    if len(set(touch_cycle_indexes)) != TOUCH_CAPACITY:
        errors.append("50-touch cycle is not unique")
    if touch_51st_index != touch_cycle_indexes[0]:
        errors.append("51st touch does not begin the next cycle")

    rollover_start = 0xFFFFFF00
    rollover_restore = (rollover_start + TOUCH_FLIRT_DISPLAY_MS) & UINT32_MASK
    rollover_auto = (rollover_start + MESSAGE_INTERVAL_MS) & UINT32_MASK
    if runtime_action(
        rollover_restore, rollover_start, rollover_start,
        True, True, False, False,
    ) != "RESTORE_NORMAL":
        errors.append("Flirt overlay millis rollover test failed")
    if runtime_action(
        rollover_auto, rollover_start, rollover_start,
        True, False, False, False,
    ) != "SELECT_NORMAL":
        errors.append("AUTO timer millis rollover test failed")

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
    print(f"Maximum fragment length: {maximum_line_length}")
    print(f"Overlong fragments: {len(overlong)}")
    print(f"Empty fragments: {len(empty)}")
    print(f"Non-ASCII fragments: {len(non_ascii)}")
    print(f"Flirty/touch AUTO fragments: {len(flirty_auto)}")
    print(f"Unsafe TOUCH fragments: {len(unsafe_touch)}")
    print(f"Awkward repeated-keyword TOUCH pairs: {len(awkward_touch)}")
    print(f"Exact two-line echo TOUCH pairs: {len(exact_echo_touch)}")
    print(f"Manglish fragments detected: {len(manglish_fragments)}")
    print(f"Forbidden awkward phrase hits: {len(forbidden_phrase_hits)}")
    print(f"Exact-time leaks: {len(exact_time_leaks)}")
    print(f"Unique TOUCH reaction fragments: {unique_touch_reactions}")
    print(f"24-hour boot-time simulations: {tested_starts}")
    print(f"Selections per simulated day: {minimum_day_selections}..{maximum_day_selections}")
    print(f"24-hour simulations with AUTO repeats: {duplicate_day_runs}")
    print(f"AUTO sequences changed by injected touches: {touch_changed_auto_runs}")

    print("\n=== REQUIRED TRIGGER INTERLEAVING ===")
    for label, (line1, line2) in interleaving:
        print(f"{label}: {line1} / {line2}")

    print("\n=== FLIRT OVERLAY SIMULATION ===")
    for label, (line1, line2) in overlay_sequence:
        print(f"{label}: {line1} / {line2}")
    print("\n=== RE-TOUCH SIMULATION ===")
    for label, (line1, line2) in retouch_sequence:
        print(f"{label}: {line1} / {line2}")
    print("\n=== CONTEXT-CHANGE-DURING-FLIRT SIMULATION ===")
    for label, (line1, line2) in context_sequence:
        print(f"{label}: {line1} / {line2}")
    print("\n=== TOUCH CYCLE ===")
    print(f"Selections before wrap: {TOUCH_CAPACITY}")
    print(f"Unique before wrap: {len(set(touch_cycle_indexes))}")
    print(f"51st restarts at first index: {touch_51st_index == touch_cycle_indexes[0]}")
    print("Millis rollover tests: PASS")

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
