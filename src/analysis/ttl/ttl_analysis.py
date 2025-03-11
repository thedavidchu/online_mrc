#!/usr/bin/python
"""
@brief  Plot histograms of TTL, size, time. Specifically, you can plot
        these independently or correlated.

@details

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

@example    My imports break the normal Python stuff, so yeah.
            Run this file with the following:
            `python3 -m src.analysis.ttl.ttl_analysis [...args]`
"""

import argparse
import logging
import os
from pathlib import Path
from tempfile import TemporaryDirectory
from warnings import warn

import numpy as np
import matplotlib.pyplot as plt

from src.analysis.common.trace import (
    read_trace,
    TRACE_DTYPES,
)

logger = logging.getLogger(__name__)

ANALYSIS_DTYPE = np.dtype(
    [
        ("timestamp_ms", np.uint64),
        ("size_b", np.uint32),
        ("ttl_ms", np.uint32),
    ],
)


def get_valid_ttl_mask(data: np.memmap, tmpdir: Path, trace_format: str):
    assert trace_format in {"Kia", "Sari"}
    assert tmpdir.exists()

    mask = np.memmap(tmpdir / "mask.bin", dtype=bool, mode="w+", shape=data.shape)
    if trace_format == "Sari":
        # NOTE  We only want entries where the TTL is set. Otherwise,
        #       small TTLs are grouped with very small TTLs despite the
        #       large semantic difference.
        # NOTE  I'm not actually sure if Sari gives objects with no TTL
        #       a TTL of 0 too.
        mask[:] = data[:]["ttl_s"] != 0
        mask.flush()
    elif trace_format == "Kia":
        # NOTE  We only want 'put' commands, which have the value 1,
        #       since only 'puts' have a TTL.
        # NOTE  If Twitter's policy that all puts should have a TTL is
        #       true, then the set of all puts (for the Twitter traces)
        #       should have a non-zero TTL.
        puts = data[:]["command"] == 1
        # NOTE  We only want entries where the TTL is set. Otherwise,
        #       small TTLs are grouped with very small TTLs despite the
        #       large semantic difference.
        mortals = data[:]["ttl_s"] != 0
        # NOTE  The documentation says I should be able to write
        #       'puts & mortals', but this leads to the error:
        #       'ValueError: The truth value of an array with more than
        #       one element is ambiguous. Use a.any() or a.all()'.
        mask[:] = np.logical_and(puts, mortals)
        mask.flush()
    return mask, mask.sum()


def filter_valid_ttl(
    tmpdir: Path, file: str, data: np.memmap, format: str
) -> np.memmap:
    """Filter out objects without TTLs."""
    assert format in TRACE_DTYPES.keys()
    valid_ttl, cnt_valid_ttl = get_valid_ttl_mask(data, tmpdir, format)
    logger.debug(f"{cnt_valid_ttl=}, {valid_ttl=}")
    filtered_data = np.memmap(
        tmpdir / file,
        dtype=TRACE_DTYPES[format],
        mode="w+",
        shape=cnt_valid_ttl,
    )
    filtered_data[:cnt_valid_ttl] = data[valid_ttl]
    filtered_data.flush()
    # We re-open this file with a read-only permission!
    filtered_data = np.memmap(
        tmpdir / file, dtype=TRACE_DTYPES[format], mode="readonly"
    )
    return filtered_data


def shuffle_data(tmpdir: Path, file: str, data: np.memmap, format: str) -> np.memmap:
    """Return only the relevant fields."""
    assert format in TRACE_DTYPES.keys()
    shfl_data = np.memmap(
        tmpdir / file, dtype=ANALYSIS_DTYPE, mode="w+", shape=len(data)
    )
    match format:
        case "Sari":
            shfl_data[:]["timestamp_ms"] = 1000 * data[:]["timestamp_s"]
            shfl_data[:]["size_b"] = data[:]["size_b"]
            shfl_data[:]["ttl_ms"] = 1000 * data[:]["ttl_s"]
        case "Kia":
            shfl_data[:]["timestamp_ms"] = data[:]["timestamp_ms"]
            shfl_data[:]["size_b"] = data[:]["size_b"]
            shfl_data[:]["ttl_ms"] = 1000 * data[:]["ttl_s"]
        case _:
            raise ValueError(f"unrecognized format '{format}'")
    shfl_data.flush()
    shfl_data = np.memmap(tmpdir / file, dtype=ANALYSIS_DTYPE, mode="readonly")
    return shfl_data


