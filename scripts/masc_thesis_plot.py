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
    get_scaled_fixed_data,
    COUNT_SHARDS_ARGS,
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

MASC_DIR = Path("masc_thesis/")

# Clusters with lots of objects and some expirations.
OK_CLUSTERS = sorted(
    {6, 7, 11, 12, 15, 17, 18, 19, 22, 24, 25, 29, 31, 37, 44, 45, 50, 52}
    - {12, 15, 31, 37, 44, 50}
)
ALL_CLUSTERS = sorted(
    [6, 7, 11, 12, 15, 17, 18, 19, 22, 24, 25, 29, 31, 37, 44, 45, 50, 52]
)


################################################################################
### HELPERS
################################################################################


def prettify_number(x: float | int) -> str:
    """Prettify a number with slide-rule accuracy, according to Michael Collins."""
    mantissa = x
    exp10 = 0
    while mantissa > 1000:
        mantissa /= 1000
        exp10 += 3
    factors = {0: "", 3: "K", 6: "M", 9: "B", 12: "T", 15: "Quad.", 18: "Quin."}
    # Slide-rule accuracy says a number starting with '1' should be
    # accurate to 0.1%; otherwise, it should be accurate to 1%.
    if str(mantissa).startswith("1"):
        return f"{mantissa:.4g}{factors.get(exp10)}"
    else:
        return f"{mantissa:.3g}{factors.get(exp10)}"


################################################################################
### INTRODUCTION
################################################################################


def plot_ttl_vs_lru():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster19.out"
    )
    OUTPUT = MASC_DIR / Path("cluster19-ttl-vs-lru.pdf")

    data = parse_data(INPUT)
    capacities = [1.0 * GiB, 4.0 * GiB]
    fig, axes = plt.subplots(1, len(capacities), squeeze=False, sharey=True)
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
            "LRU Cache Position [GiB]",
            lambda d: (
                get_stat(d, ["Extras", "remaining_lifetime", "Cache Sizes [B]"])
                if get_stat(d, ["Lower Ratio"]) == 0.0
                and get_stat(d, ["Upper Ratio"]) == 0.0
                and get_stat(d, ["Extras", "Nominal Capacity [B]"]) == c
                else None
            ),
            *GiB_SHARDS_ARGS,
            "TTL [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
            fmt=".",
            colours=["red", "green", "blue"],
        )
        axes[0, i].legend().remove()
        axes[0, i].set_title("")
    # Drawings on the left plot
    axes[0, 0].plot([1.0, 1.0], [8.0, 5.0], "_-k", label="Predicted Eviction Time")
    fig.text(
        0.455,
        0.83,
        "Predicted\nEviction\nTime",
        ha="right",
        va="top",
    )
    if False:
        axes[0, 0].plot([0.0, 0.0], [8.15, 0.0], "_-k", label="Original TTL")
        fig.text(
            0.135,
            0.68,
            "Original TTL",
            ha="center",
            va="center",
            rotation="vertical",
        )
    # Zig-zag line
    axes[0, 0].plot([0.0, 0.67, 0.0, 1.0], [8.0, 6.0, 6.0, 3.0], "b-.")
    axes[0, 0].plot([0.67], [6.0], "b>")
    axes[0, 0].plot([0.01], [6.0], "b<")
    axes[0, 0].plot([0.99], [3.0], "b>")

    # Drawings on the right plot
    axes[0, 1].plot([4.0, 4.0], [8.0, -1.0], "_-k", label="Predicted Eviction Time")
    fig.text(
        0.88,
        0.73,
        "Predicted\nEviction\nTime",
        ha="right",
        va="top",
    )

    fig.savefig(OUTPUT)


