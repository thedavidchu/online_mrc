#!/usr/bin/python3
"""Plot a histogram."""

import argparse
import os

import numpy as np


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--sparse-histogram-paths", nargs="*", type=str, default=[])
    parser.add_argument(
        "--head",
        type=int,
        default=None,
        help="save the first <head> lines (or last lines if it is negative)",
    )
    args = parser.parse_args()

    for sparse_hist_path in args.sparse_histogram_paths:
        dt = np.dtype([("index", np.uint64), ("frequency", np.uint64)])
        sparse_hist = np.fromfile(sparse_hist_path, dtype=dt)
        if args.head is not None:
            if args.head < 0:
                print(f"Saving last {-args.head} of {len(sparse_hist)}")
                sparse_hist = sparse_hist[args.head :]
            else:
                print(f"Saving first {args.head} of {len(sparse_hist)}")
                sparse_hist = sparse_hist[: args.head]
        else:
            print(f"Saving {len(sparse_hist)}")
        root, ext = os.path.splitext(sparse_hist_path)
        sparse_hist.tofile(f"{root}.txt", sep=",")


if __name__ == "__main__":
    main()
