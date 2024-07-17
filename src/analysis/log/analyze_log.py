"""Analyze a log file for timing information."""

import argparse
import re
import os
from pathlib import Path

import matplotlib.pyplot as plt
from tqdm import tqdm


def get_file_tree(path: Path | list[Path]) -> list[Path]:
    """Return a list of all the files belonging to a directory."""
    if isinstance(path, list):
        print(path)
        return [file for dir_or_file in path for file in get_file_tree(dir_or_file)]
    if os.path.isfile(path):
        return [path]
    return [
        Path(os.path.join(dirpath, f)).absolute()
        for dirpath, dirnames, filenames in os.walk(path)
        for f in filenames
    ]


def get_trace_read_time_from_log(text: str, path: Path) -> float:
    pattern = re.compile(
        r"^\[INFO\] \[\d+-\d+-\d+ \d+:\d+:\d+\] \[ [-._/a-zA-Z]+:\d+ \] \[errno \d+: [^\]]*\] Trace Read Time: (\d+[.]\d+) sec$"
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        raise ValueError(f"log {path} has no trace time")
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple trace times")
    time = float(matching_lines[0].group(1))
    return time


def get_compute_time_from_log(text: str, path: Path) -> float:
    pattern = re.compile(
        r"^\[INFO\] \[\d+-\d+-\d+ \d+:\d+:\d+\] \[ [-._/a-zA-Z]+:\d+ \] \[errno \d+: [^\]]*\] Histogram Time: (\d+[.]\d+) [|] Post-Process Time: (\d+[.]\d+) [|] MRC Time: (\d+[.]\d+) [|] Total Time: (\d+[.]\d+)$"
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        # NOTE  Some experiments did not finish
        # TODO  Remove this short-circuit once Kia makes the disks
        #       read/write again and I can run the remaining Olken
        #       traces.
        return 0
        raise ValueError(f"log {path} has no compute time")
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple compute times")
    time = float(matching_lines[0].group(4))
    return time


def plot_runtime(inputs: list[Path], extensions: list[str], output: Path):
    times = {}
    for file in get_file_tree(inputs):
        if file.suffix not in extensions:
            continue
        with file.open() as f:
            text = f.read()
        ttime = get_trace_read_time_from_log(text, file)
        ctime = get_compute_time_from_log(text, file)
        print(f"Times for {str(file)}: Trace read: {ttime} -- Total compute: {ctime}")
        times[str(file)] = (ttime, ctime)
    times = {k: v for k, v in sorted(times.items())}

    emap_times = {k: v for k, v in times.items() if "Evicting-Map" in k}
    fss_times = {k: v for k, v in times.items() if "Fixed-Size-SHARDS" in k}
    fig, axs = plt.subplots()
    fig.suptitle("Runtimes")
    axs.plot([x[1] for x in emap_times.values()], label="Evicting Map")
    axs.plot([x[1] for x in fss_times.values()], label="Fixed-Size SHARDS")
    axs.legend()
    fig.savefig(output)


def get_accuracy_from_log(text: str, path: Path) -> tuple[float, float]:
    pattern = re.compile(
        r"^\[INFO\] \[\d+-\d+-\d+ \d+:\d+:\d+\] \[ [-._/a-zA-Z]+:\d+ \] \[errno \d+: [^\]]*\] MAE: (\d+[.]\d+) [|] MSE: (\d+[.]\d+)"
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        return 0, 0
        raise ValueError(f"log {path} has no compute time")
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple compute times")
    mae, mse = float(matching_lines[0].group(1)), float(matching_lines[0].group(2))
    return mae, mse


def plot_accuracy(inputs: list[Path], extensions: list[str], output: Path):
    accuracies = {}
    # print(get_file_tree(inputs))
    for file in tqdm(get_file_tree(inputs)):
        if file.suffix not in extensions:
            continue
        with file.open() as f:
            text = f.read()
        mae, mse = get_accuracy_from_log(text, file)
        print(f"Accuracies for {str(file)}: MAE: {mae} -- MSE: {mse}")
        accuracies[str(file)] = (mae, mse)
    accuracies = {k: v for k, v in sorted(accuracies.items())}

    emap_accuracies = {k: v for k, v in accuracies.items() if "Evicting-Map" in k}
    fss_accuracies = {k: v for k, v in accuracies.items() if "Fixed-Size-SHARDS" in k}
    fig, axs = plt.subplots()
    fig.suptitle("Accuracies")
    axs.plot([x[1] for x in emap_accuracies.values()], label="Evicting Map")
    axs.plot([x[1] for x in fss_accuracies.values()], label="Fixed-Size SHARDS")
    axs.legend()
    fig.savefig(output)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--inputs", "-i", nargs="+", type=Path, help="file/directory of input(s)"
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        type=str,
        default=".log",
        help="extensions to process (format: '--extensions .log .csv')",
    )
    parser.add_argument(
        "--output", "-o", type=Path, default=None, help="file for output"
    )
    parser.add_argument("--time", action="store_true", help="plot runtime")
    parser.add_argument("--accuracy", action="store_true", help="plot accuracy")
    args = parser.parse_args()

    if args.time:
        output = "time.pdf" if args.output is None else args.output
        plot_runtime(args.inputs, args.extensions.split(","), output)
    if args.accuracy:
        output = "accuracy.pdf" if args.output is None else args.output
        plot_accuracy(args.inputs, args.extensions.split(","), output)


if __name__ == "__main__":
    main()
