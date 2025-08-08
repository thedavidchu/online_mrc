#!/usr/bin/python3
"""
@brief  Plot the comparison between existing caches, specifically memory and CPU usages.
"""
import argparse
import json
from pathlib import Path
from typing import Callable

import matplotlib.pyplot as plt
from matplotlib.cbook import boxplot_stats
import numpy as np
from plot_predictive_cache import (
    parse_data,
    get_stat,
    get_scaled_fixed_data,
    plot_lines,
    parse_number,
    GiB_SHARDS_ARGS,
    COUNT_SHARDS_ARGS,
    NO_ADJUSTMENT_PERCENTAGE_ARGS,
    HOURS_NO_SHARDS_ARGS,
)


def prettify_number(x: float | int) -> str:
    """Prettify a number with slide-rule accuracy, according to Michael Collins."""
    mantissa = x
    exp10 = 0
    while mantissa > 1000:
        mantissa /= 1000
        exp10 += 3
    factors = {0: "", 3: "K", 6: "M", 9: "B", 12: "T"}
    # Slide-rule accuracy says a number starting with '1' should be
    # accurate to 0.1%; otherwise, it should be accurate to 1%.
    if str(mantissa).startswith("1"):
        return f"{mantissa:.4g}{factors.get(exp10)}"
    else:
        return f"{mantissa:.3g}{factors.get(exp10)}"


def get_line_statistics(
    data: dict,
    get_func: Callable,
    scale_func: Callable,
    fix_func: Callable,
    workload: str,
    cache_name: str,
) -> dict[str, float]:
    dict_intersect = lambda a, b: {k: v for k, v in a.items() if k in b}
    func = get_scaled_fixed_data(get_func, scale_func, fix_func)
    lines = [func(d) for k, dlist in data.items() for d in dlist]
    stats = boxplot_stats(lines)
    return [
        {**stat, "fliers": stat["fliers"].tolist(), "label": f"{workload}:{cache_name}"}
        for stat in stats
    ]
    return [
        {
            **dict_intersect(stat, {"mean", "q1", "med", "q3"}),
            "min": np.min(line),
            "max": np.max(line),
        }
        for stat, line in zip(stats, lines)
    ]


def plot_memory_usage(
    all_data: list,
    trace_name: str,
    shards: float,
    colours: dict[str, str],
    cache_names: list[str],
    opath: Path,
):
    fig, ax = plt.subplots()
    fig.suptitle(f"Memory Usage for {trace_name} (SHARDS: {shards})")

    # We use the maximum interval size because the methods' expiry cycle
    # is shorter than our sampling cycle.
    get_max_interval_size = lambda d: np.array(
        [parse_number(x) for x in d["Statistics"]["Temporal Interval Max Sizes [B]"]]
    )

    named_data = {cache_name: data for cache_name, data in zip(cache_names, all_data)}
    for i, (cache_name, data) in enumerate(named_data.items()):
        plot_lines(
            ax,
            data,
            lambda _d: cache_name,
            "Time [h]",
            lambda d: [parse_number(x) for x in d["Statistics"]["Temporal Times [ms]"]],
            *HOURS_NO_SHARDS_ARGS,
            "Memory Usage [GiB]",
            get_max_interval_size,
            *GiB_SHARDS_ARGS,
            colours=[colours.get(cache_name, "dimgrey")],
            plot_x_axis=False,
        )
    fig.savefig(opath)