def plot_accurate_vs_lazy_ttl_memory_usage():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster19.out"
    )
    OUTPUT = MASC_DIR / Path("cluster19-accurate-vs-lazy-ttl-memory-usage.pdf")

    data = parse_data(INPUT)
    capacities = [1.0 * GiB, 4.0 * GiB, 8.0 * GiB]
    expiration_policy = {
        (0.0, 0.0): "LRU/Lazy-TTL",
        (0.0, 1.0): "TTL-only",
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
    OUTPUT = MASC_DIR / Path("cluster19-accurate-vs-lazy-ttl-miss-ratio-curve.pdf")
    ratios = [(1.0, 1.0)]
    if False:
        ratios = [(0.0, 0.0)]
        ratios = [(0.0, 0.0), (1.0, 1.0)]
    expiration_policy = {
        (0.0, 0.0): "Lazy-TTL",
        (0.0, 1.0): "LRU/Proactive-TTL",
        (1.0, 1.0): "TTL-only",
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
        axes[0, i].set_title("")
        axes[0, i].legend(loc="upper right")
    fig.savefig(OUTPUT)


################################################################################
### LRU QUEUE ANALYSIS
################################################################################


def plot_ttl_vs_lru_changes():
    times = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10]
    times = [50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100]
    times = [75, 76, 77, 78, 79, 80]
    times = [76, 78, 80]
    INPUTS = [
        Path(
            f"/home/david/projects/online_mrc/myresults/remaining-ttl-vs-lru/cluster19-4gib-{t}h-s0.001.json"
        )
        for t in times
    ]
    TITLES = [f"{t} hours" for t in times]
    OUTPUT = MASC_DIR / Path("cluster19-ttl-vs-lru-changes.pdf")

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


################################################################################
### MRC
################################################################################


def plot_all_mrc():
    """Plot MRC for Proactive-TTL, Lazy-TTL, and Psyche"""
    INPUT = lambda policy, c: (
        f"/home/david/projects/online_mrc/myresults/{policy}-ttl-v0-s0.001/result-cluster{c}.out"
    )
    LRU_OUTPUT = MASC_DIR / Path("all-lru-mrc.pdf")
    LFU_OUTPUT = MASC_DIR / Path("all-lfu-mrc.pdf")

    expiration_policy = lambda policy: {
        (0.0, 0.0): (
            f"{policy.upper()}/Lazy-TTL"
            if policy == "lru"
            else "1st-order LFU/Lazy-TTL"
        ),
        (0.0, 1.0): (
            f"{policy.upper()}/Proactive-TTL"
            if policy == "lru"
            else "1st-order LFU/Proactive-TTL"
        ),
        (1.0, 1.0): "TTL-only",
        (0.5, 0.5): ("LRU/Psyche" if policy == "lru" else "1st-order LFU/Psyche"),
    }

    # Plot LRU MRCs
    def plot_me(policy, output_path):
        rows, cols = 4, 3
        fig, axes = plt.subplots(rows, cols, sharey=True, squeeze=False)
        fig.set_size_inches(6 * cols, 5 * rows)
        clusters = iter(OK_CLUSTERS)
        for r in range(rows):
            for c in range(cols):
                ax = axes[r, c]
                cluster = next(clusters)
                data = parse_data(Path(INPUT(policy, cluster)))
                if policy == "lfu":
                    # Plot oracle.
                    oracle_dlist = data[(0.0, 1.0, "EvictionTime")]
                    mrc = {}
                    get_mr = get_scaled_fixed_data(
                        lambda d: get_stat(d, ["Oracle", "Miss Ratio"]),
                        IDENTITY_X,
                        shards_adj,
                    )
                    get_c = get_scaled_fixed_data(
                        lambda d: get_stat(d, ["Oracle", "Capacity [B]"]),
                        *GiB_SHARDS_ARGS,
                    )
                    for d in oracle_dlist:
                        mrc[get_c(d)] = get_mr(d)
                    ax.plot(mrc.keys(), mrc.values(), "g-x", label="LFU/Proactive-TTL")
                plot(
                    ax,
                    policy,
                    data,
                    *CAPACITY_GIB_ARGS,
                    "Miss Ratio",
                    lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
                    scale_y_func=IDENTITY_X,
                    fix_y_func=shards_adj,
                    set_ylim_to_one=True,
                    label_func=lambda p, l, u, e: expiration_policy(p)[(l, u)],
                )
                ax.set_title(f"Cluster #{cluster}")
        fig.savefig(output_path, bbox_inches="tight")

    plot_me("lru", LRU_OUTPUT)
    plot_me("lfu", LFU_OUTPUT)


################################################################################
### Memory Usage
################################################################################


def plot_memory_usage(
    ax,
    all_data: list,
    colours: dict[str, str],
    cache_names: list[str],
    *,
    use_relative_size: bool = True,
):
    f = get_scaled_fixed_data(
        lambda d: d["Statistics"]["Temporal Interval Max Sizes [B]"],
        *GiB_SHARDS_ARGS,
    )
    t = get_scaled_fixed_data(
        lambda d: d["Statistics"]["Temporal Times [ms]"],
        *GiB_SHARDS_ARGS,
    )
    all_data_dict = {
        cache_name: data for (data, cache_name) in zip(all_data, cache_names)
    }
    for cache_name, data in all_data_dict.items():
        for d_list, oracle_dlist in zip(
            data.values(), all_data_dict["Psyche"].values()
        ):
            for d, oracle_d in zip(d_list, oracle_dlist):
                if cache_name == "Psyche":
                    continue
                c = colours[cache_name]
                oracle_sizes = np.array(f(oracle_d))
                oracle_times = np.array(t(oracle_d))
                output_sizes = np.array(f(d))
                output_times = np.array(t(d))
                assert np.all(oracle_times == output_times)
                times = oracle_times
                ratio_sizes = output_sizes / oracle_sizes
                ax.plot(
                    times,
                    (100 * ratio_sizes) if use_relative_size else output_sizes,
                    c=c,
                    label=cache_name,
                )
    ax.set_xlabel("Time [h]")
    ax.set_ylabel(
        "Memory Usage [% Psyche]" if use_relative_size else "Memory Usage [GiB]"
    )
    ax.legend()


def plot_all_periodic_ttl_memory_usage():
    """Plot compute usage bar graphs for Proactive-TTL (max cache size), Memcached, Redis, and CacheLib."""
    version = 2
    INPUTS = lambda c: [
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-Memcached-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-Redis-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-CacheLib-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-TTL-cluster{c}-v{version}-s0.001.out"
        ),
    ]
    NAMES = ["Memcached", "Redis", "CacheLib", "Psyche"]
    OUTPUT = MASC_DIR / Path("all-periodic-ttl-memory-usage.pdf")

    # Plot LRU MRCs
    rows, cols = 4, 3
    fig, axes = plt.subplots(rows, cols, squeeze=False)
    fig.set_size_inches(6 * cols, 5 * rows)
    clusters = iter(OK_CLUSTERS)
    COLOURS = dict(
        Optimal="black",
        CacheLib="#f47629",
        Memcached="#288d82",
        Redis="#ff4438",
        Psyche="#808080",
    )
    for r in range(rows):
        for c in range(cols):
            cluster = next(clusters)
            # We group by capacity simply to create a key. We don't actually need a key.
            all_data = [
                parse_data(
                    x,
                    key_funcs=(
                        lambda _d: 0.0,
                        lambda _d: 1.0,
                        lambda _d: "EvictionTime",
                    ),
                )
                for x in INPUTS(cluster)
            ]
            ax = axes[r, c]
            plot_memory_usage(ax, all_data, COLOURS, NAMES)
            ax.set_title(f"Cluster #{cluster}")
    fig.savefig(OUTPUT, bbox_inches="tight")


