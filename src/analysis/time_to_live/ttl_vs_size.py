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

@note   For robustness, if we are passed in multiple files, then we will
        spawn a recursive subprocess to deal with this file. The reason
        is that these processes seem to get killed in mysterious ways
        and I don't know why (obviously).
@note   Okay, I figured out what it was. It was the root directory
        running out of space.
"""

import argparse
import logging
from pathlib import Path
from tempfile import TemporaryDirectory

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

########################################################################
# START RECURSION STUFF
########################################################################
import os
from shlex import split, quote
from subprocess import CompletedProcess, run


def abspath(path: str | Path) -> Path:
    """Create an absolute path."""
    return Path(path).resolve()


def sh(cmd: str, **kwargs) -> CompletedProcess:
    """
    Automatically run nohup on every script. This is because I have
    a bad habit of killing my scripts on hangup.

    @note   I will echo the commands to terminal, so don't stick your
            password in commands, obviously!
    """
    my_cmd = f"nohup {cmd}"
    print(f"Running: '{my_cmd}'")
    r = run(split(my_cmd), capture_output=True, text=True, **kwargs)
    if r.returncode != 0:
        print(
            "=============================================================================="
        )
        print(f"Return code: {r.returncode}")
        print(
            "----------------------------------- stderr -----------------------------------"
        )
        print(r.stderr)
        print(
            "----------------------------------- stdout -----------------------------------"
        )
        print(r.stdout)
        print(
            "=============================================================================="
        )
    return r


def practice_sh(cmd: str, **kwargs) -> CompletedProcess:
    # Adding the '--help' flag should call the executable but return
    # very quickly!
    return sh(cmd=f"{cmd} --help", **kwargs)


def sort_files_by_size(
    files: list[tuple[Path, Path, Path]]
) -> list[tuple[Path, Path, Path]]:
    """
    Return a list of (input_path, plot_path, histogram_path) with the
    smallest input_path first.

    @note   Non-existing files have a size of -infinity.
    """
    return sorted(
        files, key=lambda f: os.path.getsize(f[0]) if f[0].exists() else float("-inf")
    )


########################################################################
# END RECURSION STUFF
########################################################################


def get_array_mask(data: np.memmap, tmpdir: Path, trace_format: str):
    assert trace_format in {"Kia", "Sari"}
    assert tmpdir.exists()

    mask = np.memmap(tmpdir / "mask.bin", dtype=bool, mode="w+", shape=data.shape)
    if trace_format == "Sari":
        # NOTE  We only want entries where the TTL is set. Otherwise,
        #       small TTLs are grouped with very small TTLs despite the
        #       large semantic difference.
        mask[:] = data[:]["eviction_time"] != 0
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
        mortals = data[:]["ttl"] != 0
        # NOTE  The documentation says I should be able to write
        #       'puts & mortals', but this leads to the error:
        #       'ValueError: The truth value of an array with more than
        #       one element is ambiguous. Use a.any() or a.all()'.
        mask[:] = np.logical_and(puts, mortals)
        mask.flush()
    return mask, mask.sum()


def create_ttl_histogram(
    trace_path: Path,
    trace_format: str,
    stride: int,
    tmp_prefix: str | None,
):
    """
    Create a 50x50 histogram of TTL vs {object size,timestamp,etc}.
    @note   I create this function so that the 'data' array can be
            garbage collected ASAP. This is because I'm worried about
            running out of memory when processing the traces.
    """
    # Error checks
    assert trace_format in {"Kia", "Sari"}
    dtype = TRACE_DTYPES[trace_format]

    # Read input data into mmap region
    logger.info(f"Reading from {str(trace_path)} with {trace_format}'s format")
    data = np.memmap(trace_path, dtype=dtype, mode="readonly")
    # Save the relevant entries into mmap region
    with TemporaryDirectory(prefix=tmp_prefix) as t:
        tmpdir = Path(t)
        logger.info(f"Saving temporary files to {str(tmpdir)}")
        mask, mask_sz = get_array_mask(data, tmpdir, trace_format)
        masked_data = np.memmap(
            tmpdir / "masked_data.bin", dtype=dtype, mode="w+", shape=mask_sz
        )
        masked_data[:mask_sz] = data[mask]
        masked_data.flush()
        logger.debug(f"{masked_data=}")
        # NOTE  Ideally, I wouldn't create these extra memory mapped files.
        #       I would prefer to simply create light-weight 'views' into
        #       the masked_data, but simple experiments comparing the base
        #       of the masked_data to the {ttl,size,timestamp} tells me that
        #       we end up copying to memory.
        # Save TTL into mmap region
        ttl = np.memmap(tmpdir / "ttl.bin", dtype=np.uint32, mode="w+", shape=mask_sz)
        if trace_format == "Sari":
            # NOTE  The local traces differ from the format published in
            #       Sari's paper, "TTLs Matter". On the local traces, the
            #       'Eviction Time' column seems to be the regular TTL.
            ttl[:] = masked_data[::stride][
                "eviction_time"
            ]  # - data[::stride]["timestamp"]
            ttl.flush()
        elif trace_format == "Kia":
            ttl[:] = masked_data[::stride]["ttl"]
            ttl.flush()
        # Save size and timestamp into mmap region
        size = np.memmap(tmpdir / "size.bin", dtype=np.uint32, mode="w+", shape=mask_sz)
        size[:] = masked_data[::stride]["size"]
        size.flush()
        timestamp = np.memmap(
            tmpdir / "timestamp.bin", dtype=np.uint64, mode="w+", shape=mask_sz
        )
        timestamp[:] = masked_data[::stride]["timestamp"]
        timestamp.flush()
        # Create the histogram
        hist, (ttl_edges, size_edges, timestamp_edges) = np.histogramdd(
            [ttl, size, timestamp], bins=50
        )
    return hist, ttl_edges, size_edges, timestamp_edges


def plot_ttl(
    trace_path: Path,
    trace_format: str,
    stride: int,
    plot_path: Path,
    histogram_path: Path,
    tmp_prefix: str | None,
):
    if not histogram_path.exists():
        logger.info(f"Creating histogram from {str(trace_path)}")
        hist, ttl_edges, size_edges, timestamp_edges = create_ttl_histogram(
            trace_path, trace_format, stride, tmp_prefix
        )
        np.savez(
            histogram_path,
            histogram=hist,
            ttl_edges=ttl_edges,
            size_edges=size_edges,
            time_edges=timestamp_edges,
        )
        logger.info(f"Saved histogram to {str(histogram_path)}")
    else:
        logger.info(f"Loading histogram from {str(histogram_path)}")
        npz = np.load(histogram_path)
        hist = npz["histogram"]
        ttl_edges = npz["ttl_edges"]
        size_edges = npz["size_edges"]
        timestamp_edges = npz["time_edges"]
    logger.debug(f"{ttl_edges=}")
    logger.debug(f"{size_edges=}")
    logger.debug(f"{timestamp_edges=}")

    fig, (ax0, ax1) = plt.subplots(1, 2)
    fig.set_size_inches(12, 6)
    fig.suptitle("Object Time-to-Live (TTL) vs Size and Timestamp")

    # Plot TTL vs Size
    ttl_vs_size = np.sum(hist, axis=2)
    ax0.pcolormesh(size_edges, ttl_edges, ttl_vs_size, norm="log")
    ax0.set_title("Object Time-to-Live (TTL) vs Size")
    ax0.set_ylabel("Object Time-to-Live (TTL) [seconds]")
    ax0.set_xlabel("Object Size [bytes]")

    # Plot TTL vs Timestamp
    ttl_vs_timestamp = np.sum(hist, axis=1)
    ax1.pcolormesh(timestamp_edges, ttl_edges, ttl_vs_timestamp, norm="log")
    ax1.set_title("Object Time-to-Live (TTL) vs Timestamp")
    ax1.set_ylabel("Object Time-to-Live (TTL) [seconds]")
    ax1.set_xlabel("Timestamp [seconds]")

    fig.savefig(plot_path)
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
    # If your memory-mapped operations are failing for mysterious
    # reasons, I would encourage you to check your disk utilization:
    # 'df -h'. I found that my root directory is often full. This is
    # because '/tmp/' is full of old temporary files.
    parser.add_argument(
        "--tmp-dir",
        type=Path,
        required=False,
        default=None,
        help=(
            "temporary directory. Use this if your home directory "
            "(like mine) is too small to fit the traces."
        ),
    )
    args = parser.parse_args()
    if not (len(args.input) == len(args.plot) == len(args.histogram)):
        raise ValueError(
            "we require the same number of input, plot, and histogram paths"
        )
    files = [tuple(z) for z in zip(args.input, args.plot, args.histogram)]
    files = sort_files_by_size(files)

    if args.tmp_dir is None:
        tmpdir = None
    elif args.tmp_dir.exists():
        # NOTE  It would be more OS agnostic to use 'os.sep' instead of
        #       '/', but I think this is clearer for the OS that I use.
        tmpdir = f"{str(args.tmp_dir)}/"
    else:
        raise FileNotFoundError(f"{args.tmp_dir} DNE")

    if len(files) == 1:
        (input_, plot, histogram), *_ = files
        assert len(_) == 0
        if not input_.exists():
            logger.warning(f"{str(input_)} DNE")
        else:
            logger.info(f"Processing {str(input_), str(plot), str(histogram)}")
            plot_ttl(
                input_,
                args.format,
                args.stride,
                plot,
                histogram,
                tmp_prefix=tmpdir,
            )
        return
    for input_, plot, histogram in files:
        sh(
            f"python3 {abspath(__file__)} "
            f"--input {abspath(input_)} "
            f"--format {args.format} "
            f"--stride {args.stride} "
            f"--plot {abspath(plot)} "
            f"--histogram {abspath(histogram)}"
        )


if __name__ == "__main__":
    # NOTE  Default date format is: datefmt=r"%Y-%m-%d %H:%M:%S"
    #       according to https://docs.python.org/3/howto/logging.html#formatters.
    #       However, there is some number (maybe milliseconds) tacked
    #       onto the end. That's why I explicitly specify it.
    logging.basicConfig(
        format="[%(levelname)s] [%(asctime)s] [ %(pathname)s:%(lineno)d ] [errno 0: Success] %(message)s",
        level=logging.INFO,
        datefmt=r"%Y-%m-%d %H:%M:%S",
    )
    main()
