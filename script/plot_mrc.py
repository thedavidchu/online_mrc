import argparse
import json
from typing import Dict, Union

import numpy as np
import matplotlib.pyplot as plt


INFINITY = float("inf")


def decode_json_as_histogram(
        histogram: Union[str, Dict[str, Union[float, int]]]
) -> Dict[float, Union[int, float]]:
    if isinstance(histogram, str):
        histogram = json.loads(histogram)
    if "type" not in histogram:
        raise ValueError("expected 'type' field in histogram")
    
    if histogram["type"] in {"BasicHistogram", "FractionalHistogram"}:
        histogram = histogram["histogram"]
    histogram = {float(key): value for key, value in histogram.items() if value > 0 and key != INFINITY}
    return histogram


def input_histogram() -> Dict[float, Union[int, float]]:
    while True:
        histogram_text = input()
        # NOTE  We assume that there will only be a single JSON. This will be
        #       only line that starts with the character '{' and the entire
        #       JSON string will be on a single line.
        if histogram_text.startswith(r"{"):
            break
    histogram = decode_json_as_histogram(histogram_text)
    return histogram


def convert_histogram_to_miss_rate_curve(
        histogram: Dict[float, Union[float, int]]
) -> Dict[float, Union[float, int]]:
    total = sum(histogram.values())
    running_inverse_total = total
    mrc = {}
    for key, value in histogram.items():
        mrc[key] = running_inverse_total / total
        running_inverse_total -= value
    return mrc

def plot_histogram(histogram: Dict[float, Union[float, int]]):
    plt.figure()
    plt.title("Histogram")
    plt.xlabel("Reuse Stack Distance")
    plt.ylabel("Frequency")
    plt.plot(histogram.keys(), histogram.values())
    plt.savefig("histogram.png")


def plot_miss_rate_curve(mrc: Dict[float, Union[float, int]]):
    plt.figure()
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.plot(mrc.keys(), mrc.values())
    plt.savefig("mrc.png")


def read_and_plot_mrc(file_name: str):
    with open(file_name, "rb") as f:
        mrc = np.fromfile(f, dtype=np.float64)
    plt.figure()
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.plot(mrc)
    plt.savefig("mrc.png")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input",
        type=str,
        default="stdin",
        help=r"Possibilities: {stdin,file,hardcode}"
    )
    parser.add_argument(
        "--file-name",
        required=False,
        help="File name for the '--input file' option"
    )
    args = parser.parse_args()

    if args.input == "hardcode":
        histogram = {k: v for k, v in enumerate([1.000000, 0.912621, 0.825243, 0.708738, 0.621359, 0.582524, 0.504854, 0.359223, 0.271845, 0.213592, 0.135922, 0.029126])}
        histogram = decode_json_as_histogram(histogram)
        plot_histogram(histogram)
        mrc = convert_histogram_to_miss_rate_curve(histogram)
        plot_miss_rate_curve(mrc)
    elif args.input == "stdin":
        histogram = input_histogram()
        plot_histogram(histogram)
        mrc = convert_histogram_to_miss_rate_curve(histogram)
        plot_miss_rate_curve(mrc)
    elif args.input == "file":
        read_and_plot_mrc(args.file_name)
    else:
        print("Invalid args.input")


if __name__ == "__main__":
    main()
