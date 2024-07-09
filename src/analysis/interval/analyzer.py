#!/usr/bin/python3
"""Analyze a stream of reuse distances and reuse times."""

import argparse
import os

import numpy as np
import matplotlib.pyplot as plt
from tqdm import tqdm

DTYPE = np.dtype([("reuse_dist", np.float64), ("reuse_time", np.float64)])


def divide_array(array: np.ndarray, subdivisions: int):
    """Divide an array into evenly sized chunks."""
    return [
        array[i * len(array) // subdivisions : (i + 1) * len(array) // subdivisions]
        for i in range(subdivisions)
    ]


def count_sampled(array: np.ndarray):
    """Count the number of sampled accesses in a chunk."""
    reuse_time = array[:]["reuse_time"]
    return np.count_nonzero(np.isnan(reuse_time) == False)


def count_infinities(array: np.ndarray):
    """Count the number of unique accesses in a chunk."""
    reuse_time = array[:]["reuse_time"]
    return np.count_nonzero(np.isinf(reuse_time))


def count_unique(array: np.ndarray):
    """Count the number of unique accesses in a chunk."""
    reuse_time = array[:]["reuse_time"]
    position = np.arange(len(array))
    is_first_access = np.logical_and(
        np.isnan(reuse_time) == False, reuse_time >= position
    )
    return np.count_nonzero(is_first_access)


def filter_finite(array: np.ndarray):
    # We assume that an infinite reuse distance occurs iff there is an
    # infinite reuse time as well.
    finite = np.isfinite(array[:]["reuse_dist"])
    return array[finite]


def convert_to_miss_rate_curve(array: np.ndarray):
    """Convert an array into a miss rate curve."""
    finite = filter_finite(array)
    num_inf = np.isinf(array[:]["reuse_dist"]).sum()
    reuse_dist = finite[:]["reuse_dist"]
    hist, cache_sizes = np.histogram(reuse_dist, bins=100)
    mrc = 1 - np.cumsum(hist / len(array))
    # NOTE  We set the first element of the MRC to 1.0 by definition.
    return cache_sizes, np.concatenate(([1.0], mrc))


def plot_single_miss_rate_curve(array: np.ndarray, output_path: str):
    edges, mrc = convert_to_miss_rate_curve(array)
    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.ylim(0, 1.01)
    plt.step(edges, mrc, where="post")
    plt.savefig(output_path)


def plot_single_histogram(array: np.ndarray, output_path: str):
    finite = filter_finite(array)
    reuse_dist = finite[:]["reuse_dist"]
    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Histogram")
    plt.xlabel("Reuse distance")
    plt.ylabel("Frequency")
    plt.hist(reuse_dist, bins=100)
    plt.savefig(output_path)


def get_max_wss(mrcs: list[tuple[np.ndarray, np.ndarray]]) -> int:
    cache_sizes = [cache_sizes[-1] for cache_sizes, _ in mrcs]
    max_wss = max(cache_sizes)
    return max_wss


def get_mr_at_c(cache_sizes: np.ndarray, miss_rates: np.ndarray, c: int) -> int:
    # NOTE  This is pretty hacky but here is my reasoning. The cache_sizes
    #       are in ascending order. We want the last cache_size that is
    #       smaller than c. This means this is the number of elements
    #       that are less than c.
    i = np.count_nonzero(cache_sizes < c) - 1
    if i < 0:
        i = 0
    return miss_rates[i]


def plot_iso_cache_size_miss_rate(
    arrays: list[np.ndarray], output_path: str, num_plots: int
):
    """@brief   Generate plots of the miss rate for a fixed cache size over time."""
    fig, axs = plt.subplots()
    # fig.set_size_inches(16, 12)
    fig.set_dpi(300)

    fig.suptitle("Miss Rate for Fixed Cache Sizes Through Time")
    mrcs = [convert_to_miss_rate_curve(array) for array in arrays]
    max_wss = get_max_wss(mrcs)
    for c in np.linspace(0, max_wss, num_plots):
        # print(c)
        mr_at_c = [
            get_mr_at_c(cache_sizes, miss_rates, c) for cache_sizes, miss_rates in mrcs
        ]
        axs.plot(mr_at_c, label=f"Cache size: {c}")
    axs.legend()

    # Save in many formats because I hate losing work!
    root, ext = os.path.splitext(output_path)
    fig.savefig(f"{root}.png", format="png")


def plot_all_hist_and_mrc(arrays: list[np.ndarray], output_path: str):
    fig, axs = plt.subplots(ncols=len(arrays), nrows=2)
    fig.set_size_inches(2 * axs.shape[1], 2 * axs.shape[0])
    fig.set_dpi(300)

    fig.suptitle("Histogram and MRC")
    for i, array in enumerate(tqdm(arrays)):
        finite = filter_finite(array)
        reuse_dist = finite[:]["reuse_dist"]
        edges, mrc = convert_to_miss_rate_curve(array)
        axs[0, i].set_title(f"{i}/{len(arrays)}")
        axs[0, i].hist(reuse_dist, bins=100)
        axs[1, i].step(edges, mrc, where="post")

        # Rotate x-ticks
        axs[0, i].tick_params(axis="x", labelrotation=10)
        axs[1, i].tick_params(axis="x", labelrotation=10)

        # Format axes
        axs[0, i].sharey(axs[0, 0])
        axs[0, i].sharex(axs[1, i])

        # Set range of MRC
        axs[1, i].set_ylim([0, 1])

    # Save in many formats because I hate losing work!
    root, ext = os.path.splitext(output_path)
    fig.savefig(f"{root}.eps", format="eps")
    fig.savefig(f"{root}.svg", format="svg")
    fig.savefig(f"{root}.pdf", format="pdf")
    fig.savefig(f"{root}.png", format="png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input-file",
        "-i",
        required=True,
        type=str,
        help="input file path (root is used for output path)",
    )
    parser.add_argument(
        "--head",
        type=int,
        default=None,
        help="use the first (last) <head> (-<tail>) entries only",
    )
    parser.add_argument(
        "--isocache-plot",
        type=int,
        default=0,
        help="The number of cache sizes to plot over time.",
    )
    parser.add_argument("--num-intervals", "-n", type=int, default=10)
    args = parser.parse_args()

    a = np.fromfile(args.input_file, dtype=DTYPE)
    b = divide_array(a, args.num_intervals)

    print(f"Processing {args.input_file} file")
    if args.head is not None:
        if args.head < 0:
            print(f"Using last {-args.head} of {len(b)} intervals")
            b = b[args.head :]
        else:
            print(f"Using first {args.head} of {len(b)} intervals")
            b = b[: args.head]
    else:
        print(f"Using all {len(b)} intervals")

    num_accesses = [len(c) for c in b]
    num_sampled = [count_sampled(c) for c in b]
    num_new = [count_infinities(c) for c in b]
    num_unique = [count_unique(c) for c in b]
    print(f"Number of total accesses: {num_accesses}")
    print(f"Number of sampled accesses: {num_sampled}")
    print(f"Number of new accesses: {num_new}")
    print(f"Number of unique elements: {num_unique}")

    plot_all_hist_and_mrc(b, "hist-and-mrc.eps")
    if args.isocache_plot > 0:
        plot_iso_cache_size_miss_rate(b, "iso-cache.png", args.isocache_plot)


if __name__ == "__main__":
    main()
