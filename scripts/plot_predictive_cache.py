#!/usr/bin/python3
"""
@brief   Plot results of predictive cache policy runs.

@details    This script scans for single-line JSONs that begin with "> {...}".
            It then parses the JSON for certain required fields.
            It plots this data.

@example

```bash
cd $(git rev-parse --show-toplevel)
./build/src/predictor/predictor_exe [...] > data.txt
python3 scripts/plot_predictive_cache.py -i data.txt [...]
```
"""


import argparse
import json
from pathlib import Path
from string import Template
from typing import Callable

import numpy as np
import matplotlib.pyplot as plt

IDENTITY_X = lambda x: x
IDENTITY_X_D = lambda x, _d: x

# Smooth log functions by converting 0 to 1.0.
# AKA "Laplace smoothing".
ADDITIVE_SMOOTHING = lambda x: 1.0 if x == 0.0 else x

SCALE_B_TO_GiB = lambda x: x / (1 << 30)
SCALE_MS_TO_HOUR = lambda x: x / 1000 / 3600
# NOTE  Not all results have the SHARDS ratio, so if they don't, I assume the
#       ratio they use is 1.0.
shards_ratio_keys = ["Kwargs", "shards_ratio"]
SCALE_SHARDS_FUNC = lambda x, d: x / get_stat_or(d, shards_ratio_keys, 1)
# Convert ms to hours without SHARDS.
HOURS_NO_SHARDS_ARGS = [
    SCALE_MS_TO_HOUR,
    IDENTITY_X_D,
]
# Factor SHARDS into a counter.
COUNT_SHARDS_ARGS = [
    IDENTITY_X,
    SCALE_SHARDS_FUNC,
]
# Convert B to GiB with SHARDS factored in.
GiB_SHARDS_ARGS = [
    SCALE_B_TO_GiB,
    SCALE_SHARDS_FUNC,
]
# Describe an axis using the SHARDS-adjusted maximum cache capacity.
CAPACITY_GIB_ARGS = [
    "Capacity [GiB]",
    lambda d: get_stat(d, ["Capacity [B]"]),
    SCALE_B_TO_GiB,
    SCALE_SHARDS_FUNC,
]


def parse_number(num: str | float | int) -> float | int:
    """Parse a quantity to canonical units (bytes, milliseconds)."""
    memsizes = {
        "TB": 1e12,
        "GB": 1e9,
        "MB": 1e6,
        "KB": 1e3,
        "TiB": 1 << 40,
        "GiB": 1 << 30,
        "MiB": 1 << 20,
        "KiB": 1 << 10,
        "B": 1,
    }
    plural_times = {
        "years": 365 * 24 * 3600 * 1000,
        "days": 24 * 3600 * 1000,
        "hours": 3600 * 1000,
        "minutes": 60 * 1000,
        "seconds": 1000,
        "milliseconds": 1,
    }
    single_times = {
        "year": 365 * 24 * 3600 * 1000,
        "day": 24 * 3600 * 1000,
        "hour": 3600 * 1000,
        "minute": 60 * 1000,
        "second": 1000,
        "millisecond": 1,
    }
    short_times = {
        "h": 3600 * 1000,
        "min": 60 * 1000,
        "sec": 1000,
        "s": 1000,
        "ms": 1,
    }
    if isinstance(num, (float, int)):
        return num
    if not num[-1].isalpha():
        return float(num)
    val, unit = num.split()
    return (
        float(val) * {**memsizes, **plural_times, **single_times, **short_times}[unit]
    )


def get_stat(data: dict[str, object], keys: list[str]) -> float:
    ans = data
    for key in keys:
        if not isinstance(ans, dict):
            raise TypeError(f"expected dict, got {ans}")
        r = ans.get(key, None)
        if r is None:
            raise KeyError(f"missing '{key}' in {ans}")
        ans = r
    return parse_number(ans)


def get_stat_or(data: dict[str, object], keys: list[str], otherwise: float) -> float:
    try:
        return get_stat(data, keys)
    except KeyError:
        return otherwise
    except TypeError:
        return otherwise


def div_or(numerator: float, denominator: float, default: float = 0.0) -> float:
    """Divide or return default [=0] if NaN."""
    if denominator == 0.0:
        return default
    return numerator / denominator


