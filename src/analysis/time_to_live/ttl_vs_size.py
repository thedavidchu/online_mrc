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
import logging

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

logger = logging.getLogger(__name__)


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
    logger.info(f"Reading from {str(trace_path)} with {trace_format}'s format")
    data = np.memmap(trace_path, dtype=TRACE_DTYPES[trace_format], mode="readonly")
    if trace_format == "Sari":
        logger.debug(f"{data=}")
        # NOTE  The local traces differ from the format published in
        #       Sari's paper, "TTLs Matter". On the local traces, the
        #       'Eviction Time' column seems to be the regular TTL.
        ttl = data[::stride]["eviction_time"]  # - data[::stride]["timestamp"]
        size = data[::stride]["size"]
    if trace_format == "Kia":
        # We only want 'put' commands, which have the value 1 because
        # these are the commands that have an associated TTL.
        data = data[data[:]["command"] == 1]
        logger.debug(f"{data=}")
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
    if not histogram_path.exists():
        logger.info(f"Creating histogram from {str(trace_path)}")
        hist, ttl_edges, size_edges = create_ttl_vs_size_histogram(
            trace_path, trace_format, stride
        )
        np.savez(
            histogram_path, histogram=hist, ttl_edges=ttl_edges, size_edges=size_edges
        )
        logger.info(f"Saved histogram to {str(histogram_path)}")
    else:
        logger.info(f"Loading histogram from {str(histogram_path)}")
        npz = np.load(histogram_path)
        hist = npz["histogram"]
        ttl_edges = npz["ttl_edges"]
        size_edges = npz["size_edges"]
    logger.debug(f"{ttl_edges=}")
    logger.debug(f"{size_edges=}")

    plt.figure()
    plt.pcolormesh(size_edges, ttl_edges, hist, norm="log")
    plt.title("Object Time-to-Live (TTL) vs Size")
    plt.ylabel("Object Time-to-Live (TTL) [seconds]")
    plt.xlabel("Object Size [bytes]")
    plt.savefig(plot_path)
    logger.info(f"Saved plot to {str(plot_path)}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", nargs="+", type=Path, help="input trace path(s)"
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
    parser.add_argument("--plot", nargs="+", type=Path, help="output plot path(s)")
    parser.add_argument(
        "--histogram",
        nargs="+",
        type=Path,
        help="output histogram path(s). This should end in '.npz'.",
    )
    args = parser.parse_args()
    if not (len(args.input) == len(args.plot) == len(args.histogram)):
        raise ValueError(
            "we require the same number of input, plot, and histogram paths"
        )
    for input_, plot, histogram in zip(args.input, args.plot, args.histogram):
        if not input_.exists():
            logger.warning(f"{str(input_)} DNE")
            continue
        logger.info(f"Processing {str(input_), str(plot), str(histogram)}")
        plot_ttl_vs_size(input_, args.format, args.stride, plot, histogram)


if __name__ == "__main__":
    # NOTE  Default date format is: datefmt=r"%Y-%m-%d %H:%M:%S"
    #       according to https://docs.python.org/3/howto/logging.html#formatters.
    logging.basicConfig(
        format="[%(levelname)s] [%(asctime)s] [ %(pathname)s:%(lineno)d ] [errno 0: n/a] %(message)s",
        level=logging.INFO,
    )
    main()