def create_ttl_histogram(
    trace_path: Path,
    format: str,
    tmp_prefix: str | None,
):
    """
    Create a 50x50 histogram of TTL vs {object size,timestamp,etc}.
    @note   I create this function so that the 'data' array can be
            garbage collected ASAP. This is because I'm worried about
            running out of memory when processing the traces.
    """
    assert format in TRACE_DTYPES.keys()

    logger.info(f"Reading from {str(trace_path)} with {format}'s format")
    data = read_trace(trace_path, format, mode="readonly")
    logger.debug(f"Temporary prefix: {tmp_prefix}")
    with TemporaryDirectory(prefix=tmp_prefix) as t:
        tmpdir = Path(t)
        logger.info(f"Saving temporary files to {str(tmpdir)}")
        filtered_data = filter_valid_ttl(tmpdir, "filtered_data.bin", data, format)
        # NOTE  Ideally, I wouldn't create these extra memory mapped files.
        #       I would prefer to simply create light-weight 'views' into
        #       the masked_data, but simple experiments comparing the base
        #       of the masked_data to the {ttl,size,timestamp} tells me that
        #       we end up copying to memory.
        shuffled_data = shuffle_data(tmpdir, "shuffled_data.bin", filtered_data, format)
        hist, (ttl_edges, size_edges, timestamp_edges) = np.histogramdd(
            [
                shuffled_data[:]["ttl_ms"],
                shuffled_data[:]["size_b"],
                shuffled_data[:]["timestamp_ms"],
            ],
            bins=50,
        )
    return hist, ttl_edges, size_edges, timestamp_edges


def load_or_create_ttl_histogram(
    trace_path: Path, trace_format: Path, tmp_prefix: str, histogram_path: Path
) -> tuple[np.memmap, np.memmap, np.memmap, np.memmap]:
    if histogram_path is None:
        logger.info(f"Creating histogram from {str(trace_path)}")
        hist, ttl_edges, size_edges, timestamp_edges = create_ttl_histogram(
            trace_path, trace_format, tmp_prefix
        )
        logger.info(f"Not caching histogram...")
    elif histogram_path.exists():
        logger.info(f"Loading cached histogram from {str(histogram_path)}")
        npz = np.load(histogram_path)
        hist = npz["histogram"]
        ttl_edges = npz["ttl_edges"]
        size_edges = npz["size_edges"]
        timestamp_edges = npz["time_edges"]
    else:
        logger.info(f"Creating histogram from {str(trace_path)}")
        hist, ttl_edges, size_edges, timestamp_edges = create_ttl_histogram(
            trace_path, trace_format, tmp_prefix
        )
        np.savez(
            histogram_path,
            histogram=hist,
            ttl_edges=ttl_edges,
            size_edges=size_edges,
            time_edges=timestamp_edges,
        )
        logger.info(f"Cached histogram to {str(histogram_path)}")
    logger.debug(f"{ttl_edges=}")
    logger.debug(f"{size_edges=}")
    logger.debug(f"{timestamp_edges=}")
    return hist, ttl_edges, size_edges, timestamp_edges


def plot_correlations(
    hist: np.memmap,
    ttl_edges: np.memmap,
    size_edges: np.memmap,
    timestamp_edges: np.memmap,
    plot_path: Path,
):
    fig, (ax0, ax1) = plt.subplots(1, 2)
    fig.set_size_inches(12, 6)
    fig.suptitle("Object Time-to-Live (TTL) vs Size and Timestamp")

    # Plot TTL vs Size
    ttl_vs_size = np.sum(hist, axis=2)
    heatmap_a = ax0.pcolormesh(
        size_edges / 1e6, ttl_edges / 1000 / 3600, ttl_vs_size, norm="log"
    )
    ax0.set_title("Object Time-to-Live (TTL) vs Size")
    ax0.set_ylabel("Object Time-to-Live (TTL) [h]")
    ax0.set_xlabel("Object Size [MB]")

    # Plot TTL vs Timestamp
    ttl_vs_timestamp = np.sum(hist, axis=1)
    heatmap_b = ax1.pcolormesh(
        timestamp_edges / 1000 / 3600 / 24,
        ttl_edges / 1000 / 3600,
        ttl_vs_timestamp,
        norm="log",
    )
    ax1.set_title("Object Time-to-Live (TTL) vs Timestamp")
    ax1.set_ylabel("Object Time-to-Live (TTL) [h]")
    ax1.set_xlabel("Timestamp [day]")

    # Set the y_lim after plotting so that it doesn't cause it to be capped at 1.
    # TODO Actively share y-axis between ax0 and ax1
    ax0.set_xlim(xmin=0)
    ax0.set_ylim(ymin=0)
    ax1.set_xlim(xmin=0)
    ax1.set_ylim(ymin=0)

    fig.colorbar(heatmap_a)
    fig.colorbar(heatmap_b)

    fig.savefig(plot_path)
    logger.info(f"Saved plot to {str(plot_path)}")