def plot_compare_memory_usage(plot_absolute: bool = False):
    """Plot comparison of memory for Proactive-TTL and Psyche."""
    INPUT = lambda policy, c: (
        f"/home/david/projects/online_mrc/myresults/{policy}-ttl-v0-s0.001/result-cluster{c}.out"
    )
    LRU_OUTPUT = MASC_DIR / Path("all-lru-compare-memory.pdf")
    LFU_OUTPUT = MASC_DIR / Path("all-lfu-compare-memory.pdf")

    def plot_me(policy, input_path, output_path):
        rows, cols = 4, 3
        fig, axes = plt.subplots(rows, cols, squeeze=False)
        fig.set_size_inches(6 * cols, 5 * rows)
        clusters = iter(OK_CLUSTERS)
        for r in range(rows):
            for c in range(cols):
                ax = axes[r, c]
                cluster = next(clusters)
                data = parse_data(Path(input_path(policy, cluster)))
                psyche_dlist = data[(0.5, 0.5, "EvictionTime")]
                accurate_dlist = data[(0.0, 1.0, "EvictionTime")]
                # Only plot these cache sizes.
                colours = {
                    "1.0 GiB": "red",
                    "4.0 GiB": "orange",
                    "8.0 GiB": "green",
                }
                for psyche_d, acc_d in zip(psyche_dlist, accurate_dlist):
                    times = psyche_d["CacheStatistics"]["Temporal Times [ms]"]
                    psyche_cnt = np.array(
                        psyche_d["CacheStatistics"]["Temporal Sizes [B]"]
                    )
                    acc_cnt = np.array(acc_d["CacheStatistics"]["Temporal Sizes [B]"])
                    shards_scale = acc_d["Extras"]["SHARDS"][".scale"]
                    sz = get_stat(acc_d, ["Capacity [B]"]) * shards_scale / (1 << 30)
                    csize = f"{sz:.3} GiB"
                    if csize in colours:
                        colour = colours[csize]
                    else:
                        continue
                    if plot_absolute:
                        ax.plot(
                            times,
                            shards_scale * acc_cnt / (1 << 30),
                            color=colour,
                            label=f"{policy.upper() if policy == 'lru' else '1st-order LFU'}/Proactive-TTL: {csize}",
                        )
                        ax.plot(
                            times,
                            shards_scale * psyche_cnt / (1 << 30),
                            "--",
                            color=colour,
                            label=f"Psyche: {csize}",
                        )
                        ax.axhline(sz, color=colour, linestyle="dotted")
                    else:
                        diff = (psyche_cnt - acc_cnt) * shards_scale / (1 << 20)
                        ratio = 100 * (psyche_cnt / acc_cnt - 1)
                        ax.plot(times, diff, color=colour, label=csize)
                ax.axhline(0, color="black", linestyle="dashed")
                ax.legend()
                ax.set_xlabel("Time [h]")
                if plot_absolute:
                    ax.set_ylabel("Memory [GiB]")
                else:
                    ax.set_ylabel("Extra Memory [MiB]")
                ax.set_title(f"Cluster #{cluster}")
        fig.savefig(output_path, bbox_inches="tight")

    plot_me("lru", INPUT, LRU_OUTPUT)
    plot_me("lfu", INPUT, LFU_OUTPUT)


