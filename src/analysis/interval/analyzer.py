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


def count_unique(array: np.ndarray):
    """Count the number of unique accesses in a chunk."""
    reuse_time = array[:]["reuse_time"]
    position = np.arange(len(array))
    is_first_access = reuse_time > position
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
    hist, edges = np.histogram(reuse_dist, bins=100)
    mrc = 1 - np.cumsum(hist / len(array))
    # NOTE  We set the first element of the MRC to 1.0 by definition.
    return edges, np.concatenate(([1.0], mrc))


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

        # Format axes
        axs[0, i].sharey(axs[0, 0])
        axs[1, i].sharey(axs[1, 0])
        axs[0, i].sharex(axs[1, i])

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
    parser.add_argument("--num-intervals", "-n", type=int, default=10)
    args = parser.parse_args()

    a = np.fromfile(args.input_file, dtype=DTYPE)
    b = divide_array(a, args.num_intervals)

    if args.head is not None:
        if args.head < 0:
            print(f"Using last {-args.head} of {len(b)}")
            b = b[args.head :]
        else:
            print(f"Using first {args.head} of {len(b)}")
            b = b[: args.head]
    else:
        print(f"Using all {len(b)}")

    num_accesses = [len(c) for c in b]
    num_unique = [count_unique(c) for c in b]
    print(f"Number of accesses: {num_accesses}")
    print(f"Number of unique accesses: {num_unique}")

    plot_all_hist_and_mrc(b, "hist-and-mrc.eps")


if __name__ == "__main__":
    main()