def shards_adj(x: float, d) -> float:
    ratio = get_stat(d, ["Extras", "SHARDS", ".sampling_ratio"])
    total = get_stat(d, ["Extras", "SHARDS", ".num_entries_seen"])
    sampled = get_stat(d, ["Extras", "SHARDS", ".num_entries_processed"])
    expected_sampled = ratio * total
    hist_adj = expected_sampled - sampled
    # The expected_sampled is the new sum of the histogram after we
    # added the hist_adj. This means that we divide all values in
    # this histogram by this, hence the division here.
    mrc_adj = hist_adj / expected_sampled
    return x + mrc_adj


def parse_data(input_file: Path) -> dict[tuple[float, float, str], list[dict]]:
    """@return {(lower-ratio: float, upper-ratio: float, lifetime-mode: str): JSON}"""
    if not input_file.exists():
        raise FileNotFoundError(f"'{str(input_file)}' not found")
    file_data = input_file.read_text().splitlines()
    # Parse into JSON for each run of the cache
    accum = []
    for line in file_data:
        if line.startswith("> "):
            try:
                data = json.loads(line[2:])
            except json.decoder.JSONDecodeError as e:
                print(f"bad JSON: '{line[2:]}'")
                raise e

            accum.append(data)
    # Group data by (lower-ratio, upper-ratio, lifetime-mode).
    data: dict[tuple[float, float, str], list[dict]] = {}
    for d in accum:
        key = (
            float(d["Lower Ratio"]),
            float(d["Upper Ratio"]),
            "EvictionTime",
        )
        data.setdefault(key, []).append(d)
    return data


def plot_this_configuration(
    policy: str,
    lower_ratio: int | float,
    upper_ratio: int | float,
    eviction_mode: str,
) -> bool:
    return (
        get_label(policy, lower_ratio, upper_ratio, eviction_mode, default=None)
        is not None
    )


def get_label(
    policy: str,
    lower_ratio: int | float,
    upper_ratio: int | float,
    eviction_mode: str = "EvictionTime",
    *,
    default: Template | None = Template("$lower_ratio:$upper_ratio $eviction_mode"),
) -> str:
    l, u = float(lower_ratio), float(upper_ratio)
    return {
        (0.0, 1.0, "EvictionTime"): f"{policy}/Proactive-TTL",
        (0.0, 0.0, "EvictionTime"): f"{policy}/Lazy-TTL",
        (0.5, 0.5, "EvictionTime"): "Unbiased Binary Predictor",
        (1.0, 1.0, "EvictionTime"): "TTL-only",
    }.get(
        (l, u, eviction_mode),
        (
            default.substitute(
                lower_ratio=l, upper_ratio=u, eviction_mode=eviction_mode
            )
            if default is not None
            else None
        ),
    )


def get_scaled_fixed_data(
    get_func: list[str] | Callable[[dict[str, object]], float],
    scale_func: Callable[[dict[str, object]], float] = IDENTITY_X,
    fix_func: Callable[[float, dict[str, object]], float] = IDENTITY_X_D,
    *,
    vector: bool = False,
) -> Callable[[dict[str, object]], list[float] | float]:
    """@return lambda"""
    if vector:
        return lambda d: [fix_func(scale_func(x), d) for x in get_func(d)]
    return lambda d: fix_func(scale_func(get_func(d)), d)


