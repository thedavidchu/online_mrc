#!/usr/bin/python3
""" @brief  Plot the cardinalities."""

import argparse
import os

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


def plot_cardinalities(path: str):
    a = np.fromfile(path, dtype=DTYPE)
    fig, ax = plt.subplots()
    fig.set_dpi(600)
    fig.suptitle("True vs Estimated Cardinalities")
    ax.plot(a[:]["Olken"], label="True Cardinality")
    ax.plot(a[:]["HyperLogLog"], label="Estimated Cardinality (HLL)")
    ax.plot(a[:]["FS-SHARDS"], label="Estimated Cardinality (Fixed-Size SHARDS)")
    ax.plot(a[:]["FR-SHARDS"], label="Estimated Cardinality (Fixed-Rate SHARDS)")
    ax.legend()
    root, ext = os.path.splitext(path)
    fig.savefig(f"{root}.pdf")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", type=str, required=True, help="path to the input file"
    )
    args = parser.parse_args()

    plot_cardinalities(args.input)


if __name__ == "__main__":
    main()
