#!/usr/bin/python3
"""Plot a histogram."""

import argparse
import os

import numpy as np
import matplotlib.pyplot as plt


def plot_dense_histogram(input_path: str, output_path: str, plot_logarithmic: bool):
    array = np.fromfile(input_path, dtype=np.uint64)
    fig, ax = plt.subplots()
    fig.suptitle(f"{'Logarithmic ' if plot_logarithmic else ''}Histogram")
    fig.supxlabel("Index")
    fig.supylabel("Frequency")
    ax.hist(array, bins=100, log=plot_logarithmic)
    fig.savefig(output_path)


def plot_sparse_histogram(input_path: str, output_path: str, plot_logarithmic: bool):
    dtype = np.dtype([("index", np.uint64), ("frequency", np.uint64)])
    sparse_array = np.fromfile(input_path, dtype=dtype)
    # NOTE  For some odd reason, if we add a Python int to np.uint64,
    #       then we end up with a double. Who knew!
    max_index = int(np.max(sparse_array[:]["index"]))
    # NOTE  The extra +1 is because the indices start from zero,
    #       so the max_index will refer to the position max_index + 1.
    dense_array = np.zeros(shape=max_index + 1, dtype=np.uint64)
    dense_array[sparse_array[:]["index"]] = sparse_array[:]["frequency"]
    fig, ax = plt.subplots()
    fig.suptitle(f"{'Logarithmic ' if plot_logarithmic else ''}Histogram")
    fig.supxlabel("Index")
    fig.supylabel("Frequency")
    ax.hist(dense_array, bins=100, log=plot_logarithmic)
    fig.savefig(output_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-path", "-i", type=str, required=True)
    parser.add_argument("--output-path", "-o", type=str, default="histogram.png")
    parser.add_argument(
        "--logarithmic",
        "--log",
        action="store_true",
        help="plot Y-axis logarithmically",
    )
    parser.add_argument(
        "--dense",
        action="store_true",
        help="whether the input file is dense or sparsely formatted "
        "(dense: <frequency: uint64>, sparse: <index: uint64, frequency: uint64>)",
    )
    args = parser.parse_args()

    input_path = args.input_path
    output_path = args.output_path
    is_dense = args.dense
    plot_logarithmic = args.logarithmic

    if not os.path.exists(input_path):
        raise FileNotFoundError(f"{input_path} does not exist")

    if is_dense:
        plot_dense_histogram(input_path, output_path)
    else:
        plot_sparse_histogram(input_path, output_path, plot_logarithmic)


if __name__ == "__main__":
    main()
