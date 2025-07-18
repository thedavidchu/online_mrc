#!/usr/bin/python3

import argparse
from pathlib import Path
from functools import reduce

import numpy as np
import matplotlib.pyplot as plt

from plot_predictive_cache import (
    get_label,
    get_scaled_fixed_data,
    get_stat,
    parse_data,
    plot,
    plot_lines,
    shards_adj,
    CAPACITY_GIB_ARGS,
    IDENTITY_X,
    IDENTITY_X_D,
    SCALE_B_TO_GiB,
    SCALE_MS_TO_HOUR,
    SCALE_SHARDS_FUNC,
    GiB_SHARDS_ARGS,
    COUNT_SHARDS_ARGS,
)


def filter_max_cache_capacity_only(data: dict[tuple[float, float, str], list[object]]):
    """Return only the maximum common cache capacity."""
    capacity = lambda d: SCALE_SHARDS_FUNC(get_stat(d, ["Capacity [B]"]), d)
    cache_caps = [{capacity(d) for d in data_list} for data_list in data.values()]
    matching_cache_caps = reduce(lambda x, y: x & y, cache_caps)
    assert matching_cache_caps
    max_matching_cache_cap = max(matching_cache_caps)
    return max_matching_cache_cap, {
        k: [d for d in data_list if capacity(d) == max_matching_cache_cap]
        for k, data_list in data.items()
    }


def sorted_dict(x: dict[int | float, object], *, key=None) -> dict[int | float, object]:
    """Sort a dict by the keys."""
    return dict(sorted(x.items(), key=key))


def convert_dlist_to_dict_by_capacity(
    data: dict[tuple[float, float, str], list[dict[str, object]]],
) -> dict[tuple[float, float, str], dict[float, list[dict[str, object]]]]:
    capacity_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Capacity [B]"]), *GiB_SHARDS_ARGS
    )
    return sorted_dict({k: {capacity_func(d): d for d in v} for k, v in data.items()})


def plot_mrc(ax, data, p):
    # Plot the "oracle" for LFU because our other method is an approximate LFU.
    if p == "LFU" and (0.0, 1.0, "EvictionTime") in data:
        oracle_data_list = data[(0.0, 1.0, "EvictionTime")]
        # Plot oracle MRC
        cache_cap_func = get_scaled_fixed_data(
            lambda d: get_stat(d, ["Oracle", "Capacity [B]"]),
            *CAPACITY_GIB_ARGS[2:],
        )
        cache_cap = [cache_cap_func(d) for d in oracle_data_list]
        mrc_func = get_scaled_fixed_data(
            lambda d: get_stat(d, ["Oracle", "Miss Ratio"]),
            IDENTITY_X,
            shards_adj,
        )
        mrc = [mrc_func(d) for d in oracle_data_list]
        ax.plot(cache_cap, mrc, "bx-", label="Oracle")
    plot(
        ax,
        p,
        data,
        *CAPACITY_GIB_ARGS,
        "Miss Ratio",
        lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
        scale_y_func=IDENTITY_X,
        # TODO Adjust for shards
        fix_y_func=shards_adj,
        set_ylim_to_one=True,
    )


def plot_triple_metrics(args):
    """Plot MRC, max-cache-capacity size over time, and the max-cache-capacity's objects over time."""
    input_file = args.input.resolve()
    data = parse_data(input_file)
    p = args.policy.upper()
    fig, axes = plt.subplots(1, 3, squeeze=False)
    fig.set_size_inches(15, 5)
    # TODO Ideally we get this from the data, but that's more work.
    shards_info = "" if args.shards_ratio == 1.0 else f" (SHARDS: {args.shards_ratio})"
    fig.suptitle(f"Metrics for {args.name}{shards_info}")
    # Miss Ratio Curve (MRC)
    plot_mrc(axes[0, 0], data, p)
    # Cache Size Over Time
    used_data = {
        (l, u, mode): d_list
        for (l, u, mode), d_list in data.items()
        if (l, u, mode) in {(0.0, 1.0, "EvictionTime"), (0.5, 0.5, "EvictionTime")}
    }
    max_cap, used_data = filter_max_cache_capacity_only(used_data)
    label_func = (
        lambda d: f'{get_label(p, d["Lower Ratio"], d["Upper Ratio"])} ({SCALE_SHARDS_FUNC(SCALE_B_TO_GiB(get_stat(d, ["Capacity [B]"])), d):.3} GiB)'
    )
    plot_lines(
        axes[0, 1],
        used_data,
        label_func,
        "Time [h]",
        lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
        SCALE_MS_TO_HOUR,
        IDENTITY_X_D,
        "Cache Size [GiB]",
        lambda d: d["CacheStatistics"]["Temporal Sizes [B]"],
        *GiB_SHARDS_ARGS,
        colours=["purple", "black"],
    )
    plot_lines(
        axes[0, 2],
        used_data,
        label_func,
        "Time [h]",
        lambda d: d["LRU-TTL Statistics"]["Temporal Times [ms]"],
        SCALE_MS_TO_HOUR,
        IDENTITY_X_D,
        f"Metadata ({p} + TTL Queue) Objects [#]",
        lambda d: np.array(d["LRU-TTL Statistics"]["Temporal LRU Sizes [#]"])
        + np.array(d["LRU-TTL Statistics"]["Temporal TTL Sizes [#]"]),
        *COUNT_SHARDS_ARGS,
        colours=["purple", "black"],
    )
    # Overwrite titles
    axes[0, 0].set_title(f"Miss Ratio Curve")
    axes[0, 1].set_title(f"Cache Size over Time for {SCALE_B_TO_GiB(max_cap):.3} GiB")
    axes[0, 2].set_title(
        f"Metadata Objects over Time for {SCALE_B_TO_GiB(max_cap):.3} GiB"
    )
    # Metadata Objects Over Time
    fig.savefig(args.output.resolve())