def plot_all_total_memory_usage_comparison():
    """Plot metadata for Proactive-TTL and Psyche"""
    INPUT = lambda policy, c: (
        f"/home/david/projects/online_mrc/myresults/{policy}-ttl-v0-s0.001/result-cluster{c}.out"
    )
    LRU_OUTPUT = MASC_DIR / Path("all-lru-total-memory-savings.pdf")
    LFU_OUTPUT = MASC_DIR / Path("all-lfu-total-memory-savings.pdf")

    def plot_me(policy, input_path, output_path):
        rows, cols = 4, 3
        fig, axes = plt.subplots(rows, cols, squeeze=False)
        fig.set_size_inches(6 * cols, 5 * rows)
        clusters = iter(OK_CLUSTERS)
        for r in range(rows):
            for c in range(cols):
                ax = axes[r, c]
                cluster = next(clusters)
                data = parse_data(Path(input_path(policy, cluster)))
                psyche_dlist = data[(0.5, 0.5, "EvictionTime")]
                accurate_dlist = data[(0.0, 1.0, "EvictionTime")]
                # Only plot these cache sizes.
                colours = {
                    "1.0 GiB": "red",
                    "4.0 GiB": "orange",
                    "8.0 GiB": "green",
                }
                for psyche_d, acc_d in zip(psyche_dlist, accurate_dlist):
                    times = psyche_d["CacheStatistics"]["Temporal Times [ms]"]
                    shards_scale = acc_d["Extras"]["SHARDS"][".scale"]
                    sz = get_stat(acc_d, ["Capacity [B]"]) * shards_scale / (1 << 30)
                    csize = f"{sz:.3} GiB"
                    if csize in colours:
                        colour = colours[csize]
                    else:
                        continue
                    if policy == "lfu":
                        acc_stats = acc_d["Oracle"]["Statistics"]
                        psyche_stats = psyche_d["CacheStatistics"]
                    else:
                        acc_stats = acc_d["CacheStatistics"]
                        psyche_stats = psyche_d["CacheStatistics"]
                    psyche_cnt = np.array(psyche_stats["Temporal Resident Objects [#]"])
                    acc_cnt = np.array(acc_stats["Temporal Resident Objects [#]"])
                    psyche_size = np.array(psyche_stats["Temporal Sizes [B]"])
                    acc_size = np.array(acc_d["CacheStatistics"]["Temporal Sizes [B]"])
                    metadata_diff = (acc_cnt * 32 - psyche_cnt * 16) * shards_scale
                    size_diff = (acc_size - psyche_size) * shards_scale
                    diff = metadata_diff + size_diff
                    ax.plot(times, diff / (1 << 20), color=colour, label=csize)
                    ax.axhline(0, color="black", linestyle="dashed")
                ax.legend()
                ax.set_xlabel("Time [h]")
                ax.set_ylabel("Total Memory Savings [MiB]")
                ax.set_title(f"Cluster #{cluster}")
        fig.savefig(output_path, bbox_inches="tight")

    plot_me("lru", INPUT, LRU_OUTPUT)
    plot_me("lfu", INPUT, LFU_OUTPUT)


################################################################################
### Metadata Usage
################################################################################


