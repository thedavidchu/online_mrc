#!/usr/bin/python3
""" @brief  Plot the cardinalities."""

import argparse
import os
from pathlib import Path

import matplotlib.pyplot as plt
import numpy as np

DTYPE = np.dtype(
    [
        ("Olken", np.uint64),
        ("HyperLogLog", np.uint64),
        ("FS-SHARDS", np.uint64),
        ("FR-SHARDS", np.uint64),
    ]
)


def plot_cardinalities(input_path: Path, output_path: Path | None):
    a = np.fromfile(input_path, dtype=DTYPE)
    fig, ax = plt.subplots()
    fig.set_dpi(600)
    fig.suptitle("True vs Estimated Cardinalities")
    ax.plot(a[:]["Olken"], label="True Cardinality")
    ax.plot(a[:]["HyperLogLog"], label="Estimated Cardinality (HLL)")
    ax.plot(a[:]["FS-SHARDS"], label="Estimated Cardinality (Fixed-Size SHARDS)")
    ax.plot(a[:]["FR-SHARDS"], label="Estimated Cardinality (Fixed-Rate SHARDS)")
    ax.legend()
    if output_path is None:
        root, ext = os.path.splitext(input_path)
        output_path = f"{root}.pdf"
    fig.savefig(output_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", type=Path, required=True, help="path to the input file"
    )
    parser.add_argument(
        "--output",
        "-o",
        type=Path,
        default=None,
        help="path to the output file. Default: '<input-stem>.pdf'.",
    )
    args = parser.parse_args()

    plot_cardinalities(args.input, args.output)


if __name__ == "__main__":
    main()
