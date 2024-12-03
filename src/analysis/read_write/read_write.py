import argparse
import os
from pathlib import Path

import numpy as np
import matplotlib.pyplot as plt


NUM_BINS = 50
KIA_FORMAT = np.dtype(
    [
        ("timestamp", np.uint64),
        ("command", np.uint8),
        ("key", np.uint64),
        ("size", np.uint32),
        ("ttl", np.uint32),
    ],
)


def plot_rw_frequency(i: int, trace_path: Path, plot_path: Path):
    data = np.memmap(trace_path, dtype=KIA_FORMAT, mode="readonly")
    read_data = data[data[:]["command"] == 0]
    write_data = data[data[:]["command"] == 1]
    # Get total number of reads/writes for a given key
    u, cnt = np.unique(data[:]["key"], return_counts=True)
    rd_u, rd_cnt = np.unique(read_data[:]["key"], return_counts=True)
    wr_u, wr_cnt = np.unique(write_data[:]["key"], return_counts=True)
    print(
        f"{i}: '{trace_path}' | Unique R/W: {len(u)} | Unique R/O: {len(rd_u)} | Unique W/O: {len(wr_u)}"
    )

    fig, axs = plt.subplots(ncols=3, sharey=True)
    fig.set_size_inches(8, 4)
    fig.suptitle(f"Reads/Writes per key in Trace '{trace_path.stem}'")
    fig.supxlabel("# uses")
    fig.supylabel("Frequency")
    axs[0].title.set_text("Total Read/Write Frequency")
    axs[0].hist(cnt, bins=NUM_BINS, log=True)
    axs[1].title.set_text("Read Frequency")
    axs[1].hist(rd_cnt, bins=NUM_BINS, log=True)
    axs[2].title.set_text("Write Frequency")
    axs[2].hist(wr_cnt, bins=NUM_BINS, log=True)

    # NOTE  Set the limits after plotting so that they are rendered as
    #       expected.
    axs[0].set_ylim(ymin=0.5)  # Set the y-limit once, since it's shared.
    axs[0].set_xlim(xmin=0)
    axs[1].set_xlim(xmin=0)
    axs[2].set_xlim(xmin=0)
    fig.savefig(plot_path)
    # This closes the figure, since there is a sort-of maximum of 20
    # before you get a warning.
    plt.close(fig)


def run_plotter_over_dir(trace_dir: Path, plot_dir: Path):
    if not trace_dir.is_dir():
        raise FileNotFoundError(f"trace directory '{str(trace_dir)}' DNE")
    if not plot_dir.is_dir():
        raise FileNotFoundError(f"plot directory '{str(plot_dir)}' DNE")
    # Perform a single-level walk of the trace directory. Sort by file size.
    trace_paths = sorted(
        [child for child in trace_dir.iterdir() if child.is_file()],
        key=lambda p: p.stat().st_size,
    )
    plot_paths = [
        plot_dir / f"{trace_path.stem}-rw-hist.pdf" for trace_path in trace_paths
    ]

    for i, (trace_path, plot_path) in enumerate(zip(trace_paths, plot_paths)):
        plot_rw_frequency(i, trace_path, plot_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--trace-dir",
        "-t",
        type=Path,
        required=True,
        help="path to input directory with traces",
    )
    parser.add_argument(
        "--plot-dir",
        "-p",
        type=Path,
        required=True,
        help="path to output directory for plots",
    )
    args = parser.parse_args()
    run_plotter_over_dir(args.trace_dir, args.plot_dir)


if __name__ == "__main__":
    main()
