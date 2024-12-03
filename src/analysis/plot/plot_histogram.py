#!/usr/bin/python3
"""Plot a histogram from my custom binary format."""

import argparse
import os
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


def plot_sparse_histogram(
    input_paths: list[Path], output_path: Path, log_x: bool, log_y: bool, cdf: bool
):
    header_dtype = np.dtype(
        [
            ("num_bins", np.uint64),
            ("bin_size", np.uint64),
            ("false_infinity", np.uint64),
            ("infinity", np.uint64),
            ("running_sum", np.uint64),
        ]
    )
    dtype = np.dtype([("index", np.uint64), ("frequency", np.uint64)])
    fig, axs = plt.subplots(ncols=len(input_paths), sharey=True, squeeze=False)
    fig.set_size_inches(3 * len(input_paths), 4)
    fig.suptitle(
        f"{'Histogram' if not cdf else 'CDF'} ({'log' if log_x else 'linear'}-{'log' if log_y else 'linear'} scale)"
    )
    fig.supxlabel("Index")
    fig.supylabel("Frequency")

    for i, input_path in enumerate(input_paths):
        header = np.fromfile(input_path, dtype=header_dtype, count=1)
        sparse_array = np.fromfile(
            input_path, dtype=dtype, offset=header_dtype.itemsize
        )
        index, frequency = sparse_array[:]["index"], sparse_array[:]["frequency"]
        axs[0][i].title.set_text(input_path.stem)
        if cdf:
            frequency = np.cumsum(frequency) / header[0]["running_sum"]
        if log_x and log_y:
            axs[0][i].loglog(index, frequency)
        elif log_x:
            axs[0][i].semilogx(index, frequency)
        elif log_y:
            axs[0][i].semilogy(index, frequency)
        else:
            axs[0][i].plot(index, frequency)
    # Do this after the plotting so we don't end up scaling it from [0, 1].
    for i, ax in enumerate(axs[0]):
        ax.set_xlim(xmin=0)
        ax.set_ylim(ymin=0)
    fig.savefig(output_path)

    # This is just to prevent annoying warnings about too many figures open.
    plt.close(fig)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-paths", "-i", nargs="+", type=Path, required=True)
    parser.add_argument("--output-path", "-o", type=Path, default="histogram.png")
    parser.add_argument(
        "--logarithmic-x",
        "--logx",
        action="store_true",
        help="plot X-axis logarithmically",
    )
    parser.add_argument(
        "--logarithmic-y",
        "--logy",
        action="store_true",
        help="plot Y-axis logarithmically",
    )
    parser.add_argument(
        "--cdf",
        action="store_true",
        help="plot histogram as CDF",
    )
    args = parser.parse_args()

    input_paths = []
    if not all(path.exists() for path in input_paths):
        raise FileNotFoundError(f"at least one '{input_paths}' DNE")

    plot_sparse_histogram(
        args.input_paths,
        args.output_path,
        args.logarithmic_x,
        args.logarithmic_y,
        args.cdf,
    )


if __name__ == "__main__":
    main()
