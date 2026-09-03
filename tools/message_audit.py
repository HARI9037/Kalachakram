#!/usr/bin/env python3
"""Audit Kalachakram's trigger-aware AUTO and TOUCH message engines."""

from __future__ import annotations

import re
import sys
from collections import Counter, defaultdict
from math import gcd
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
AUTO_RANDOM_SALT = 0xA341316C
TOUCH_RANDOM_SALT = 0xC8013EA4


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


def mix_seed(value: int) -> int:
    value &= UINT32_MASK
    value ^= value >> 16
    value = (value * 0x7FEB352D) & UINT32_MASK
    value ^= value >> 15
    value = (value * 0x846CA68B) & UINT32_MASK
    value ^= value >> 16
    return value or 0x6D2B79F5


def next_random(state: int) -> int:
    value = state
    value ^= (value << 13) & UINT32_MASK
    value ^= value >> 17
    value ^= (value << 5) & UINT32_MASK
    return (value & UINT32_MASK) or 0x6D2B79F5


class PermutationEngine:
    def __init__(self, seed: int, salt: int) -> None:
        self.random_state = mix_seed(seed ^ salt)
        self.counters = [0] * 24
        self.parameters: list[tuple[int, int] | None] = [None] * 24
        self.last_indexes: list[int | None] = [None] * 24
        self.parameter_history: list[list[tuple[int, int]]] = [
            [] for _ in range(24)
        ]

    def _random(self) -> int:
        self.random_state = next_random(self.random_state)
        return self.random_state

    def _choose_multiplier(self, capacity: int, setup_count: int) -> int:
        while True:
            candidate = 1 + self._random() % (capacity - 1)
            setup_step = candidate % setup_count
            if (
                gcd(candidate, capacity) == 1
                and setup_step not in (1, setup_count - 1)
            ):
                return candidate

    def _configure(
        self,
        pool_index: int,
        capacity: int,
        setup_count: int,
    ) -> None:
        previous = self.parameters[pool_index]
        last_index = self.last_indexes[pool_index]
        while True:
            multiplier = self._choose_multiplier(capacity, setup_count)
            offset = self._random() % capacity
            parameters = (multiplier, offset)
            if previous is None or (
                parameters != previous and offset != last_index
            ):
                break
        self.parameters[pool_index] = parameters
        self.parameter_history[pool_index].append(parameters)

    def select(
        self,
        pool_index: int,
        capacity: int,
        setup_count: int,
        initial_counter: int = 0,
    ) -> int:
        if self.parameters[pool_index] is None:
            self._configure(pool_index, capacity, setup_count)
            self.counters[pool_index] = initial_counter % capacity

        multiplier, offset = self.parameters[pool_index]  # type: ignore[misc]
        selected = (multiplier * self.counters[pool_index] + offset) % capacity
        self.last_indexes[pool_index] = selected
        self.counters[pool_index] += 1
        if self.counters[pool_index] >= capacity:
            self.counters[pool_index] = 0
            self._configure(pool_index, capacity, setup_count)
        return selected


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
    seed: int,
) -> list[tuple[str, str]]:
    auto_engine = PermutationEngine(seed, AUTO_RANDOM_SALT)
    touch_engine = PermutationEngine(seed, TOUCH_RANDOM_SALT)
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

        combination = auto_engine.select(
            pool_index,
            capacity,
            setup_count,
            0,
        )
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
            touch_engine.select(
                pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
            )
            touch_engine.select(
                pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
            )

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
    random_order_rows: list[tuple[str, list[int], list[int]]] = []
    auto_boundary_repeat_failures = 0
    touch_boundary_repeat_failures = 0
    wrong_context_auto = 0
    wrong_context_touch = 0
    identical_parameter_cycles = 0
    identical_seed_orders = 0
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
        test_seed = 0x6B414C41 ^ (pool_index * 0x1021)
        auto_engine = PermutationEngine(test_seed, AUTO_RANDOM_SALT)
        auto_indexes = [
            auto_engine.select(pool_index, auto_capacity, setup_count)
            for _ in range(auto_capacity + 1)
        ]
        touch_engine = PermutationEngine(test_seed, TOUCH_RANDOM_SALT)
        touch_indexes = [
            touch_engine.select(
                pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
            )
            for _ in range(TOUCH_CAPACITY + 1)
        ]

        if len(set(auto_indexes[:auto_capacity])) != auto_capacity:
            errors.append(f"{context}: AUTO permutation is not full-cycle")
        if len(set(touch_indexes[:touch_capacity])) != touch_capacity:
            errors.append(f"{context}: TOUCH permutation is not full-cycle")
        if auto_indexes[auto_capacity - 1] == auto_indexes[auto_capacity]:
            auto_boundary_repeat_failures += 1
        if touch_indexes[touch_capacity - 1] == touch_indexes[touch_capacity]:
            touch_boundary_repeat_failures += 1
        if len(set(auto_engine.parameter_history[pool_index][:2])) != 2:
            identical_parameter_cycles += 1
        if len(set(touch_engine.parameter_history[pool_index][:2])) != 2:
            identical_parameter_cycles += 1

        second_seed_engine = PermutationEngine(test_seed ^ 0x9E3779B9, AUTO_RANDOM_SALT)
        second_seed_indexes = [
            second_seed_engine.select(pool_index, auto_capacity, setup_count)
            for _ in range(min(12, auto_capacity))
        ]
        if auto_indexes[:len(second_seed_indexes)] == second_seed_indexes:
            identical_seed_orders += 1

        for selected in auto_indexes[:auto_capacity]:
            if not 0 <= selected < auto_capacity:
                wrong_context_auto += 1
            elif pair_from_auto_index(
                pool_index, selected, configs, auto_setups, auto_reactions
            ) not in auto_pools[pool_index]:
                wrong_context_auto += 1
        for selected in touch_indexes[:touch_capacity]:
            if not 0 <= selected < touch_capacity:
                wrong_context_touch += 1
            elif pair_from_touch_index(
                pool_index, selected, touch_setups, touch_reactions
            ) not in touch_pools[pool_index]:
                wrong_context_touch += 1

        if pool_index in (0, 7, 13, 19, 23):
            random_order_rows.append(
                (context, auto_indexes[:10], touch_indexes[:10])
            )

    if auto_boundary_repeat_failures:
        errors.append(
            f"AUTO cycle-boundary repeats: {auto_boundary_repeat_failures}"
        )
    if touch_boundary_repeat_failures:
        errors.append(
            f"TOUCH cycle-boundary repeats: {touch_boundary_repeat_failures}"
        )
    if identical_parameter_cycles:
        errors.append(
            f"Cycles that reused permutation parameters: {identical_parameter_cycles}"
        )
    if identical_seed_orders:
        errors.append(
            f"Contexts with identical orders across different seeds: {identical_seed_orders}"
        )
    if wrong_context_auto or wrong_context_touch:
        errors.append(
            "Wrong-context selections: "
            f"AUTO={wrong_context_auto}, TOUCH={wrong_context_touch}"
        )

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
    # Mechanical regression guard; the complete reaction bank is also
    # manually reviewed because flirt quality cannot be proven by keywords.
    clearly_flirty_markers = (
        "MISSED", "THINKING OF ME", "RESIST", "BLUSH", "WITH ME",
        "YOU LIKE", "LIKE THIS", "JUST US", "CLOSER", "CAME FOR ME",
        "SUITS US", "YOUR WAKE-UP", "CRUSH", "CHOSE ME", "SPECIAL",
        "FLIRT", "CUTE", "ME BEFORE", "EAGER", "LOOK AT ME",
        "FLATTERED", "ADORABLE", "KEEP LOOKING", "DATE", "PICK ME",
        "SMOOTH MOVE", "EYES ON ME", "CHOOSE ME", "YOUR SNACK",
        "SAVE ME A SEAT", "HAVE TASTE", "DESSERT IS ME", "I LIKE YOU",
        "HUNGRY FOR ME", "SWEET CHOICE", "WANT ATTENTION", "MISSED MY FACE",
        "YOU NEED ME", "YOUR BREAK", "CAUGHT YOU", "YOU FOUND ME",
        "LIKE THE LOOK", "STAY A WHILE", "INTO ME", "DON'T LEAVE",
        "YOUR ESCAPE", "HOME WITH ME", "LINGER WITH ME", "FOR TWO",
        "YOU + ME", "MISS YOU", "CHARMING", "STAY CLOSE", "LIKE ME",
        "FAVORITE TAP", "DREAM OF ME", "STAY UP WITH ME", "WE LOOK CUTE",
        "BLUSH BEFORE", "DREAMY",
    )
    not_clearly_flirty_reactions = [
        reaction
        for reaction in touch_reactions
        if not any(marker in reaction for marker in clearly_flirty_markers)
    ]
    not_clearly_flirty_outputs = [
        pair for pair in touch_outputs if pair[1] in not_clearly_flirty_reactions
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
    if not_clearly_flirty_outputs:
        errors.append(
            "TOUCH outputs without an explicitly flirty reaction: "
            f"{len(not_clearly_flirty_outputs)}"
        )
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
                seed=0x4B414C41 ^ start_minute ^ (start_second << 16),
            )
            with_touches = simulate_automatic_day(
                start_minute,
                start_second,
                configs,
                auto_setups,
                auto_reactions,
                inject_touches=True,
                seed=0x4B414C41 ^ start_minute ^ (start_second << 16),
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
    setup_count = configs[pool_index][1]
    interleaved_auto_engine = PermutationEngine(0x6B414C41, AUTO_RANDOM_SALT)
    interleaved_touch_engine = PermutationEngine(0x6B414C41, TOUCH_RANDOM_SALT)
    interleaving: list[tuple[str, tuple[str, str]]] = []
    for label in ("A1", "T1", "T2", "A2", "A3", "T3", "A4"):
        if label.startswith("A"):
            index = interleaved_auto_engine.select(
                pool_index, auto_capacity, setup_count
            )
            pair = pair_from_auto_index(
                pool_index, index, configs, auto_setups, auto_reactions
            )
        else:
            index = interleaved_touch_engine.select(
                pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
            )
            pair = pair_from_touch_index(
                pool_index, index, touch_setups, touch_reactions
            )
        interleaving.append((label, pair))

    baseline_auto_engine = PermutationEngine(0x6B414C41, AUTO_RANDOM_SALT)
    expected_auto_indexes = [
        baseline_auto_engine.select(
            pool_index, auto_capacity, setup_count
        )
        for _ in range(4)
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
    overlay_auto_engine = PermutationEngine(0x6B414C41, AUTO_RANDOM_SALT)
    overlay_touch_engine = PermutationEngine(0x6B414C41, TOUCH_RANDOM_SALT)
    a1_index = overlay_auto_engine.select(
        pool_index, auto_capacity, setup_count
    )
    a2_index = overlay_auto_engine.select(
        pool_index, auto_capacity, setup_count
    )
    a1 = pair_from_auto_index(
        pool_index, a1_index, configs, auto_setups, auto_reactions
    )
    a2 = pair_from_auto_index(
        pool_index, a2_index, configs, auto_setups, auto_reactions
    )
    t1_index = overlay_touch_engine.select(
        pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
    )
    t2_index = overlay_touch_engine.select(
        pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
    )
    t1 = pair_from_touch_index(
        pool_index, t1_index, touch_setups, touch_reactions
    )
    t2 = pair_from_touch_index(
        pool_index, t2_index, touch_setups, touch_reactions
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
    new_context_engine = PermutationEngine(0x6B414C41, AUTO_RANDOM_SALT)
    new_index = new_context_engine.select(
        new_pool_index,
        new_capacity,
        configs[new_pool_index][1],
    )
    new_normal = pair_from_auto_index(
        new_pool_index,
        new_index,
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

    touch_cycle_engine = PermutationEngine(0x6B414C41, TOUCH_RANDOM_SALT)
    touch_cycle_indexes = [
        touch_cycle_engine.select(
            pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
        )
        for _ in range(TOUCH_CAPACITY)
    ]
    touch_51st_index = touch_cycle_engine.select(
        pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
    )
    if len(set(touch_cycle_indexes)) != TOUCH_CAPACITY:
        errors.append("50-touch cycle is not unique")
    if touch_51st_index == touch_cycle_indexes[-1]:
        errors.append("51st touch immediately repeated the 50th touch")

    if a2_index == a1_index:
        errors.append("AUTO selection repeated after TOUCH restoration")
    if overlay_auto_engine.counters[pool_index] != 2:
        errors.append("AUTO counter reset during TOUCH overlay simulation")
    if a2_index in (0, 1):
        errors.append("Post-TOUCH AUTO test landed on source index 0 or 1")

    # Hundreds of mixed events must leave the AUTO order byte-for-byte equal
    # to an AUTO-only engine started with the same seed and context state.
    long_auto_engine = PermutationEngine(0x13579BDF, AUTO_RANDOM_SALT)
    long_touch_engine = PermutationEngine(0x13579BDF, TOUCH_RANDOM_SALT)
    long_auto_indexes: list[int] = []
    long_touch_indexes: list[int] = []
    for event in range(600):
        if event % 5 in (1, 3):
            long_touch_indexes.append(
                long_touch_engine.select(
                    pool_index, TOUCH_CAPACITY, TOUCH_SETUP_COUNT
                )
            )
        else:
            long_auto_indexes.append(
                long_auto_engine.select(
                    pool_index, auto_capacity, setup_count
                )
            )

    long_baseline_engine = PermutationEngine(0x13579BDF, AUTO_RANDOM_SALT)
    long_baseline_indexes = [
        long_baseline_engine.select(
            pool_index, auto_capacity, setup_count
        )
        for _ in range(len(long_auto_indexes))
    ]
    if long_auto_indexes != long_baseline_indexes:
        errors.append("Long TOUCH interleaving changed AUTO order")

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
    print(
        "Clearly flirty TOUCH outputs: "
        f"{len(touch_outputs) - len(not_clearly_flirty_outputs)}"
    )
    print(f"Not clearly flirty TOUCH outputs: {len(not_clearly_flirty_outputs)}")
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
    print(f"Wrong-context AUTO selections: {wrong_context_auto}")
    print(f"Wrong-context TOUCH selections: {wrong_context_touch}")
    print(f"AUTO cycle-boundary repeat failures: {auto_boundary_repeat_failures}")
    print(f"TOUCH cycle-boundary repeat failures: {touch_boundary_repeat_failures}")
    print(f"Reused cycle parameter pairs: {identical_parameter_cycles}")
    print(f"Identical orders across different seeds: {identical_seed_orders}")
    print(f"Long interleaving AUTO events: {len(long_auto_indexes)}")
    print(f"Long interleaving TOUCH events: {len(long_touch_indexes)}")
    print(
        "Long interleaving changed AUTO order: "
        f"{long_auto_indexes != long_baseline_indexes}"
    )

    print("\n=== REQUIRED TRIGGER INTERLEAVING ===")
    for label, (line1, line2) in interleaving:
        print(f"{label}: {line1} / {line2}")

    print("\n=== FLIRT OVERLAY SIMULATION ===")
    print(
        f"Indexes: A{a1_index} -> T{t1_index} -> "
        f"restore A{a1_index} -> A{a2_index}"
    )
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
    print(f"51st index: {touch_51st_index}")
    print(f"51st differs from 50th: {touch_51st_index != touch_cycle_indexes[-1]}")
    print("Millis rollover tests: PASS")

    print("\n=== RANDOMIZED INDEX SAMPLES ===")
    for context, auto_indexes, touch_indexes in random_order_rows:
        print(f"{context} AUTO: {', '.join(str(index) for index in auto_indexes)}")
        print(f"{context} TOUCH: {', '.join(str(index) for index in touch_indexes)}")

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