def plot_independent(
    hist: np.memmap,
    ttl_edges: np.memmap,
    size_edges: np.memmap,
    timestamp_edges: np.memmap,
    plot_path: Path,
):
    fig, (ax0, ax1, ax2) = plt.subplots(1, 3)
    fig.set_size_inches(18, 6)
    fig.suptitle("Object Time-to-Live (TTL), Size, and Timestamp Histograms")

    ttl_hist = np.sum(hist, axis=(1, 2))
    # Smooth the log-plot by converting zeros to ones.
    ax0.semilogy(ttl_edges[:-1] / 1000 / 3600, ttl_hist + 1)
    ax0.set_title("Object Time-to-Live (TTL) Histogram")
    ax0.set_xlabel("Object Time-to-Live (TTL) [h]")
    ax0.set_ylabel("Log-Frequency")

    size_hist = np.sum(hist, axis=(0, 2))
    # Smooth the log-plot by converting zeros to ones.
    ax1.semilogy(size_edges[:-1] / 1e6, size_hist + 1)
    ax1.set_title("Object Size Histogram")
    ax1.set_xlabel("Size [MB]")
    ax1.set_ylabel("Log-Frequency")

    time_hist = np.sum(hist, axis=(0, 1))
    # Smooth the log-plot by converting zeros to ones.
    ax2.semilogy(timestamp_edges[:-1] / 1000 / 3600 / 24, time_hist + 1)
    ax2.set_title("Object Timestamp Histogram")
    ax2.set_xlabel("Time [day]")
    ax2.set_ylabel("Log-Frequency")

    # Set the y_lim after plotting so that it doesn't cause it to be capped at 1.
    # TODO Actively share y-axis between ax0 and ax1
    ax0.set_xlim(xmin=0)
    ax0.set_ylim(ymin=0)
    ax1.set_xlim(xmin=0)
    ax1.set_ylim(ymin=0)
    ax2.set_xlim(xmin=0)
    ax2.set_ylim(ymin=0)

    fig.savefig(plot_path)
    logger.info(f"Saved plot to {str(plot_path)}")


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
    parser.add_argument(
        "--histogram",
        "-g",
        type=Path,
        default=None,
        help="output path to histogram cache. This should end in '.npz'. "
        "If the path exists, then we load the cached histogram. "
        "If the path DNE, then we save the histogram here. "
        "If the path is not provided, then we turn off caching.",
    )
    parser.add_argument(
        "--plot-correlated",
        type=Path,
        default=None,
        help="output path for plot of TTL correlated with size and timestamp.",
    )
    parser.add_argument(
        "--plot-independent",
        type=Path,
        default=None,
        help="output path for plot of TTL, size, and timestamp independently.",
    )
    # If your memory-mapped operations are failing for mysterious
    # reasons, I would encourage you to check your disk utilization:
    # 'df -h'. I found that my root directory is often full. This is
    # because '/tmp/' is full of old temporary files.
    parser.add_argument(
        "--tmp-subdirectory",
        "-t",
        type=Path,
        required=False,
        default=None,
        help=(
            "an existing subdirectory of /tmp. Therefore, the temporary "
            "directory will be at the path /tmp/<tmp-subdirectory>/... "
            "This is for easily deleting dead temporary files if I use "
            "too much disk. "
            "To be honest, I'm not sure how I intended this to be used, "
            "so please use with caution (or not at all). DEPRECATED!"
        ),
    )
    args = parser.parse_args()

    if not args.input.exists():
        raise FileNotFoundError(f"{str(args.input)} DNE")
    # Validate tmp_subdirectory. I'm not sure what I intended to do.
    if args.tmp_subdirectory is None:
        tmpdir = None
    elif args.tmp_subdirectory.is_dir():
        # NOTE  On UNIX-like OS, this is equivalent to appending '/'.
        tmpdir = f"{str(args.tmp_subdirectory)}" + os.sep
    else:
        raise FileNotFoundError(f"{args.tmp_subdirectory} DNE")

    # I am trying to be clever for concision, but this may inadvertently
    # lead to these NEVER evaluating to True. For example, if I got the
    # number of elements in the list wrong, then it would always result
    # in a False evaluation.
    if [args.histogram, args.plot_correlated, args.plot_independent] == [None] * 3:
        raise ValueError("no useful work!")
    elif [args.plot_correlated, args.plot_independent] == [None] * 2:
        warn("no output plot specified!")
    elif args.plot_correlated == args.plot_independent:
        # We know these paths are not None, since we checked that in an
        # above condition.
        raise ValueError(
            f"specified the same output plot path twice: {args.plot_correlated} vs {args.plot_independent}"
        )

    output = load_or_create_ttl_histogram(
        args.input, args.format, tmpdir, args.histogram
    )
    if args.plot_correlated:
        plot_correlations(*output, args.plot_correlated)
    if args.plot_independent:
        plot_independent(*output, args.plot_independent)


if __name__ == "__main__":
    # NOTE  Default date format is: datefmt=r"%Y-%m-%d %H:%M:%S"
    #       according to https://docs.python.org/3/howto/logging.html#formatters.
    #       However, there is some number (maybe milliseconds) tacked
    #       onto the end. That's why I explicitly specify it.
    logging.basicConfig(
        format="[%(levelname)s] [%(asctime)s] [ %(pathname)s:%(lineno)d ] [errno 0: Success] %(message)s",
        level=logging.DEBUG,
        datefmt=r"%Y-%m-%d %H:%M:%S",
    )
    main()