def plot_all_metadata_usage():
    """Plot metadata for Proactive-TTL and Psyche"""
    INPUT = lambda policy, c: (
        f"/home/david/projects/online_mrc/myresults/{policy}-ttl-v0-s0.001/result-cluster{c}.out"
    )
    LRU_OUTPUT = MASC_DIR / Path("all-lru-metadata.pdf")
    LFU_OUTPUT = MASC_DIR / Path("all-lfu-metadata.pdf")

    def plot_me(policy, input_path, output_path):
        rows, cols = 4, 3
        fig, axes = plt.subplots(rows, cols, squeeze=False)
        fig.set_size_inches(6 * cols, 5 * rows)
        clusters = iter(OK_CLUSTERS)
        for r in range(rows):
            for c in range(cols):
                ax = axes[r, c]
                cluster = next(clusters)
                data = parse_data(Path(input_path(policy, cluster)))
                psyche_dlist = data[(0.5, 0.5, "EvictionTime")]
                accurate_dlist = data[(0.0, 1.0, "EvictionTime")]
                # Only plot these cache sizes.
                colours = {
                    "1.0 GiB": "red",
                    "4.0 GiB": "orange",
                    "8.0 GiB": "green",
                }
                for psyche_d, acc_d in zip(psyche_dlist, accurate_dlist):
                    times = psyche_d["CacheStatistics"]["Temporal Times [ms]"]
                    psyche_cnt = np.array(
                        psyche_d["CacheStatistics"]["Temporal Resident Objects [#]"]
                    )
                    acc_cnt = np.array(
                        acc_d["CacheStatistics"]["Temporal Resident Objects [#]"]
                    )
                    shards_scale = acc_d["Extras"]["SHARDS"][".scale"]
                    sz = get_stat(acc_d, ["Capacity [B]"]) * shards_scale / (1 << 30)
                    csize = f"{sz:.3} GiB"
                    if csize in colours:
                        colour = colours[csize]
                    else:
                        continue
                    diff = (acc_cnt * 32 - psyche_cnt * 16) * shards_scale
                    ax.plot(times, diff / (1 << 20), color=colour, label=csize)
                    ax.axhline(0, color="black", linestyle="dashed")
                ax.legend()
                ax.set_xlabel("Time [h]")
                ax.set_ylabel("Metadata Savings [MiB]")
                ax.set_title(f"Cluster #{cluster}")
        fig.savefig(output_path, bbox_inches="tight")

    plot_me("lru", INPUT, LRU_OUTPUT)
    plot_me("lfu", INPUT, LFU_OUTPUT)


################################################################################
### Compute Usage
################################################################################


def plot_cpu_usage(
    ax,
    all_data: list,
    colours: dict[str, str],
    cache_names: list[str],
    cluster: int,
):
    f = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Expiration Work [#]"]), *COUNT_SHARDS_ARGS
    )
    g = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Expirations [#]"]), *COUNT_SHARDS_ARGS
    )
    xs, exp_array, searched_array, cs = [], [], [], []
    for i, (data, cache_name) in enumerate(zip(all_data, cache_names)):
        for d_list in data.values():
            for d in d_list:
                c, nr_searched, nr_exp = colours[cache_name], f(d), g(d)
                xs.append(cache_name)
                exp_array.append(nr_exp)
                searched_array.append(nr_searched)
                cs.append(c)
    botbar = ax.bar(xs, exp_array, color=cs, log=True)
    if cluster != 44:
        ax.bar_label(
            botbar,
            labels=[prettify_number(y) for y in exp_array],
            label_type="center",
            padding=-10,
        )
    else:
        # Cluster 44 has zero expirations.
        ax.bar_label(
            botbar,
            labels=[prettify_number(y) for y in exp_array],
            label_type="edge",
            padding=200,
        )
    topbar = ax.bar(
        xs,
        # The height is the difference between the height we want and
        # the bottom.
        np.array(searched_array) - np.array(exp_array),
        # We make these 50% transparent.
        color=[f"{c}80" for c in cs],
        log=True,
        bottom=exp_array,
    )
    ax.bar_label(
        topbar,
        labels=[prettify_number(y) for y in searched_array],
        label_type="center",
        padding=-5,
    )
    ax.set_xlabel("Expiration Policy")
    ax.set_ylabel("Searched Objects")