def plot_relative_memory_usage(
    all_data: list,
    trace_name: str,
    shards: float,
    colours: dict[str, str],
    cache_names: list[str],
    opath: Path,
    *,
    absolute: bool,
):
    fig, ax = plt.subplots()
    fig.suptitle(f"Relative Memory Usage for {trace_name} (SHARDS: {shards})")
    named_data = {cache_name: data for cache_name, data in zip(cache_names, all_data)}

    def func(d):
        # We use the maximum interval size because the methods' expiry cycle
        # is shorter than our sampling cycle.
        get_max_interval_size = lambda d: np.array(
            get_stat(d, ["Statistics", "Temporal Interval Max Sizes [B]"])
        )
        optimal_d = named_data["Optimal"][(0.0, 1.0, "EvictionTime")][0]
        if absolute:
            r = get_max_interval_size(d) - get_max_interval_size(optimal_d)
            return r
        else:
            r = get_max_interval_size(d) / get_max_interval_size(optimal_d)
            return r

    a = []
    for cache_name, data in named_data.items():
        plot_lines(
            ax,
            data,
            lambda _d: cache_name,
            "Time [h]",
            lambda d: [parse_number(x) for x in d["Statistics"]["Temporal Times [ms]"]],
            *HOURS_NO_SHARDS_ARGS,
            (
                "Difference in Memory Usage [GiB]"
                if absolute
                else "Relative Memory Usage [%]"
            ),
            func,
            *(GiB_SHARDS_ARGS if absolute else NO_ADJUSTMENT_PERCENTAGE_ARGS),
            colours=[colours.get(cache_name, "dimgrey")],
            plot_x_axis=False,
        )
        if False:
            value = get_line_statistics(
                data,
                func,
                *(GiB_SHARDS_ARGS if absolute else NO_ADJUSTMENT_PERCENTAGE_ARGS),
                trace_name,
                cache_name,
            )
            a.extend(value)
    fig.savefig(opath)
    if False:
        fig, ax = plt.subplots()
        print(a)
        ax.bxp(a)
        fig.savefig(opath.parent / (opath.stem + "-boxplot" + opath.suffix))


def plot_cpu_usage(
    all_data: list,
    trace_name: str,
    shards: float,
    colours: dict[str, str],
    cache_names: list[str],
    opath: Path,
):
    fig, ax = plt.subplots()
    # fig.suptitle(f"# Objects Searched for {trace_name} (SHARDS: {shards})")
    f = get_scaled_fixed_data(
        lambda d: get_stat(d, ["Expiration Work [#]"]), *COUNT_SHARDS_ARGS
    )
    xs, ys, cs = [], [], []
    for i, (data, cache_name) in enumerate(zip(all_data, cache_names)):
        for d_list in data.values():
            for d in d_list:
                xs.append(cache_name)
                ys.append(f(d))
                cs.append(colours.get(cache_name, "dimgrey"))
    bar = ax.bar(xs, np.array(ys) / 1e9, color=cs)
    ax.bar_label(bar, labels=[prettify_number(y) for y in ys])
    ax.set_xlabel("Expiration Policy")
    ax.set_ylabel("Searched Objects [billions]")
    fig.savefig(opath)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--inputs", "-i", nargs="+", type=Path, required=True, help="input file"
    )
    parser.add_argument(
        "--cache-names", nargs="+", type=str, help="ordered names of the caches"
    )
    parser.add_argument("--trace-name", type=str, help="trace name to display")
    parser.add_argument("--shards-ratio", type=float, help="SHARDS ratio")
    parser.add_argument("--memory-usage", type=Path, help="memory usage output path")
    parser.add_argument(
        "--relative-memory-usage", type=Path, help="relative memory usage output path"
    )
    parser.add_argument("--cpu-usage", type=Path, help="memory usage output path")
    args = parser.parse_args()

    # We group by capacity simply to create a key. We don't actually need a key.
    all_data = [
        parse_data(
            x, key_funcs=(lambda _d: 0.0, lambda _d: 1.0, lambda _d: "EvictionTime")
        )
        for x in args.inputs
    ]
    # Optimal, CacheLib, Memcached, Redis colours (sampled directly from images).
    COLOURS = dict(
        Optimal="black", CacheLib="#f47629", Memcached="#288d82", Redis="#ff4438"
    )
    if args.memory_usage is not None:
        plot_memory_usage(
            all_data,
            args.trace_name,
            args.shards_ratio,
            COLOURS,
            args.cache_names,
            args.memory_usage.resolve(),
        )
    if args.relative_memory_usage is not None:
        plot_relative_memory_usage(
            all_data,
            args.trace_name,
            args.shards_ratio,
            COLOURS,
            args.cache_names,
            args.relative_memory_usage.resolve(),
            absolute=False,
        )
    if args.cpu_usage is not None:
        plot_cpu_usage(
            all_data,
            args.trace_name,
            args.shards_ratio,
            COLOURS,
            args.cache_names,
            args.cpu_usage.resolve(),
        )


if __name__ == "__main__":
    main()
