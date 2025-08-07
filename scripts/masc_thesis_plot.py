#!usr/bin/bash
"""
## Colour Schemes
- Black = Oracle
- Grey = TTL-only
- Blue = LFU/LRU-only
- Red = Predictor

"""
import argparse
import json
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

from plot_predictive_cache import (
    plot_lines,
    plot,
    get_stat,
    MiB,
    GiB,
    parse_data,
    get_label,
    GiB_SHARDS_ARGS,
    SCALE_MS_TO_HOUR,
    CAPACITY_GIB_ARGS,
    HOURS_NO_SHARDS_ARGS,
    SCALE_B_TO_GiB,
    IDENTITY_X_D,
    IDENTITY_X,
    shards_adj,
    COLOUR_MAP,
)

################################################################################
### INTRODUCTION
################################################################################


def plot_remaining_lifetime_vs_lru_position():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster19.out"
    )
    OUTPUT = Path("cluster19-remaining-lifetime-vs-lru-position.pdf")

    data = parse_data(INPUT)
    capacities = [256 * MiB, 1.0 * GiB, 4.0 * GiB]
    fig, axes = plt.subplots(
        1, len(capacities), squeeze=False, sharex=True, sharey=True
    )
    fig.set_size_inches(len(capacities) * 5, 5)
    for i, c in enumerate(capacities):
        axes[0, i].axvline(
            SCALE_B_TO_GiB(c),
            color="black",
            linestyle=":",
            label=f"x={SCALE_B_TO_GiB(c):.3} GiB",
        )
        plot_lines(
            axes[0, i],
            data,
            lambda d: None,
            f"LRU Cache Position [GiB]",
            lambda d: (
                get_stat(d, ["Extras", "remaining_lifetime", "Cache Sizes [B]"])
                if get_stat(d, ["Lower Ratio"]) == 0.0
                and get_stat(d, ["Upper Ratio"]) == 0.0
                and get_stat(d, ["Extras", "Nominal Capacity [B]"]) == c
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
            fmt=".",
            colours=["red", "green", "blue"],
        )
        axes[0, i].legend(loc="upper right")
        axes[0, i].set_title(f"{SCALE_B_TO_GiB(c):.3} GiB LRU Cache")
    fig.savefig(OUTPUT)


def plot_accurate_vs_lazy_ttl_memory_usage():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster19.out"
    )
    OUTPUT = Path("cluster19-accurate-vs-lazy-ttl-memory-usage.pdf")

    data = parse_data(INPUT)
    capacities = [1.0 * GiB, 4.0 * GiB, 8.0 * GiB]
    expiration_policy = {
        (0.0, 0.0): "No expiration policy",
        (0.0, 1.0): "Proactive expiration policy",
    }
    # We use the maximum interval size because the methods' expiry cycle
    # is shorter than our sampling cycle.
    get_max_interval_size = lambda d: np.array(
        get_stat(d, ["Statistics", "Temporal Interval Max Sizes [B]"])
    )
    fig, axes = plt.subplots(
        1, len(capacities), squeeze=False, sharex=True, sharey=True
    )
    fig.set_size_inches(len(capacities) * 5, 5)
    for i, c in enumerate(capacities):
        axes[0, i].axhline(
            SCALE_B_TO_GiB(c),
            color="black",
            linestyle=":",
            label=f"Cache Capacity ({SCALE_B_TO_GiB(c):.3} GiB)",
        )
        for l, u in [(0.0, 1.0), (0.0, 0.0)]:
            plot_lines(
                axes[0, i],
                data,
                lambda d: expiration_policy[(l, u)],
                "Time [h]",
                lambda d: (
                    get_stat(d, ["Statistics", "Temporal Times [ms]"])
                    if get_stat(d, ["Lower Ratio"]) == l
                    and get_stat(d, ["Upper Ratio"]) == u
                    and get_stat(d, ["Extras", "Nominal Capacity [B]"]) == c
                    else None
                ),
                *HOURS_NO_SHARDS_ARGS,
                "Memory Usage [GiB]",
                get_max_interval_size,
                *GiB_SHARDS_ARGS,
                colours=[COLOUR_MAP[(l, u)]],
                plot_x_axis=False,
            )
        axes[0, i].legend(loc="upper right")
        axes[0, i].set_title(f"Memory Usage for {SCALE_B_TO_GiB(c):.3} GiB")
    fig.savefig(OUTPUT)


