#!/usr/bin/python
"""
@brief  Plot TTL vs size.

We want to figure out a pattern in how the TTLs are set.

Sari Sultan's thesis, "Configuring In-Memory Caches", gives a CDF of the
TTLs in the Twitter traces in Figure 3.1. The CDFs with multiple
vertical sections are the interesting ones. Here's a list (not
guaranteed to be complete) of clusters with two or more large clusters
of TTLs:
- cluster4
- cluster5
- cluster6 
- cluster24
- cluster25
- cluster26
- cluster34
- cluster37
- cluster46
- cluster52
- cluster53
"""

import argparse
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt

TRACE_DTYPES: dict[str, np.dtype] = {
    "Kia": np.dtype(
        [
            ("timestamp", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size", np.uint32),
            ("ttl", np.uint32),
        ],
    ),
    # As per https://github.com/SariSultan/TTLsMatter-EuroSys24
    "Sari": np.dtype(
        [
            ("timestamp", np.uint32),
            ("key", np.uint64),
            ("size", np.uint32),
            ("eviction_time", np.uint32),
        ]
    ),
}


def create_ttl_vs_size_histogram(
    trace_path: Path,
    trace_format: str,
    stride: int,
):
    """
    Create a 50x50 histogram of TTL vs object size.
    @note   I create this function so that the 'data' array can be
            garbage collected ASAP. This is because I'm worried about
            running out of memory when processing the traces.
    """
    print(f"Reading from {str(trace_path)} with {trace_format}'s format")
    data = np.memmap(trace_path, dtype=TRACE_DTYPES[trace_format], mode="readonly")
    print(data[:10])
    if trace_format == "Sari":
        print(f"{data=}")
        # NOTE  The local traces differ from the format published in
        #       Sari's paper, "TTLs Matter". On the local traces, the
        #       'Eviction Time' column seems to be the regular TTL.
        ttl = data[::stride]["eviction_time"]  # - data[::stride]["timestamp"]
        size = data[::stride]["size"]
    if trace_format == "Kia":
        # We only want 'put' commands, which have the value 1 because
        # these are the commands that have an associated TTL.
        data = data[data[:]["command"] == 1]
        print(f"{data=}")
        ttl = data[::stride]["ttl"]
        size = data[::stride]["size"]
    hist, ttl_edges, size_edges = np.histogram2d(ttl, size, bins=50)
    return hist, ttl_edges, size_edges


def plot_ttl_vs_size(
    trace_path: Path,
    trace_format: str,
    stride: int,
    plot_path: Path,
    histogram_path: Path,
):
    hist, ttl_edges, size_edges = create_ttl_vs_size_histogram(
        trace_path, trace_format, stride
    )
    print(f"{ttl_edges=}")
    print(f"{size_edges=}")
    np.save(histogram_path, hist)

    print(f"Saved histogram to {str(histogram_path)}")
    plt.figure()
    plt.pcolormesh(size_edges, ttl_edges, hist)
    plt.title("Object Time-to-Live (TTL) vs Size")
    plt.ylabel("Object Time-to-Live (TTL) [seconds]")
    plt.xlabel("Object Size [bytes]")
    plt.savefig(plot_path)
    print(f"Saved plot to {str(plot_path)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", type=Path, required=True, help="input trace path"
    )
    parser.add_argument(
        "--format",
        "-f",
        type=str,
        choices=["Kia", "Sari"],
        default="Kia",
        help="input trace format",
    )
    parser.add_argument("--stride", type=int, default=1000, help="stride to sample")
    parser.add_argument("--plot", type=Path, required=True, help="output plot path")
    parser.add_argument(
        "--histogram", type=Path, required=True, help="output histogram path"
    )
    args = parser.parse_args()
    plot_ttl_vs_size(args.input, args.format, args.stride, args.plot, args.histogram)


if __name__ == "__main__":
    main()