def plot_sizes(args):
    input_file = args.input.resolve()
    data = parse_data(input_file)
    p = args.policy.upper()
    oracle_key = (0.0, 1.0, "EvictionTime")
    predict_key = (0.5, 0.5, "EvictionTime")
    assert len(data[oracle_key]) == len(data[predict_key])
    nr_sizes = len(data[oracle_key])

    fig, axes = plt.subplots(2, nr_sizes, sharey="row", sharex=True, squeeze=False)
    fig.set_size_inches(5 * nr_sizes, 10)
    # TODO Ideally we get this from the data, but that's more work.
    shards_info = "" if args.shards_ratio == 1.0 else f" (SHARDS: {args.shards_ratio})"
    fig.suptitle(f"Sizes for {args.name}{shards_info}")

    # Plot cache sizes over time.
    get_cap_func = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Capacity [B]"]), *GiB_SHARDS_ARGS
    )
    data_by_cap = convert_dlist_to_dict_by_capacity(data)
    oracle_data_by_cap = data_by_cap[oracle_key]
    predict_data_by_cap = data_by_cap[predict_key]
    label_func = (
        lambda d: f'{get_label(p, d["Lower Ratio"], d["Upper Ratio"])} ({SCALE_SHARDS_FUNC(SCALE_B_TO_GiB(get_stat(d, ["Capacity [B]"])), d):.3} GiB)'
    )
    for ax0, ax1, (oracle_cap, oracle_d), (predict_cap, predict_d) in zip(
        axes[0], axes[1], oracle_data_by_cap.items(), predict_data_by_cap.items()
    ):
        used_data = {oracle_key: [oracle_d], predict_key: [predict_d]}
        plot_lines(
            ax0,
            used_data,
            label_func,
            "Time [h]",
            lambda d: d["CacheStatistics"]["Temporal Times [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
            "Cache Size [GiB]",
            lambda d: d["CacheStatistics"]["Temporal Sizes [B]"],
            *GiB_SHARDS_ARGS,
            colours=["purple", "black"],
        )
        ax0.set_title(f"{oracle_cap:.3} [GiB]")
        # Plot cache metadata usage.
        plot_lines(
            ax1,
            used_data,
            label_func,
            "Time [h]",
            lambda d: d["LRU-TTL Statistics"]["Temporal Times [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
            f"Metadata ({p} + TTL Queue) Objects [#]",
            lambda d: np.array(d["LRU-TTL Statistics"]["Temporal LRU Sizes [#]"])
            + np.array(d["LRU-TTL Statistics"]["Temporal TTL Sizes [#]"]),
            *COUNT_SHARDS_ARGS,
            colours=["purple", "black"],
        )
        ax1.set_title(f"{oracle_cap:.3} [GiB]")
    fig.savefig(args.sizes_output.resolve())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, required=True, help="input file")
    parser.add_argument(
        "--policy", "-p", choices=["lru", "lfu"], required=True, help="eviction policy"
    )
    parser.add_argument("--name", "-n", required=True, help="name of workload")
    parser.add_argument(
        "--shards-ratio", type=float, default=1.0, help="SHARDS sampling ratio"
    )
    parser.add_argument("--output", "-o", type=Path, help="main metrics plot")
    parser.add_argument("--sizes-output", type=Path, help="sizes plot")
    args = parser.parse_args()

    plot_triple_metrics(args)
    plot_sizes(args)


if __name__ == "__main__":
    main()