def plot_all_periodic_ttl_compute_usage():
    """Plot compute usage bar graphs for Proactive-TTL (max cache size), Memcached, Redis, and CacheLib."""
    version = 2
    INPUTS = lambda c: [
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-Memcached-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-Redis-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-CacheLib-cluster{c}-v{version}-s0.001.out"
        ),
        Path(
            f"/home/david/projects/online_mrc/myresults/accurate/result-TTL-cluster{c}-v{version}-s0.001.out"
        ),
    ]
    NAMES = ["Memcached", "Redis", "CacheLib", "Psyche"]
    OUTPUT = MASC_DIR / Path("all-periodic-ttl-compute-usage.pdf")

    # Plot LRU MRCs
    rows, cols = 4, 3
    fig, axes = plt.subplots(rows, cols, sharey=True, squeeze=False)
    fig.set_size_inches(6 * cols, 5 * rows)
    clusters = iter(OK_CLUSTERS)
    COLOURS = dict(
        Optimal="black",
        CacheLib="#f47629",
        Memcached="#288d82",
        Redis="#ff4438",
        Psyche="#808080",
    )
    for r in range(rows):
        for c in range(cols):
            cluster = next(clusters)
            # We group by capacity simply to create a key. We don't actually need a key.
            all_data = [
                parse_data(
                    x,
                    key_funcs=(
                        lambda _d: 0.0,
                        lambda _d: 1.0,
                        lambda _d: "EvictionTime",
                    ),
                )
                for x in INPUTS(cluster)
            ]
            ax = axes[r, c]
            plot_cpu_usage(ax, all_data, COLOURS, NAMES, cluster)
            ax.set_title(f"Cluster #{cluster}")
            ax.set_ylim(bottom=1e6, top=20e12)
            ax.set_yticks([1e3, 1e6, 1e9, 1e12])
    fig.savefig(OUTPUT, bbox_inches="tight")


################################################################################
### LFU/TTL Frequency Analysis
################################################################################


def plot_lfu_ttl_frequency_analysis():
    INPUT = lambda f: Path(
        f"/home/david/projects/online_mrc/myresults/lfu_ttl_frequency_study/result-lfu-cluster52-v{f}-s0.001.out"
    )
    OUTPUT = MASC_DIR / Path("cluster52_frequency_study.pdf")
    FREQ = [1, 8, 64, 512]
    fig, axes = plt.subplots(1, len(FREQ), sharex=True, sharey=True, squeeze=False)
    for i, f in enumerate(FREQ):
        ipath = INPUT(f)
        all_data = parse_data(ipath)
        axes[0, i].plot()

    def nth(x: int):
        if str(x).endswith("1"):
            return f"{x}st"
        if str(x).endswith("1"):
            return f"{x}nd"
        if str(x).endswith("1"):
            return f"{x}rd"
        return f"{x}th"

    expiration_policy = lambda f: {
        (0.0, 0.0): f"{nth(f)}-order LFU/Lazy-TTL",
        (0.0, 1.0): f"{nth(f)}-order LFU/Proactive-TTL",
        (1.0, 1.0): "TTL-only",
        (0.5, 0.5): f"{nth(f)}-order LFU/Psyche",
    }

    rows, cols = 1, 4
    fig, axes = plt.subplots(rows, cols, sharey=True, squeeze=False)
    fig.set_size_inches(6 * cols, 5 * rows)
    freq = iter(FREQ)
    for r in range(rows):
        for c in range(cols):
            ax = axes[r, c]
            f = next(freq)
            data = parse_data(Path(INPUT(f)))
            # Plot oracle.
            oracle_dlist = data[(0.0, 1.0, "EvictionTime")]
            mrc = {}
            get_mr = get_scaled_fixed_data(
                lambda d: get_stat(d, ["Oracle", "Miss Ratio"]),
                IDENTITY_X,
                shards_adj,
            )
            get_c = get_scaled_fixed_data(
                lambda d: get_stat(d, ["Oracle", "Capacity [B]"]),
                *GiB_SHARDS_ARGS,
            )
            for d in oracle_dlist:
                mrc[get_c(d)] = get_mr(d)
            ax.plot(mrc.keys(), mrc.values(), "g-x", label="LFU/Proactive-TTL")
            plot(
                ax,
                "lfu",
                data,
                *CAPACITY_GIB_ARGS,
                "Miss Ratio",
                lambda d: get_stat(d, ["CacheStatistics", "Miss Ratio"]),
                scale_y_func=IDENTITY_X,
                fix_y_func=shards_adj,
                set_ylim_to_one=True,
                label_func=lambda p, l, u, e: expiration_policy(f)[(l, u)],
            )
            ax.set_title(f"Frequency {f}")
    fig.savefig(OUTPUT, bbox_inches="tight")


