#!/usr/bin/python3
import argparse
import json
from pathlib import Path
from typing import Callable

import matplotlib.pyplot as plt


def parse_data(input_file: Path) -> list[dict]:
    """@return [JSONs]"""
    if not input_file.exists():
        raise FileNotFoundError(f"'{str(input_file)}' not found")
    file_data = input_file.read_text().splitlines()
    # Parse into JSON for each run of the cache
    accum = []
    for line in file_data:
        if line.startswith("> "):
            try:
                data = json.loads(line[2:])
            except json.decoder.JSONDecodeError as e:
                print(f"bad JSON: '{line[2:]}'")
                raise e

            accum.append(data)
    return accum


def parse_number(num: str | float | int) -> float | int:
    """Parse a quantity to canonical units (bytes, milliseconds)."""
    memsizes = {
        "TB": 1e12,
        "GB": 1e9,
        "MB": 1e6,
        "KB": 1e3,
        "TiB": 1 << 40,
        "GiB": 1 << 30,
        "MiB": 1 << 20,
        "KiB": 1 << 10,
        "B": 1,
    }
    plural_times = {
        "years": 365 * 24 * 3600 * 1000,
        "days": 24 * 3600 * 1000,
        "hours": 3600 * 1000,
        "minutes": 60 * 1000,
        "seconds": 1000,
        "milliseconds": 1,
    }
    single_times = {
        "year": 365 * 24 * 3600 * 1000,
        "day": 24 * 3600 * 1000,
        "hour": 3600 * 1000,
        "minute": 60 * 1000,
        "second": 1000,
        "millisecond": 1,
    }
    short_times = {
        "h": 3600 * 1000,
        "min": 60 * 1000,
        "sec": 1000,
        "s": 1000,
        "ms": 1,
    }
    if isinstance(num, (float, int)):
        return num
    if not num[-1].isalpha():
        return float(num)
    val, unit = num.split()
    return (
        float(val) * {**memsizes, **plural_times, **single_times, **short_times}[unit]
    )


def data_vs_capacity(
    data_list: list[dict[str, object]], get_value: Callable[[dict[str, object]], object]
) -> dict[float, object]:
    return dict(
        sorted(
            (
                parse_number(d["Capacity [B]"])
                / d["Extras"]["SHARDS"][".sampling_ratio"]
                / (1 << 30),
                get_value(d),
            )
            for d in data_list
        )
    )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, required=True, help="input file")
    parser.add_argument("--mrc", type=Path, help="MRC output path")
    parser.add_argument(
        "--lifetime-threshold", type=Path, help="Lifetime threshold output path"
    )
    args = parser.parse_args()

    input_file = args.input.resolve()
    data_list = parse_data(input_file)

    if args.mrc is not None:
        mrc = data_vs_capacity(data_list, lambda d: d["Miss Ratio"])
        fig, ax = plt.subplots()
        ax.set_title("Miss Ratio Curve")
        ax.set_xlabel("Cache Size [GiB]")
        ax.set_ylabel("Miss Ratio")
        ax.plot(mrc.keys(), mrc.values(), "x-")
        ax.set_ylim(0.0, 1.0)
        fig.savefig(args.mrc.resolve())
    if args.lifetime_threshold is not None:
        lifetime_thresholds = data_vs_capacity(
            data_list,
            lambda d: {
                parse_number(f): hist["Histogram"]["total"]
                for f, hist in d["Lifetime Thresholds"].items()
            },
        )
        fig, axes = plt.subplots(
            nrows=1,
            ncols=len(lifetime_thresholds),
            squeeze=False,
            sharey=True,
            sharex=True,
            figsize=(1 * len(lifetime_thresholds), 9),
        )
        fig.suptitle("Maximum Evicted Frequency")
        fig.supxlabel("Cache Size [GiB]")
        fig.supylabel("LFU Frequency")
        for ax, (c, hist) in zip(axes[0], lifetime_thresholds.items()):
            ax.set_title(f"{c:.3} GiB")
            ax.set_xlabel("\nHistogram\nFrequency")
            # I was going to use a violin plot, but it tries to map your
            # data to a Gaussian distribution, which my data isn't.
            ax.plot(hist.values(), hist.keys(), "x-")
            ax.axhline(y=0, color="black", linestyle="--", label="y=0")
        fig.savefig(args.lifetime_threshold.resolve())


if __name__ == "__main__":
    main()
