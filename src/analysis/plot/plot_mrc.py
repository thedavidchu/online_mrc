import argparse
import os
from pathlib import Path
from warnings import warn

import numpy as np
import matplotlib.pyplot as plt


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


def read_and_plot_mrc(path: Path, label: str, debug: bool, separator: str):
    if not path.exists or not path.is_file:
        warn(f"{str(path)} DNE")
        return
    dt = np.dtype([("index", np.uint64), ("miss-rate", np.float64)])
    with open(path, "rb") as f:
        if separator == "":
            _ = np.fromfile(
                f,
                dtype=np.dtype([("num_bins", np.uint64), ("bin_size", np.uint64)]),
                count=1,
            )
            sparse_mrc = np.fromfile(f, dtype=dt)
        else:
            sparse_mrc = np.loadtxt(f, dtype=dt, delimiter=separator)
    if debug:
        print(sparse_mrc)
    plt.step(sparse_mrc["index"], sparse_mrc["miss-rate"], where="post", label=label)


@timing
def plot_from_path(path: Path, *, label: str | None = None, debug: bool = False):
    # Set the label to the root of the file name if the user hasn't
    # specified a custom label. I do this so then I can label the oracle.
    if label is None:
        label = path.stem
    if path.suffix == ".bin":
        read_and_plot_mrc(path, label, debug, separator="")
    elif path.suffix == ".dat":
        read_and_plot_mrc(path, label, debug, separator=",")
    else:
        raise ValueError(
            f"unrecognized file type for '{path}'. Expecting {{.bin,.dat}}, got {path.suffix}"
        )


def plot_mrc(
    oracle_path: Path | None,
    input_paths: list[Path],
    output_paths: list[Path],
    debug: bool,
):
    plt.figure(figsize=(12, 8), dpi=300)
    plt.title("Miss-Rate Curve")
    plt.xlabel("Number of key-value pairs")
    plt.ylabel("Miss-rate")
    plt.ylim(0, 1.01)
    if oracle_path is not None:
        plot_from_path(oracle_path, label=f"Oracle ({oracle_path.stem})", debug=debug)
    for input_path in input_paths:
        plot_from_path(input_path, debug=debug)
    plt.legend()
    for output_path in output_paths:
        plt.savefig(output_path)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--oracle", type=Path, required=False, help="path to oracle")
    parser.add_argument(
        "--input", "-i", nargs="+", type=Path, required=True, help="input path(s)"
    )
    parser.add_argument(
        "--output",
        "-o",
        nargs="*",
        type=Path,
        default=["mrc.pdf"],
        help="output path(s)",
    )
    parser.add_argument("--debug", action="store_true")
    args = parser.parse_args()

    plot_mrc(args.oracle, args.input, args.output, args.debug)


if __name__ == "__main__":
    main()