def plot_lfu_ttl_frequency_vs_shards(use_cdf: bool = False):
    INPUT_1000 = Path(
        f"/home/david/projects/online_mrc/myresults/lfu_ttl_frequency_study/result-lfu-cluster52-v1-s0.001.out"
    )
    INPUT_100 = Path(
        f"/home/david/projects/online_mrc/myresults/lfu_ttl_frequency_study/result-lfu-cluster52-v1-s0.01.out"
    )
    OUTPUT = MASC_DIR / Path("cluster52_frequency_vs_shards.pdf")
    object = parse_data(INPUT_1000)[(0.0, 1.0, "EvictionTime")][0]
    oracle_thresholds = object["Oracle"]["Lifetime Thresholds"]
    shards = object["Extras"]["SHARDS"][".scale"]
    cap_1000 = get_stat(object, ["Capacity [B]"]) * shards / (1 << 30)
    hist_1000 = {
        int(k): v["Histogram"]["total"] * shards for k, v in oracle_thresholds.items()
    }
    object = parse_data(INPUT_100)[(0.0, 1.0, "EvictionTime")][0]
    oracle_thresholds = object["Oracle"]["Lifetime Thresholds"]
    shards = object["Extras"]["SHARDS"][".scale"]
    cap_100 = get_stat(object, ["Capacity [B]"]) * shards / (1 << 30)
    hist_100 = {
        int(k): v["Histogram"]["total"] * shards for k, v in oracle_thresholds.items()
    }

    def cdf_percent(hist):
        mycdf = {}
        total = 0
        for f, nr_evict in hist.items():
            total += nr_evict
            mycdf[f] = total
        return {k: 100 * (1 - v / total) for k, v in mycdf.items()}

    fig, ax = plt.subplots()
    if use_cdf:
        cdf_1000 = cdf_percent(hist_1000)
        cdf_100 = cdf_percent(hist_100)
        ax.semilogy(
            cdf_1000.keys(), cdf_1000.values(), color="lightgrey", label="SHARDS: 0.001"
        )
        ax.semilogy(
            cdf_100.keys(), cdf_100.values(), color="darkgrey", label="SHARDS: 0.01"
        )
        ax.set_ylabel("Percent of Evictions with Larger Frequencies [%]")
    else:
        ax.semilogy(
            hist_1000.keys(),
            hist_1000.values(),
            color="lightgrey",
            label="SHARDS: 0.001",
        )
        ax.semilogy(
            hist_100.keys(), hist_100.values(), color="darkgrey", label="SHARDS: 0.01"
        )
        ax.set_ylabel("Eviction [#]")
    sz_1000, sz_100 = f"{cap_1000:.3} GiB", f"{cap_100:.3} GiB"
    assert sz_1000 == sz_100, f"{sz_1000} vs {sz_100}"
    fig.suptitle(f"Frequency Evictions for {sz_1000}")
    ax.set_xlabel("Frequency")
    ax.legend()
    fig.savefig(OUTPUT, bbox_inches="tight")


################################################################################
### ABLATION STUDY
################################################################################

# TODO:
# - Show decay works
# - Median vs mean lifetime
# - LRU vs TTL queue sizes over time
# -


def plot_cluster19_median_vs_mean():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v20250910-s0.001/result-cluster19.out"
    )
    OUTPUT = MASC_DIR / Path("cluster19_statistics.pdf")
    cap = "1.0 GiB"
    # Oracle or Psyche.
    oracle_dlist = parse_data(INPUT)[(0.0, 1.0, "EvictionTime")]
    dlist_0p = parse_data(INPUT)[(0.0, 0.0, "EvictionTime")]
    dlist_25p = parse_data(INPUT)[(0.25, 0.25, "EvictionTime")]
    dlist_50p = parse_data(INPUT)[(0.5, 0.5, "EvictionTime")]
    dlist_75p = parse_data(INPUT)[(0.75, 0.75, "EvictionTime")]
    dlist_100p = parse_data(INPUT)[(1.0, 1.0, "EvictionTime")]
    # Current histogram or overall histogram.
    ch = "Current Histogram "
    # ch = ""

    nrows, ncols = 1, 1
    fig, axes = plt.subplots(nrows, ncols, squeeze=False, sharex=True, sharey=True)
    fig.set_size_inches(6 * nrows, 6 * ncols)

    def plot_me(ax, dlist):
        d, *_ = [
            d
            for d in dlist
            if f"{get_scaled_fixed_data(
                lambda d: get_stat(d, ["Capacity [B]"]), *GiB_SHARDS_ARGS
            )(d):.3} GiB"
            == cap
        ]
        d_stats = d["Lifetime Thresholds"]
        d_t = SCALE_MS_TO_HOUR(np.array(d_stats["Temporal Times [ms]"]))
        d_min = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}Min Eviction Times [ms]"])
        )
        d_max = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}Max Eviction Times [ms]"])
        )
        d_25p = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}25th-percentile Eviction Times [ms]"])
        )
        d_75p = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}75th-percentile Eviction Times [ms]"])
        )
        d_med = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}Median Eviction Times [ms]"])
        )
        d_mean = SCALE_MS_TO_HOUR(
            np.array(d_stats[f"Temporal {ch}Mean Eviction Times [ms]"])
        )
        ax.plot(d_t, d_max, color="red", label="Max")
        ax.plot(d_t, d_75p, color="orange", label="75th-Percentile")
        ax.plot(d_t, d_mean, "--", color="black", label="Mean")
        ax.plot(d_t, d_med, color="green", label="Median")
        ax.plot(d_t, d_25p, color="purple", label="25th-Percentile")
        ax.plot(d_t, d_min, color="darkblue", label="Min")
        ax.legend(loc="best")
        ax.set_title(cap)
        ax.set_xlabel("Time [h]")
        ax.set_ylabel("Eviction Time [h]")
        ax.set_xlim(0.0)
        ax.set_ylim(0.0)

    plot_me(axes[0, 0], oracle_dlist)
    # plot_me(axes[0, 1], dlist_0p)
    # plot_me(axes[0, 2], dlist_25p)
    # plot_me(axes[0, 3], dlist_50p)
    # plot_me(axes[0, 4], dlist_75p)
    # plot_me(axes[0, 5], dlist_100p)
    fig.savefig(OUTPUT)


