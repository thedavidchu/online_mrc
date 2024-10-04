"""Analyze a log file for timing information."""

import argparse
import os
import re
from collections import Counter
from datetime import datetime
from difflib import SequenceMatcher
from pathlib import Path
from warnings import warn

import matplotlib.pyplot as plt
import numpy as np
from tqdm import tqdm


def check_no_matches(*args) -> bool:
    """Check that all of the arguments are unique (or are None).

    Examples
    --------
    1. {1, 2, None, None} -> True
    2. {0, 0} -> False
    """
    num_none = sum(arg is None for arg in args)
    num_unique = len({arg for arg in args if arg is not None})
    return len(args) == num_none + num_unique


# NOTE  I do not support spaces or control characters in paths.
PATH_PATTERN = r"[a-zA-Z0-9_./\\-]+"
FLOAT_PATTERN = r"\d+[.]\d+"


def get_log_pattern(
    level: str,
    pattern: str,
    *,
    capture_time: bool = False,
    capture_path: bool = False,
    capture_error: bool = False,
) -> str:
    def optionally_bracket(target: str, bracket: bool) -> str:
        return f"({target})" if bracket else target

    # NOTE  This is not an efficient way of checking, but it is easier
    #       on the eyes than constructing a set of individual strings.
    assert level in "VERBOSE,TRACE,DEBUG,INFO,WARN,ERROR,FATAL".split(",")
    time_pattern = optionally_bracket(r"\d+-\d+-\d+ \d+:\d+:\d+", capture_time)
    path_pattern = optionally_bracket(rf"{PATH_PATTERN}:\d+", capture_path)
    error_pattern = optionally_bracket(r"errno \d+: [^\]]+", capture_error)
    return rf"^\[{level}\] \[{time_pattern}\] \[ {path_pattern} \] \[{error_pattern}\] {pattern}"


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


def get_file_stem(path: str) -> str:
    return os.path.splitext(os.path.split(path)[-1])[0]


def get_longest_substring(*args: str) -> str:
    """
    I originally copied this code from
    https://www.geeksforgeeks.org/sequencematcher-in-python-for-longest-common-substring/.
    Of course, they had a syntax error, because what would online source
    code if not buggy?

    I fixed the syntax error and probably spent longer on that than I
    would have if I had just written the darn thing myself.

    So I rewrote it and read the documentation. Wow, imagine that.
    """

    def get_longest_substring_twoway(a: str, b: str) -> str:
        s = SequenceMatcher(isjunk=None, a=a, b=b)
        alo, blo, size = s.find_longest_match(0, len(a), 0, len(b))
        assert a[alo : alo + size] == b[blo : blo + size]
        return a[alo : alo + size]

    # Pattern matching is your friend! Wow, that was so easy... I just
    # hope that it actually works, haha!
    match args:
        case []:
            # Maybe we should return None. I'm too tired to think.
            return ""
        case [x]:
            return x
        case [x, y]:
            # I think this case may be redundant considering the case
            # below, but this is more symmetric and therefore easier to
            # reason about.
            return get_longest_substring_twoway(x, y)
        case [x, y, *z]:
            return get_longest_substring(get_longest_substring_twoway(x, y), *z)


def get_x_axis_labels(file_names: list[str]) -> list[str]:
    return [get_file_stem(f) for f in file_names]


