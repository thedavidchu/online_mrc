import argparse
import json
import os

import numpy as np
import matplotlib.pyplot as plt


INFINITY = float("inf")


def timing(f):
    """I ripped this off of StackOverFlow: https://stackoverflow.com/questions/1622943/timeit-versus-timing-decorator"""
    from functools import wraps
    from time import time

    @wraps(f)
    def wrap(*args, **kw):
        ts = time()
        result = f(*args, **kw)
        te = time()
        pretty_args = [repr(v) for v in args]
        pretty_kw = [f"{k}={repr(v)}" for k, v in kw.items()]
        all_args = ", ".join([*pretty_args, *pretty_kw])
        print(f"{f.__name__}({all_args}) took: {te - ts} sec")
        return result

    return wrap


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


def plot_miss_rate_curve(path: str, label: str, debug: bool):
    with open(path) as f:
        histogram_json = json.load(f)
    sparse_histogram = decode_histogram_json(histogram_json)
    sparse_mrc = convert_histogram_to_miss_rate_curve(sparse_histogram)
    if debug:
        print(sparse_histogram)
    plt.plot(sparse_mrc.keys(), sparse_mrc.values(), label=label)


def read_and_plot_dense_mrc(path: str, label: str, debug: bool):
    with open(path, "rb") as f:
        dense_mrc = np.fromfile(f, dtype=np.float64)
    if debug:
        print(dense_mrc)
    plt.plot(dense_mrc, label=label)


def read_and_plot_sparse_mrc(path: str, label: str, debug: bool):
    dt = np.dtype([("index", np.uint64), ("miss-rate", np.float64)])
    with open(path, "rb") as f:
        sparse_mrc = np.fromfile(f, dtype=dt)
    if debug:
        print(sparse_mrc)
    plt.plot(sparse_mrc["index"], sparse_mrc["miss-rate"], label=label)


@timing
def plot_from_path(path: str, *, label: str = None, debug: bool = False):
    root, ext = os.path.splitext(path)

    # Set the label to the root of the file name if the user hasn't
    # specified a custom label. I do this so then I can label the oracle.
    if label is None:
        label = root

    if ext == ".json":
        plot_miss_rate_curve(path, label, debug)
    elif ext == ".bin":
        read_and_plot_sparse_mrc(path, label, debug)
    else:
        raise ValueError(f"unrecognized file type. Expecting {{.json,.bin}}, got {ext}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", type=str, required=False, help="path to oracle")
    parser.add_argument(
        "--input", nargs="+", type=str, required=True, help="input path(s)"
    )
    parser.add_argument("--output", type=str, default="mrc.png", help="output path")
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    oracle_path = args.oracle
    input_paths: list[str] = args.input
    output_path = args.output

    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.ylim(0, 1)
    if oracle_path is not None:
        root, _ = os.path.splitext(oracle_path)
        plot_from_path(oracle_path, label=f"Oracle ({root})", debug=args.debug)
    for input_path in input_paths:
        plot_from_path(input_path, debug=args.debug)
    plt.legend()
    plt.savefig(output_path)


if __name__ == "__main__":
    main()