def plot(
    ax: plt.Axes,
    policy: str,
    all_data: dict[str, list[object]],
    # X-axis data
    x_label: str,
    get_x_func: list[str] | Callable[[dict[str, object]], float],
    scale_x_func: Callable[[dict[str, object]], float],
    fix_x_func: Callable[[float, dict[str, object]], float],
    # Y-axis data
    y_label: str,
    get_y_func: Callable[[dict[str, object]], float],
    scale_y_func: Callable[[dict[str, object]], float],
    fix_y_func: Callable[[float, dict[str, object]], float],
    *,
    set_ylim_to_one: bool = False,
):
    """
    @note   It is a bit involved to correct the X-axis data with
            configurable parameters, so we just hard code the correction.
    @param get_{x,y}_func: fn (JSON) -> float. Get data.
    @param scale_{x,y}_func: fn (float) -> float. Scale data to different unit.
    @param fix_{x,y}_func: fn (float, JSON) -> float. Correct data (e.g. SHARDS).
    """
    # Get corrected y value from a JSON
    f_x = lambda d: fix_x_func(scale_x_func(get_x_func(d)), d)
    f_y = lambda d: fix_y_func(scale_y_func(get_y_func(d)), d)

    ax.set_title(f"{y_label} vs {x_label}")
    for (l, u, tm), d_list in all_data.items():
        cache_sizes = [f_x(d) for d in d_list]
        y = [f_y(d) for d in d_list]
        marker = {
            "EvictionTime": "o",
            # "LifeTime": "x",
        }.get(tm, None)
        colour = {
            (0.0, 0.0): "black",
            # (0.25, 0.25): "pink",
            # (0.25, 0.75): "gold",
            # (0.75, 0.75): "teal",
            (1.0, 1.0): "slategrey",
            (0.5, 0.5): "red",
            (0.0, 1.0): "purple",
        }.get((l, u), None)
        if colour is None or marker is None:
            continue
        # Prettify labels
        label = get_label(policy, l, u, tm)
        if not plot_this_configuration(policy, l, u, tm):
            continue
        ax.plot(cache_sizes, y, marker + ":", color=colour, label=label)
    # Plot between [0.0, 1.0] for MRC curves.
    if set_ylim_to_one:
        ax.set_ylim(0.0, 1.0)
    ax.legend()
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)


