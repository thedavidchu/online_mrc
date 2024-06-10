#!/usr/bin/python3
"""Analyze a stream of reuse distances and reuse times."""

import argparse

import numpy as np
import matplotlib.pyplot as plt
from tqdm import tqdm

UINT64_MAX: int = 18446744073709551615
DTYPE = np.dtype([("reuse_dist", np.uint64), ("reuse_time", np.uint64)])


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


def filter_infinity(array: np.ndarray):
    # We assume that an infinite reuse distance occurs iff there is an
    # infinite reuse time as well.
    finite = array[:]["reuse_dist"] != UINT64_MAX
    return array[finite]


def convert_to_miss_rate_curve(array: np.ndarray):
    """Convert an array into a miss rate curve."""
    finite = filter_infinity(array)
    num_inf = len(array) - len(finite)
    reuse_dist = finite[:]["reuse_dist"]
    hist, edges = np.histogram(reuse_dist, bins=100)
    mrc = 1 - np.cumsum(hist / len(array))
    # NOTE  We set the first element of the MRC to 1.0 by definition.
    return edges, np.concatenate(([1.0], mrc))


def plot_miss_rate_curve(array: np.ndarray, output_path: str):
    edges, mrc = convert_to_miss_rate_curve(array)
    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.ylim(0, 1.01)
    plt.step(edges, mrc, where="post")
    plt.savefig(output_path)


def plot_histogram(array: np.ndarray, output_path: str):
    finite = filter_infinity(array)
    reuse_dist = finite[:]["reuse_dist"]
    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Histogram")
    plt.xlabel("Reuse distance")
    plt.ylabel("Frequency")
    plt.hist(reuse_dist, bins=100)
    plt.savefig(output_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-file", required=True, type=str, help="input file path")
    parser.add_argument("--num-intervals", type=int, default=10)
    args = parser.parse_args()

    a = np.fromfile(args.input_file, dtype=DTYPE)
    print(a)
    b = divide_array(a, args.num_intervals)

    num_accesses = [len(c) for c in b]
    num_unique = [count_unique(c) for c in b]
    print(f"Number of accesses: {num_accesses}")
    print(f"Number of unique accesses: {num_unique}")

    for i, c in enumerate(tqdm(b)):
        plot_histogram(c, f"hist-{i}.png")
        plot_miss_rate_curve(c, f"mrc-{i}.png")


if __name__ == "__main__":
    main()