def get_trace_read_time_from_log(text: str, path: Path) -> float:
    pattern = re.compile(
        get_log_pattern("INFO", rf"Trace Read Time: ({FLOAT_PATTERN}) sec$")
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        warn(f"log {path} has no trace time")
        return 0.0
    if len(matching_lines) > 1:
        raise ValueError(f"log {path} has multiple trace times")
    time = float(matching_lines[0].group(1))
    return time


def get_compute_time_from_log(text: str, path: Path) -> dict[str, float]:
    pattern = re.compile(
        get_log_pattern(
            "INFO",
            rf"({PATH_PATTERN}) -- Histogram Time: ({FLOAT_PATTERN}) [|] Post-Process Time: ({FLOAT_PATTERN}) [|] MRC Time: ({FLOAT_PATTERN}) [|] Total Time: ({FLOAT_PATTERN})$",
        )
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
        return {}
    return {l.group(1): float(l.group(5)) for l in matching_lines}


def plot_runtime(
    inputs: list[Path],
    extensions: list[str],
    outputs: list[Path],
    algorithms: list[str],
    plot_trace_read_times: bool,
):
    """
    @note   I combined the runtime and the trace time simply so I
            wouldn't have to copy and paste this function another time.
            They aren't closely related or anything. If you can cleverly
            refactor the boilerplate logic out, I'd encourage you to
            split these two in the future."""
    assert all(isinstance(x, str) for x in algorithms)
    files = sorted(
        [f for f in get_file_tree(inputs) if f.suffix in extensions], key=get_file_stem
    )
    compute_times = {algo: np.full_like(files, np.nan) for algo in algorithms}
    trace_read_times = np.full_like(files, np.nan)
    for i, file in tqdm(enumerate(files)):
        with file.open() as f:
            text = f.read()
        ttime = get_trace_read_time_from_log(text, file)
        trace_read_times[i] = ttime
        ctimes = get_compute_time_from_log(text, file)
        for algo, ctime in ctimes.items():
            print(
                f"Times for {str(file)}:{algo} -- Trace read: {ttime} | Total compute: {ctime}"
            )
            if algo in algorithms:
                compute_times[algo][i] = ctime
    fig, axs = plt.subplots()
    fig.set_size_inches(12, 8)
    fig.suptitle("Runtimes by Trace")
    fig.supxlabel("Trace Name")
    fig.supylabel("Runtimes [seconds]")
    if plot_trace_read_times:
        axs.plot(trace_read_times, "x-", label="Trace Read Times")
    for algo, times in compute_times.items():
        pretty_label = algo.replace("-", " ")
        axs.plot(times, "x-", label=pretty_label)
    axs.legend()

    # Add labels to the axis
    labels = get_x_axis_labels([str(f) for f in files])
    axs.set_xticks(
        range(len(labels)),
        labels,
        rotation=90,
    )
    for output in outputs:
        fig.savefig(output)


def get_accuracy_from_log(text: str, path: Path) -> dict[str, tuple[float, float]]:
    pattern = re.compile(
        get_log_pattern(
            "INFO",
            rf"({PATH_PATTERN}) -- Mean Absolute Error \(MAE\): ({FLOAT_PATTERN}) [|] Mean Squared Error \(MSE\): ({FLOAT_PATTERN})",
        )
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        warn(f"log {path} has no accuracy")
        return {}
    # NOTE  I no longer explicit name them here, but the first float is
    #       the MAE and the second is the MSE.
    return {l.group(1): (float(l.group(2)), float(l.group(3))) for l in matching_lines}


def plot_accuracy(
    inputs: list[Path],
    extensions: list[str],
    outputs: list[Path],
    algorithms: list[str],
):
    assert all(isinstance(x, str) for x in algorithms)
    files = sorted(
        [f for f in get_file_tree(inputs) if f.suffix in extensions], key=get_file_stem
    )
    mean_absolute_errors = {algo: np.full_like(files, np.nan) for algo in algorithms}
    for i, file in tqdm(enumerate(files)):
        with file.open() as f:
            text = f.read()
        accuracies = get_accuracy_from_log(text, file)
        for algo, (mae, mse) in accuracies.items():
            print(f"Accuracies for {str(file)}:{algo} -- MAE: {mae} | MSE: {mse}")
            if algo in algorithms:
                mean_absolute_errors[algo][i] = mae
    fig, axs = plt.subplots()
    fig.set_size_inches(12, 8)
    fig.suptitle("Mean Absolute Error (MAE) by Trace")
    fig.supxlabel("Trace Name")
    fig.supylabel("Mean Absolute Error (MAE) [%]")
    axs.set_ylim(bottom=0)
    for algo, accuracies in mean_absolute_errors.items():
        pretty_label = algo.replace("-", " ")
        axs.plot(100 * accuracies, "x-", label=pretty_label)

    labels = get_x_axis_labels([str(f) for f in files])
    axs.set_xticks(
        range(len(labels)),
        labels,
        rotation=90,
    )
    axs.legend()
    for output in outputs:
        fig.savefig(output)


def get_runner_arguments_from_log(
    text: str, path: Path
) -> list[dict[str, bool | float | int | str | None]]:
    def str2bool(boolstr: str) -> bool:
        if boolstr == "true":
            return True
        elif boolstr == "false":
            return False
        else:
            raise ValueError(f"unexpected boolean string {boolstr}")

    pattern = re.compile(
        rf"RunnerArguments\("
        rf"algorithm=([\w-]+), "
        rf"mrc=(\(?{PATH_PATTERN}\)?), "
        rf"hist=(\(?{PATH_PATTERN}\)?), "
        # This can be a float or an int. It may also be in the format
        # '1e-3' so I shouldn't try constraining it.
        rf"sampling=([^,]+), "
        rf"num_bins=(\d+), "
        rf"bin_size=(\d+), "
        rf"max_size=(\d+), "
        rf"mode=(\w+), "
        rf"adj=(true|false), "
        rf"qmrc_size=(\d+), "
        rf"dictionary={{.*}}"
        rf"\)"
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        warn(f"log {path} has no runner arguments")
    return [
        dict(
            # These are alphabetic with dashes.
            algorithm=m.group(1),
            mrc=None if m.group(2) == "(null)" else Path(m.group(2)),
            hist=None if m.group(3) == "(null)" else Path(m.group(3)),
            sampling=float(m.group(4)),
            num_bins=int(m.group(5)),
            bin_size=int(m.group(6)),
            max_size=int(m.group(7)),
            # These are alphabetic with underscores.
            mode=m.group(8),
            adj=str2bool(m.group(9)),
            qmrc_size=int(m.group(10)),
        )
        for m in matching_lines
    ]


def get_throughput_from_log(text: str, path: Path) -> list[dict[datetime, int]]:
    pattern = re.compile(
        get_log_pattern("TRACE", rf"Finished (\d+) / (\d+)", capture_time=True)
    )
    matching_lines = [
        re.match(pattern, line)
        for line in text.splitlines()
        if re.match(pattern, line) is not None
    ]
    if len(matching_lines) == 0:
        warn(f"log {path} has no throughput")
        return []
    r = [
        (
            datetime.strptime(m.group(1), r"%Y-%m-%d %H:%M:%S").timestamp(),
            int(m.group(2)),
            int(m.group(3)),
        )
        for m in matching_lines
    ]
    # Group the matches.
    accum = []
    for time, numerator, denominator in r:
        if numerator == 0:
            base = time
            accum.append([])
        else:
            accum[-1].append(time - base)
    # Count the number of identical timestamps in each group.
    accum = [{time: count for time, count in sorted(Counter(i).items())} for i in accum]
    return accum


def plot_throughput(
    inputs: list[Path],
    extensions: list[str],
    outputs: list[Path],
):
    """
    @brief  Count the number of millions of entries processed per second.
    @note   As is good engineering practice, we simplify the
            implementation as much as possible.
    """
    files = sorted(
        [f for f in get_file_tree(inputs) if f.suffix in extensions], key=get_file_stem
    )
    data = []
    for i, file in tqdm(enumerate(files)):
        with file.open() as f:
            text = f.read()
        runner_args = get_runner_arguments_from_log(text, file)
        throughput = get_throughput_from_log(text, file)
        data.append((runner_args, throughput))
    fig, axs = plt.subplots()
    fig.set_size_inches(12, 8)
    fig.suptitle("Throughput vs Time")
    fig.supxlabel("Time from Start [seconds]")
    fig.supylabel("Throughput [millions of entries processed]")
    for file, (runner_args, throughputs) in zip(files, data):
        for runner_arg, throughput in zip(runner_args, throughputs):
            pretty_label = f"{str(file)}:{runner_arg['algorithm']}"
            axs.plot(throughput.keys(), throughput.values(), "x-", label=pretty_label)
    axs.legend()
    for output in outputs:
        fig.savefig(output)


def analyze_log(
    inputs: list[Path],
    extensions: list[str],
    time_: list[Path],
    accuracy: list[Path],
    olken_time: list[Path],
    trace_time: list[Path],
    throughput: list[Path],
):
    if not check_no_matches(
        *time_ if time_ is not None else [],
        *accuracy if accuracy is not None else [],
        *olken_time if olken_time is not None else [],
        *trace_time if trace_time is not None else [],
        *throughput if throughput is not None else [],
    ):
        raise ValueError(
            f"duplicate values in {time_, accuracy, olken_time, trace_time, throughput}"
        )

    if time_ is not None:
        outputs = [Path("time.pdf")] if time_ == [] else time_
        plot_runtime(
            inputs,
            extensions,
            outputs,
            ["Evicting-Map", "Fixed-Size-SHARDS"],
            False,
        )
    if olken_time is not None:
        outputs = [Path("olken-time.pdf")] if olken_time == [] else olken_time
        plot_runtime(inputs, extensions, outputs, ["Olken"], False)
    if trace_time is not None:
        outputs = [Path("trace-time.pdf")] if trace_time == [] else trace_time
        plot_runtime(inputs, extensions, outputs, [], True)
    if accuracy is not None:
        outputs = [Path("accuracy.pdf")] if accuracy == [] else accuracy
        plot_accuracy(
            inputs, extensions, outputs, ["Evicting-Map", "Fixed-Size-SHARDS"]
        )
    if throughput is not None:
        outputs = [Path("throughput.pdf")] if throughput == [] else throughput
        plot_throughput(inputs, extensions, outputs)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--inputs", "-i", nargs="+", type=Path, help="file/directory of input(s)"
    )
    parser.add_argument(
        "--extensions",
        nargs="+",
        type=str,
        default=[".log"],
        help="extensions to process (format: '--extensions .log .csv')",
    )
    parser.add_argument(
        "--time",
        nargs="*",
        type=Path,
        default=None,
        help="specifying this will plot runtime. Default: time.pdf (ignore what Python says). You can replace this with your own custom names!",
    )
    parser.add_argument(
        "--olken-time",
        nargs="*",
        type=Path,
        default=None,
        help="specifying this will plot Olken runtime and/or trace read time. Default: olken-time.pdf (ignore what Python says). You can replace this with your own custom names!",
    )
    parser.add_argument(
        "--trace-time",
        nargs="*",
        type=Path,
        default=None,
        help="specifying this will plot trace read time. Default: trace-time.pdf (ignore what Python says). You can replace this with your own custom names!",
    )
    parser.add_argument(
        "--accuracy",
        nargs="*",
        type=Path,
        default=None,
        help="specifying this will plot accuracy. Default: accuracy.pdf (ignore what Python says). You can replace this with your own custom names!",
    )
    parser.add_argument(
        "--throughput",
        nargs="*",
        type=Path,
        default=None,
        help="specifying this will plot throughput. Default: throughput.pdf (ignore what Python says). You can replace this with your own custom names!",
    )
    args = parser.parse_args()

    analyze_log(
        inputs=args.inputs,
        extensions=args.extensions,
        time_=args.time,
        accuracy=args.accuracy,
        olken_time=args.olken_time,
        trace_time=args.trace_time,
        throughput=args.throughput,
    )


if __name__ == "__main__":
    main()