def plot_lines(
    ax: plt.Axes,
    all_data: dict[str, list[object]],
    label_func: Callable[[dict[str, object]], str | None],
    # X-axis data
    x_label: str,
    get_x_list_func: list[str] | Callable[[dict[str, object]], list[float] | None],
    scale_x_func: Callable[[dict[str, object]], float],
    fix_x_func: Callable[[float, dict[str, object]], float],
    # Y-axis data
    y_label: str,
    get_y_list_func: Callable[[dict[str, object]], list[float] | None],
    scale_y_func: Callable[[dict[str, object]], float],
    fix_y_func: Callable[[float, dict[str, object]], float],
    *,
    plot_x_axis: bool = True,
    colours: list[str] | None = None,
):
    """
    @brief  Plot data where each "point" is a line.
    @note   It is a bit involved to correct the X-axis data with
            configurable parameters, so we just hard code the correction.
    @param get_{x,y}_func: fn (JSON) -> float. Get data.
    @param scale_{x,y}_func: fn (float) -> float. Scale data to different unit.
    @param fix_{x,y}_func: fn (float, JSON) -> float. Correct data (e.g. SHARDS).
    """
    # Get corrected y value from a JSON
    f_x = lambda d: (
        [fix_x_func(scale_x_func(x), d) for x in get_x_list_func(d)]
        if get_x_list_func(d) is not None
        else None
    )
    f_y = lambda d: (
        [fix_y_func(scale_y_func(x), d) for x in get_y_list_func(d)]
        if get_y_list_func(d) is not None
        else None
    )
    colours = iter(
        [
            "red",
            "orangered",
            "orange",
            "yellow",
            "greenyellow",
            "green",
            "turquoise",
            "blue",
            "purple",
            "violet",
            "grey",
            "black",
        ]
        if colours is None
        else colours
    )

    ax.set_title(f"{y_label} vs {x_label}")
    if plot_x_axis:
        ax.axhline(y=0, color="black", linestyle="--", label="y=0")
    for (l, u, tm), d_list in all_data.items():
        x_list = [f_x(d) for d in d_list]
        y_list = [f_y(d) for d in d_list]
        labels = [label_func(d) for d in d_list]
        for x, y, label, c in reversed(list(zip(x_list, y_list, labels, colours))):
            if x is None or y is None:
                continue
            ax.plot(x, y, c=c, label=label)
    ax.legend()
    ax.set_xlabel(x_label)
    ax.set_ylabel(y_label)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, required=True, help="input file")
    parser.add_argument(
        "--policy", "-p", choices=["lru", "lfu"], required=True, help="eviction policy"
    )
    parser.add_argument("--mrc", type=Path, help="MRC output path")
    parser.add_argument(
        "--removal-stats", "-r", type=Path, help="plot removed bytes output path"
    )
    parser.add_argument(
        "--eviction-time", type=Path, help="eviction time statistics output path"
    )
    parser.add_argument("--sizes", type=Path, help="cache sizes statistics output path")
    parser.add_argument(
        "--remaining-lifetime", type=Path, help="cache sizes statistics output path"
    )
    parser.add_argument(
        "--temporal-statistics", type=Path, help="temporal statistics output plot"
    )
    parser.add_argument(
        "--eviction-histograms", type=Path, help="eviction histograms output plot"
    )
    parser.add_argument("--other", type=Path, help="other plot")
    args = parser.parse_args()

    input_file = args.input.resolve()
    data = parse_data(input_file)
    p = args.policy.upper()
    if args.mrc is not None:
        fig, axes = plt.subplots(1, 2, sharex=True, sharey=True)
        fig.set_size_inches(10, 5)
        fig.suptitle("Miss Ratio Curves")
        plot(
            axes[0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Miss Ratio",
            lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
            scale_y_func=IDENTITY_X,
            fix_y_func=shards_adj,
            set_ylim_to_one=True,
        )
        plot(
            axes[1],
            data,
            "Mean Cache Capacity [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "Mean Size [B]"]),
            *GiB_SHARDS_ARGS,
            "Miss Ratio",
            lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
            scale_y_func=IDENTITY_X,
            fix_y_func=shards_adj,
            set_ylim_to_one=True,
        )
        fig.savefig(args.mrc.resolve())
    if args.removal_stats is not None:
        fig, axes = plt.subplots(3, 3, sharex=True, sharey=True)
        fig.suptitle("Eviction + Expiration Statistics vs Cache Sizes")
        fig.set_size_inches(15, 15)
        plot(
            axes[0, 0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Evicted + Expired [GiB]",
            lambda d: (
                get_stat(d, ["CacheStatistics", "Total Evicts [B]"])
                + get_stat(d, ["CacheStatistics", "Total Expires [B]"])
            ),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[0, 1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Properly Evicted + Expired [GiB]",
            lambda d: (
                get_stat(d, ["CacheStatistics", "lru_evict", "[B]"])
                + get_stat(d, ["CacheStatistics", "ttl_expire", "[B]"])
            ),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[0, 2],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Improperly Evicted + Expired [GiB]",
            lambda d: (
                get_stat(d, ["CacheStatistics", "ttl_evict", "[B]"])
                + get_stat(d, ["CacheStatistics", "ttl_lazy_expire", "[B]"])
            ),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[1, 0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Evicted [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "Total Evicts [B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[2, 0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Expired [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "Total Expires [B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[1, 1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            f"{p} Evicted [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "lru_evict", "[B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[1, 2],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "TTL Evicted [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "ttl_evict", "[B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[2, 1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "TTL Proactice Expired [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "ttl_expire", "[B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[2, 2],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "TTL Lazy Expired [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "ttl_lazy_expire", "[B]"]),
            *GiB_SHARDS_ARGS,
        )
        fig.savefig(args.removal_stats.resolve())
    if args.eviction_time is not None:
        fig, axes = plt.subplots(2, 2, sharey="row", sharex=True)
        fig.suptitle("Eviction vs Expiration Time")
        fig.set_size_inches(10, 10)
        # Mean remaining lifespan.
        y_numer_keys = ["CacheStatistics", "lru_evict", "Pre-Expire Evicts [ms]"]
        y_numer_keys_1 = ["CacheStatistics", "ttl_evict", "Pre-Expire Evicts [ms]"]
        y_numer_keys_2 = ["CacheStatistics", "no_room_evict", "Pre-Expire Evicts [ms]"]
        y_numer_keys_3 = [
            "CacheStatistics",
            "ttl_lazy_expire",
            "Pre-Expire Evicts [ms]",
        ]
        y_denom_keys = ["CacheStatistics", "lru_evict", "Pre-Expire Evicts [#]"]
        y_denom_keys_1 = ["CacheStatistics", "ttl_evict", "Pre-Expire Evicts [#]"]
        y_denom_keys_2 = ["CacheStatistics", "no_room_evict", "Pre-Expire Evicts [#]"]
        y_denom_keys_3 = ["CacheStatistics", "ttl_lazy_expire", "Pre-Expire Evicts [#]"]
        plot(
            axes[0, 0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Mean Remaining Lifespan until Expiration [h]",
            lambda d: (
                div_or(
                    get_stat(d, y_numer_keys)
                    + get_stat(d, y_numer_keys_1)
                    + get_stat(d, y_numer_keys_2)
                    + get_stat(d, y_numer_keys_3),
                    get_stat(d, y_denom_keys)
                    + get_stat(d, y_denom_keys_1)
                    + get_stat(d, y_denom_keys_2)
                    + get_stat(d, y_denom_keys_3),
                )
            ),
            *HOURS_NO_SHARDS_ARGS,
        )

        # Mean stay past expiration.
        y_numer_keys = ["CacheStatistics", "lru_evict", "Post-Expire Evicts [ms]"]
        y_numer_keys_1 = ["CacheStatistics", "ttl_evict", "Post-Expire Evicts [ms]"]
        y_numer_keys_2 = ["CacheStatistics", "no_room_evict", "Post-Expire Evicts [ms]"]
        y_numer_keys_3 = [
            "CacheStatistics",
            "ttl_lazy_expire",
            "Post-Expire Evicts [ms]",
        ]
        y_denom_keys = ["CacheStatistics", "lru_evict", "Post-Expire Evicts [#]"]
        y_denom_keys_1 = ["CacheStatistics", "ttl_evict", "Post-Expire Evicts [#]"]
        y_denom_keys_2 = ["CacheStatistics", "no_room_evict", "Post-Expire Evicts [#]"]
        y_denom_keys_3 = [
            "CacheStatistics",
            "ttl_lazy_expire",
            "Post-Expire Evicts [#]",
        ]
        plot(
            axes[0, 1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Mean Stay Past Expiration [h]",
            lambda d: (
                div_or(
                    get_stat(d, y_numer_keys)
                    + get_stat(d, y_numer_keys_1)
                    + get_stat(d, y_numer_keys_2)
                    + get_stat(d, y_numer_keys_3),
                    get_stat(d, y_denom_keys)
                    + get_stat(d, y_denom_keys_1)
                    + get_stat(d, y_denom_keys_2)
                    + get_stat(d, y_denom_keys_3),
                )
            ),
            *HOURS_NO_SHARDS_ARGS,
        )

        # Number of objects evicted before their expiration.
        y_keys_0 = ["CacheStatistics", "lru_evict", "Pre-Expire Evicts [#]"]
        y_keys_1 = ["CacheStatistics", "ttl_evict", "Pre-Expire Evicts [#]"]
        y_keys_2 = ["CacheStatistics", "no_room_evict", "Pre-Expire Evicts [#]"]
        y_keys_3 = ["CacheStatistics", "ttl_lazy_expire", "Pre-Expire Evicts [#]"]
        plot(
            axes[1, 0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Number evicted BEFORE expiration",
            lambda d: get_stat(d, y_keys_0)
            + get_stat(d, y_keys_1)
            + get_stat(d, y_keys_2)
            + get_stat(d, y_keys_3),
            *COUNT_SHARDS_ARGS,
        )

        # Number of objects evicted after their expiration.
        y_keys_0 = ["CacheStatistics", "lru_evict", "Post-Expire Evicts [#]"]
        y_keys_1 = ["CacheStatistics", "ttl_evict", "Post-Expire Evicts [#]"]
        y_keys_2 = ["CacheStatistics", "no_room_evict", "Post-Expire Evicts [#]"]
        y_keys_3 = ["CacheStatistics", "ttl_lazy_expire", "Post-Expire Evicts [#]"]
        plot(
            axes[1, 1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Number evicted AFTER expiration",
            lambda d: get_stat(d, y_keys_0)
            + get_stat(d, y_keys_1)
            + get_stat(d, y_keys_2)
            + get_stat(d, y_keys_3),
            *COUNT_SHARDS_ARGS,
        )
        fig.savefig(args.eviction_time.resolve())
    if args.sizes is not None:
        fig, axes = plt.subplots(1, 2, sharex=True, sharey=True)
        fig.set_size_inches(10, 5)
        fig.suptitle("Cache Sizes")
        plot(
            axes[0],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Maximum Cache Size [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "Max Size [B]"]),
            *GiB_SHARDS_ARGS,
        )
        plot(
            axes[1],
            p,
            data,
            *CAPACITY_GIB_ARGS,
            "Mean Cache Size [GiB]",
            lambda d: get_stat(d, ["CacheStatistics", "Mean Size [B]"]),
            *GiB_SHARDS_ARGS,
        )
        fig.savefig(args.sizes.resolve())
    if args.remaining_lifetime is not None:
        fig, axes = plt.subplots(2, 2, sharex=True, sharey=True)
        fig.set_size_inches(10, 10)
        fig.suptitle("Remaining Lifetime in Cache vs Position in LRU Queue")
        label_func = (
            lambda d: f'{SCALE_SHARDS_FUNC(get_stat(d, ["Capacity [B]"]) / (1<<30), d):.3} GiB'
        )
        plot_lines(
            axes[0, 0],
            data,
            label_func,
            f"({p}-only) {p} Cache Position [GiB]",
            lambda d: (
                d["Extras"]["remaining_lifetime"]["Cache Sizes [B]"]
                if get_stat(d, ["Lower Ratio"]) == 0
                and get_stat(d, ["Upper Ratio"]) == 0
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
        )
        plot_lines(
            axes[0, 1],
            data,
            label_func,
            f"({p}-TTL) {p} Cache Position [GiB]",
            lambda d: (
                d["Extras"]["remaining_lifetime"]["Cache Sizes [B]"]
                if get_stat(d, ["Lower Ratio"]) == 0
                and get_stat(d, ["Upper Ratio"]) == 1
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
        )
        plot_lines(
            axes[1, 0],
            data,
            label_func,
            f"(TTL-only) {p} Cache Position [GiB]",
            lambda d: (
                d["Extras"]["remaining_lifetime"]["Cache Sizes [B]"]
                if get_stat(d, ["Upper Ratio"]) == 1
                and get_stat(d, ["Lower Ratio"]) == 1
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
        )
        plot_lines(
            axes[1, 1],
            data,
            label_func,
            f"(Predictive) {p} Cache Position [GiB]",
            lambda d: (
                d["Extras"]["remaining_lifetime"]["Cache Sizes [B]"]
                if get_stat(d, ["Upper Ratio"]) == 0.5
                and get_stat(d, ["Lower Ratio"]) == 0.5
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
        )
        fig.savefig(args.remaining_lifetime.resolve())
    if args.temporal_statistics is not None:
        used_data = {
            (l, u, mode): d_list
            for (l, u, mode), d_list in data.items()
            if plot_this_configuration(p, l, u, mode)
        }
        fig, axes = plt.subplots(
            6, len(used_data), sharex=True, sharey="row", squeeze=False
        )
        fig.set_size_inches(5 * len(used_data), 6 * 5)
        fig.suptitle("Temporal Statistics")
        label_func = (
            lambda d: f'{SCALE_SHARDS_FUNC(SCALE_B_TO_GiB(get_stat(d, ["Capacity [B]"])), d):.3} GiB'
        )
        for i, ((l, u, mode), d_list) in enumerate(used_data.items()):
            tmp_data = {(l, u, mode): d_list}
            plot_lines(
                axes[0, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                "Cache Size [GiB]",
                lambda d: d["CacheStatistics"]["Temporal Sizes [B]"],
                *GiB_SHARDS_ARGS,
            )
            plot_lines(
                axes[1, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                "High Watermark Cache Size [GiB]",
                lambda d: d["CacheStatistics"]["Temporal Max Sizes [B]"],
                *GiB_SHARDS_ARGS,
            )
            plot_lines(
                axes[2, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                "Objects [#]",
                lambda d: d["CacheStatistics"]["Temporal Resident Objects [#]"],
                *COUNT_SHARDS_ARGS,
            )
            plot_lines(
                axes[3, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["LRU-TTL Statistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                f"{p} Objects [#]",
                lambda d: d["LRU-TTL Statistics"]["Temporal LRU Sizes [#]"],
                *COUNT_SHARDS_ARGS,
            )
            plot_lines(
                axes[4, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["LRU-TTL Statistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                "TTL Objects [#]",
                lambda d: d["LRU-TTL Statistics"]["Temporal TTL Sizes [#]"],
                *COUNT_SHARDS_ARGS,
            )
            plot_lines(
                axes[5, i],
                tmp_data,
                label_func,
                "Time [h]",
                lambda d: d["LRU-TTL Statistics"]["Temporal Times [ms]"],
                SCALE_MS_TO_HOUR,
                IDENTITY_X_D,
                f"{p} + TTL Objects [#]",
                lambda d: np.array(d["LRU-TTL Statistics"]["Temporal LRU Sizes [#]"])
                + np.array(d["LRU-TTL Statistics"]["Temporal TTL Sizes [#]"]),
                *COUNT_SHARDS_ARGS,
            )
            # Overwrite titles
            axes[0, i].set_title(f"{get_label(p, l,u,mode)} Sizes vs. Time")
            axes[1, i].set_title(f"{get_label(p, l,u,mode)} High Watermark vs. Time")
            axes[2, i].set_title(f"{get_label(p, l,u,mode)} Objects vs. Time")
            axes[3, i].set_title(f"{get_label(p, l,u,mode)} {p} Objects vs. Time")
            axes[4, i].set_title(f"{get_label(p, l,u,mode)} TTL Objects vs. Time")
            axes[5, i].set_title(f"{get_label(p, l,u,mode)} {p} + TTL Objects vs. Time")
        fig.savefig(args.temporal_statistics.resolve())
    if args.policy == "lru" and args.eviction_histograms is not None:
        used_data = {
            (l, u, mode): d_list
            for (l, u, mode), d_list in data.items()
            if plot_this_configuration(p, l, u, mode)
        }
        fig, axes = plt.subplots(
            2, len(used_data), sharex=True, sharey=True, squeeze=False
        )
        fig.suptitle(f"{p} Eviction Time Histograms")
        fig.set_size_inches(5 * len(used_data), 2 * 5)
        for i, ((l, u, mode), d_list) in enumerate(used_data.items()):
            axes[0, i].set_title(
                f"{get_label(p, 0,1,mode)} Lifetime Histogram\nin {p}-TTL's {p} Queue"
            )
            axes[1, i].set_title(
                f"{get_label(p, l,u,mode)} Lifetime Histogram\nin {p}-TTL's {p} Queue"
            )
            for d in d_list:
                # LRU/Proactive-TTLs
                hist = {
                    float(k): float(v)
                    for k, v in d["Oracle"]["Lifetime Thresholds"]["Histogram"][
                        "histogram"
                    ].items()
                }
                hist = dict(sorted(hist.items()))
                axes[0, i].loglog(
                    [ADDITIVE_SMOOTHING(x) for x in hist.keys()],
                    hist.values(),
                    label=f"{SCALE_SHARDS_FUNC(SCALE_B_TO_GiB(get_stat(d, ["Capacity [B]"])), d):.3} GiB",
                )
                # Other eviction histogram
                hist = {
                    float(k): float(v)
                    for k, v in d["Lifetime Thresholds"]["Histogram"][
                        "histogram"
                    ].items()
                }
                hist = dict(sorted(hist.items()))
                axes[1, i].loglog(
                    [ADDITIVE_SMOOTHING(x) for x in hist.keys()],
                    hist.values(),
                    label=f"{SCALE_SHARDS_FUNC(SCALE_B_TO_GiB(get_stat(d, ["Capacity [B]"])), d):.3} GiB",
                )
            axes[0, i].legend()
            axes[1, i].legend()
        fig.savefig(args.eviction_histograms.resolve())
    if args.other is not None:
        y_keys = ["CacheStatistics", "lru_evict", "Post-Expire Evicts [#]"]
        y_label = "Mean Time from Eviction until Expiry [h]"
        # NOTE  I don't set a suptitle because there is only 1 plot.
        fig, ax = plt.subplots()
        plot(
            ax,
            p,
            data,
            *CAPACITY_GIB_ARGS,
            y_label,
            lambda d: (get_stat(d, y_keys) if get_stat(d, y_keys) else 0.0),
            scale_y_func=IDENTITY_X,
            fix_y_func=IDENTITY_X_D if True else SCALE_SHARDS_FUNC,
        )
        fig.savefig(args.other.resolve())

    # Check if no work was done.
    if {args.mrc, args.removal_stats, args.eviction_time, args.other} == {None}:
        from warnings import warn

        warn("no work done")


if __name__ == "__main__":
    main()