def plot_cluster19_decay():
    INPUT_NO_DECAY = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v20250911-s0.001/result-cluster19-no-decay.out"
    )
    INPUT_HOURLY_DECAY = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v20250911-s0.001/result-cluster19-50p-hourly.out"
    )
    INPUT_15MIN_DECAY = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v20250911-s0.001/result-cluster19-50p-15min.out"
    )
    INPUT_TOTAL_DECAY = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v20250911-s0.001/result-cluster19-100p-minutely.out"
    )
    OUTPUT = MASC_DIR / Path("cluster19_decay_study.pdf")

    ncols = 4
    fig, axes = plt.subplots(1, ncols, sharex=True, sharey=True, squeeze=False)
    fig.set_size_inches(6 * ncols, 6)

    def plot_me(ax, title, ipath):
        acc_d = parse_data(ipath)[(0.0, 1.0, "EvictionTime")][0]
        psyche_d = parse_data(ipath)[(0.5, 0.5, "EvictionTime")][0]
        times = psyche_d["Statistics"]["Temporal Times [ms]"]
        shards_scale = acc_d["Extras"]["SHARDS"][".scale"]
        acc_stats = acc_d["Statistics"]
        psyche_stats = psyche_d["Statistics"]
        psyche_size = (
            SCALE_B_TO_GiB(np.array(psyche_stats["Temporal Sizes [B]"])) * shards_scale
        )
        acc_size = (
            SCALE_B_TO_GiB(np.array(acc_stats["Temporal Sizes [B]"])) * shards_scale
        )
        ax.plot(times, psyche_size, color="red", label="LRU/Psyche")
        ax.plot(times, acc_size, color="black", label="LRU/Proactive-TTL")
        ax.axhline(SCALE_B_TO_GiB(4 << 30), color="black", linestyle="dashed")
        ax.legend()
        ax.set_xlabel("Time [h]")
        ax.set_ylabel("Memory Usage [GiB]")
        ax.set_title(title)

    plot_me(axes[0, 0], "No Decay", INPUT_NO_DECAY)
    plot_me(axes[0, 1], "50% Decay Hourly", INPUT_HOURLY_DECAY)
    plot_me(axes[0, 2], "50% Decay Every 15 Minutes", INPUT_15MIN_DECAY)
    plot_me(axes[0, 3], "100% Minutely Decay", INPUT_TOTAL_DECAY)

    fig.savefig(OUTPUT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input")
    args = parser.parse_args()

    MASC_DIR.mkdir(exist_ok=True)

    # Introduction
    plot_ttl_vs_lru()
    plot_accurate_vs_lazy_ttl_memory_usage()
    plot_accurate_vs_lazy_ttl_mrc()
    plot_ttl_vs_lru_changes()

    ## Evaluation
    plot_all_mrc()
    if False:
        plot_all_metadata_usage()
        plot_compare_memory_usage()
    plot_all_total_memory_usage_comparison()
    plot_all_periodic_ttl_memory_usage()
    plot_all_periodic_ttl_compute_usage()
    ## LFU Frequency Study
    plot_lfu_ttl_frequency_analysis()
    plot_lfu_ttl_frequency_vs_shards()
    ## Ablation
    plot_cluster19_median_vs_mean()
    plot_cluster19_decay()


if __name__ == "__main__":
    main()
