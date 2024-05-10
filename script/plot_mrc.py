import argparse
import json
import os

import numpy as np
import matplotlib.pyplot as plt


INFINITY = float("inf")


def decode_histogram_json(
    histogram_json: dict[str, float | int | dict[str, float]]
) -> dict[float, float]:
    """
    Expecting a JSON (string or object) of format:
    {
        ".type": "Histogram" | "FractionalHistogram",
        ".num_bins": %u64,
        ".bin_size": %u64,
        ".running_sum": %u64,
        ".histogram": {"%f": %f},
        ".false_infinity": %f,
        ".infinity": %f
    }
    """
    assert histogram_json["type"] in {"Histogram", "FractionalHistogram"}

    histogram = {
        float(key) * histogram_json[".bin_size"]: float(value)
        for key, value in histogram_json[".histogram"].items()
    }
    histogram.update(
        (
            (
                histogram_json[".num_bins"] * histogram_json[".bin_size"],
                histogram_json[".false_infinity"],
            ),
            (INFINITY, histogram_json[".infinity"]),
        )
    )
    histogram = dict(sorted(list(histogram.items())))
    return histogram


def convert_histogram_to_miss_rate_curve(
    histogram: dict[float, float]
) -> dict[float, float]:
    total = sum(histogram.values())
    running_inverse_total = total
    mrc = {}
    for key, value in histogram.items():
        mrc[key] = running_inverse_total / total
        running_inverse_total -= value
    return mrc


def plot_histogram(histogram: dict[float, float]):
    plt.figure()
    plt.title("Histogram")
    plt.xlabel("Reuse Stack Distance")
    plt.ylabel("Frequency")
    plt.plot(histogram.keys(), histogram.values())
    plt.savefig("histogram.png")


def plot_miss_rate_curve(input_path: str, label: str):
    with open(input_path) as f:
        histogram_json = json.load(f)
    sparse_histogram = decode_histogram_json(histogram_json)
    sparse_mrc = convert_histogram_to_miss_rate_curve(sparse_histogram)
    plt.plot(sparse_mrc.keys(), sparse_mrc.values(), label=label)


def read_and_plot_mrc(input_path: str, label: str):
    with open(input_path, "rb") as f:
        dense_mrc = np.fromfile(f, dtype=np.float64)
    plt.plot(dense_mrc, label=label)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", nargs="+", type=str, required=True, help="input path(s)"
    )
    parser.add_argument("--output", type=str, default="mrc.png", help="output path")
    args = parser.parse_args()

    input_paths: list[str] = args.input
    output_path = args.output

    plt.figure()
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.ylim(0, 1)
    for input_path in input_paths:
        if os.path.splitext(input_path)[-1] == ".json":
            plot_miss_rate_curve(input_path, os.path.splitext(input_path)[0])
        else:
            read_and_plot_mrc(input_path, os.path.splitext(input_path)[0])
    plt.legend()
    plt.savefig(output_path)


if __name__ == "__main__":
    main()
