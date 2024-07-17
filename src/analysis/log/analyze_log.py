"""Analyze a log file for timing information."""

import argparse
import re
import os
from difflib import SequenceMatcher
from pathlib import Path
from warnings import warn

import matplotlib.pyplot as plt
from tqdm import tqdm


def get_file_tree(path: Path | list[Path]) -> list[Path]:
    """Return a list of all the files belonging to a directory."""
    if isinstance(path, list):
        return [file for dir_or_file in path for file in get_file_tree(dir_or_file)]
    if os.path.isfile(path):
        return [path]
    return [
        Path(os.path.join(dirpath, f)).absolute()
        for dirpath, dirnames, filenames in os.walk(path)
        for f in filenames
    ]


def get_file_stems(path: list[str]) -> list[str]:
    return [os.path.splitext(os.path.split(p)[-1])[0] for p in path]


def get_longest_substring(a: str, b: str) -> str:
    """
    I originally copied this code from
    https://www.geeksforgeeks.org/sequencematcher-in-python-for-longest-common-substring/.
    Of course, they had a syntax error, because what would online source
    code if not buggy?

    I fixed the syntax error and probably spent longer on that than I
    would have if I had just written the darn thing myself.

    So I rewrote it and read the documentation. Wow, imagine that.
    """

    s = SequenceMatcher(isjunk=None, a=a, b=b)
    alo, blo, size = s.find_longest_match(0, len(a), 0, len(b))
    assert a[alo : alo + size] == b[blo : blo + size]
    return a[alo : alo + size]


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
        warn(f"log {path} has no compute time")
        return 0
        raise ValueError(f"log {path} has no compute time")
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple compute times")
    time = float(matching_lines[0].group(4))
    return time


def plot_runtime(inputs: list[Path], extensions: list[str], output: Path):
    compute_times = {}
    for file in get_file_tree(inputs):
        if file.suffix not in extensions:
            continue
        with file.open() as f:
            text = f.read()
        ttime = get_trace_read_time_from_log(text, file)
        ctime = get_compute_time_from_log(text, file)
        print(f"Times for {str(file)}: Trace read: {ttime} -- Total compute: {ctime}")
        compute_times[str(file)] = ctime
    compute_times = {k: v for k, v in sorted(compute_times.items())}

    emap_times = {k: v for k, v in compute_times.items() if "Evicting-Map" in k}
    fss_times = {k: v for k, v in compute_times.items() if "Fixed-Size-SHARDS" in k}
    fig, axs = plt.subplots()
    fig.set_size_inches(12, 8)
    fig.suptitle("Runtimes by Trace")
    fig.supxlabel("Trace Name")
    fig.supylabel("Runtimes [seconds]")
    axs.plot([t for t in emap_times.values()], label="Evicting Map")
    axs.plot([t for t in fss_times.values()], label="Fixed-Size SHARDS")
    axs.legend()

    # Add labels to the axis
    emap_stems = get_file_stems(emap_times.keys())
    fss_stems = get_file_stems(fss_times.keys())
    labels = [
        get_longest_substring(emap_stem, fss_stem).strip(" \t_-./")
        for emap_stem, fss_stem in zip(emap_stems, fss_stems)
    ]
    axs.set_xticks(
        range(len(labels)),
        labels,
        rotation=90,
    )
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
        warn(f"log {path} has no accuracy")
        return 0, 0
        raise ValueError(f"log {path} has no compute time")
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple compute times")
    mae, mse = float(matching_lines[0].group(1)), float(matching_lines[0].group(2))
    return mae, mse


def plot_accuracy(inputs: list[Path], extensions: list[str], output: Path):
    mae_accuracies = {}
    for file in tqdm(get_file_tree(inputs)):
        if file.suffix not in extensions:
            continue
        with file.open() as f:
            text = f.read()
        mae, mse = get_accuracy_from_log(text, file)
        print(f"Accuracies for {str(file)}: MAE: {mae} -- MSE: {mse}")
        mae_accuracies[str(file)] = mae
    mae_accuracies = {k: v for k, v in sorted(mae_accuracies.items())}

    emap_accuracies = {k: v for k, v in mae_accuracies.items() if "Evicting-Map" in k}
    fss_accuracies = {
        k: v for k, v in mae_accuracies.items() if "Fixed-Size-SHARDS" in k
    }
    fig, axs = plt.subplots()
    fig.set_size_inches(12, 8)
    fig.suptitle("Mean Absolute Error (MAE) by Trace")
    fig.supxlabel("Trace Name")
    fig.supylabel("Mean Absolute Error (MAE) [%]")
    axs.plot(
        [100 * x for x in emap_accuracies.values()],
        label="Evicting Map",
    )
    axs.plot([100 * x for x in fss_accuracies.values()], label="Fixed-Size SHARDS")

    emap_stems = get_file_stems(emap_accuracies.keys())
    fss_stems = get_file_stems(fss_accuracies.keys())
    labels = [
        get_longest_substring(emap_stem, fss_stem).strip(" \t_-./")
        for emap_stem, fss_stem in zip(emap_stems, fss_stems)
    ]
    axs.set_xticks(
        range(len(labels)),
        labels,
        rotation=90,
    )
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
