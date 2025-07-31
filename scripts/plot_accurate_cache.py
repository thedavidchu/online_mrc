#!/usr/bin/python3
import argparse
from pathlib import Path
from typing import Callable

import matplotlib.pyplot as plt
import numpy as np
from plot_predictive_cache import parse_data, parse_number


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


def plot_lifetime_threshold(violin: bool, data_list, output: Path):
    lifetime_thresholds = data_vs_capacity(
        data_list,
        lambda d: {
            parse_number(f): hist["Histogram"]["total"]
            for f, hist in d["Oracle"]["Lifetime Thresholds"].items()
        },
    )
    if not violin:
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
    else:
        fig, ax = plt.subplots()
        fig.suptitle("Distribution of LFU Frequency Evictions")
        fig.supxlabel("Cache Size [GiB]")
        fig.supylabel("Distribution of LFU Frequency Evictions")
        ax.axhline(y=0, color="black", linestyle="--", label="y=0")
        # I was going to use a violin plot, but it tries to map your
        # data to a Gaussian distribution, which my data isn't.
        vpstats = [
            dict(
                coords=list(hist.keys()),
                vals=np.log10(np.array(list(hist.values()))),
                mean=0,
                median=0,
                min=0,
                max=0,
            )
            for hist in lifetime_thresholds.values()
        ]
        positions = list(lifetime_thresholds.keys())
        ax.violin(
            vpstats,
            positions,
        )
    fig.savefig(output.resolve())


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, required=True, help="input file")
    parser.add_argument("--mrc", type=Path, help="MRC output path")
    parser.add_argument(
        "--lifetime-threshold", type=Path, help="Lifetime threshold output path"
    )
    args = parser.parse_args()

    input_file = args.input.resolve()
    data_list = parse_data(input_file)[(0.0, 1.0, "EvictionTime")]

    if args.mrc is not None:
        mrc = data_vs_capacity(data_list, lambda d: d["Oracle"]["Miss Ratio"])
        fig, ax = plt.subplots()
        ax.set_title("Miss Ratio Curve")
        ax.set_xlabel("Cache Size [GiB]")
        ax.set_ylabel("Miss Ratio")
        ax.plot(mrc.keys(), mrc.values(), "x-")
        ax.set_ylim(0.0, 1.0)
        fig.savefig(args.mrc.resolve())
    if args.lifetime_threshold is not None:
        plot_lifetime_threshold(True, data_list, args.lifetime_threshold)


if __name__ == "__main__":
    main()