def plot_accurate_vs_lazy_ttl_mrc():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster19.out"
    )
    OUTPUT = Path("cluster19-accurate-vs-lazy-ttl-miss-ratio-curve.pdf")
    ratios = [(0.0, 0.0)]  # [(0.0, 0.0), (1.0, 1.0)]
    expiration_policy = {
        (0.0, 0.0): "No expiration policy",
        (0.0, 1.0): "Proactive expiration policy",
        (1.0, 1.0): "No eviction policy",
    }
    data = parse_data(INPUT)
    fig, axes = plt.subplots(1, len(ratios), squeeze=False, sharex=True, sharey=True)
    fig.set_size_inches(5 * len(ratios), 5)
    for i, (l, u) in enumerate(ratios):
        mydata = {
            # Accurate is first so it gets plotted at the back.
            (0.0, 1.0, "EvictionTime"): data[(0.0, 1.0, "EvictionTime")],
            (l, u, "EvictionTime"): data[(l, u, "EvictionTime")],
        }
        plot(
            axes[0, i],
            "LRU",
            mydata,
            *CAPACITY_GIB_ARGS,
            "Miss Ratio",
            lambda d: get_stat(d, ["Statistics", "Miss Ratio"]),
            scale_y_func=IDENTITY_X,
            fix_y_func=shards_adj,
            set_ylim_to_one=True,
            label_func=lambda p, l, u, e: expiration_policy[(l, u)],
        )
        axes[0, i].set_title(f"{get_label("LRU", l, u)} MRC")
        axes[0, i].legend(loc="upper right")
    fig.savefig(OUTPUT)


################################################################################
### LRU QUEUE ANALYSIS
################################################################################


def plot_remaining_ttl_vs_lru_position_for_times():
    times = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    times = [50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100]
    times = [75, 76, 77, 78, 79, 80]
    INPUTS = [
        Path(
            f"/home/david/projects/online_mrc/myresults/remaining-ttl-vs-lru/cluster19-4gib-{t}h-s0.001.json"
        )
        for t in times
    ]
    TITLES = [f"{t} hours" for t in times]
    OUTPUT = Path("cluster19-remaining-lifetime-vs-lru-position-for-times.pdf")

    fig, axes = plt.subplots(1, len(INPUTS), sharex=True, sharey=True, squeeze=False)
    fig.set_size_inches(len(INPUTS) * 5, 5)

    c = 4 * GiB
    for i, (INPUT, TITLE) in enumerate(zip(INPUTS, TITLES, strict=True)):
        with open(INPUT) as f:
            data = json.load(f)
            data["Lower Ratio"] = 0.0
            data["Upper Ratio"] = 1.0
            data["Lower Ratio"] = 0.0
            data["Extras"]["Nominal Capacity [B]"] = c
            data["Extras"]["SHARDS"] = {
                "type": "FixedRateShardsSampler",
                ".sampling_ratio": 0.001,
                ".threshold": 18446744073709552,
                ".scale": 1000,
                ".adjustment": True,
                ".num_entries_seen": 0,
                ".num_entries_processed": 0,
            }
            data = {(0.0, 1.0, "EvictionTime"): [data]}
        axes[0, i].axvline(
            SCALE_B_TO_GiB(c),
            color="black",
            linestyle=":",
            label=f"x={SCALE_B_TO_GiB(c):.3} GiB",
        )
        plot_lines(
            axes[0, i],
            data,
            lambda d: None,
            f"LRU Cache Position [GiB]",
            lambda d: (
                get_stat(d, ["Extras", "remaining_lifetime", "Cache Sizes [B]"])
                if get_stat(d, ["Lower Ratio"]) == 0.0
                and get_stat(d, ["Upper Ratio"]) == 1.0
                and get_stat(d, ["Extras", "Nominal Capacity [B]"]) == c
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
            fmt=".",
            colours=["red", "green", "blue"],
        )
        axes[0, i].legend(loc="upper right")
        axes[0, i].set_title(TITLE)
    fig.savefig(OUTPUT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input")
    args = parser.parse_args()

    plot_remaining_lifetime_vs_lru_position()
    plot_accurate_vs_lazy_ttl_memory_usage()
    plot_accurate_vs_lazy_ttl_mrc()
    plot_remaining_ttl_vs_lru_position_for_times()


if __name__ == "__main__":
    main()
