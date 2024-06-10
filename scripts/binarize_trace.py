#!/usr/bin/python3
"""Convert a trace into Kia's data type"""

import argparse
import os

import numpy as np
import pandas as pd

INPUT_FORMATS = ["MSR"]
OUTPUT_FORMATS = ["Kia"]

KIA_DTYPE = np.dtype(
    [
        ("timestamp", np.uint64),
        ("command", np.uint8),
        ("key", np.uint64),
        ("size", np.uint32),
        ("ttl", np.uint32),
    ]
)


def msr_parser(fname: str) -> np.ndarray:
    df = pd.read_csv(
        fname,
        header=0,
        # Leading underscore denotes the column is unused
        names=[
            "timestamp",
            "_workload",
            "_unknown_0",
            "command",
            "key",
            "size",
            "_unknown_1",
        ],
        usecols=lambda x: not x.startswith("_"),
        # NOTE  The converter parameter takes precedence over dtype, so
        #       I do not include the "command" converter in dtype.
        dtype=np.dtype(
            [
                ("timestamp", np.uint64),
                ("_workload", str),
                ("_unknown_0", str),
                # NOTE  I intentionally misnamed this with a leading underscore.
                #       I get a warning about having both a dtype and a converter
                #       for "command", but it works this way (non-trivial getting
                #       it to work the other way, and I get better performance
                #       this way too).
                ("_command", np.uint8),
                ("key", np.uint64),
                ("size", np.uint32),
                ("_unknown_1", str),
            ]
        ),
        converters={"command": lambda x: 0 if x == "Read" else 1},
        on_bad_lines="error",
    )

    df["command"] = df["command"].astype(np.uint8)
    # NOTE There are no TTLs, so we set the TTL to zero
    df["ttl"] = np.zeros_like(df["timestamp"], dtype=np.uint32)
    return df


def parse_file(fname: str, text_format: str):
    if not os.path.exists(fname):
        raise ValueError(f"file '{fname}' does not exist")
    if not os.path.isfile(fname):
        raise ValueError(f"file '{fname}' is not a file")
    if text_format not in INPUT_FORMATS:
        raise ValueError(f"got {text_format}, expected {INPUT_FORMATS}")
    match text_format:
        case "MSR":
            parsed_lines = msr_parser(fname)
        case _:
            raise ValueError(f"unrecognized input format: {text_format}")
    return parsed_lines


def save_binary(output_file: str, output: pd.DataFrame, binary_format: str):
    if binary_format not in OUTPUT_FORMATS:
        raise ValueError(f"got {binary_format}, expected {OUTPUT_FORMATS}")
    match binary_format:
        case "Kia":
            # NOTE  I do this manually because the conversion function
            #       pd.DataFrame.to_numpy() wasn't cooperating.
            target = np.zeros(shape=len(output), dtype=KIA_DTYPE)
            target[:]["timestamp"] = output["timestamp"]
            target[:]["command"] = output["command"]
            target[:]["key"] = output["key"]
            target[:]["size"] = output["size"]
            target[:]["ttl"] = output["ttl"]
            target.tofile(output_file)
        case "Sari":
            raise NotImplementedError("Sari's format is not currently supported")
        case _:
            raise ValueError(f"unsupported format '{binary_format}'")


def compare_binary(oracle_file: str, test_file: str, binary_format: str):
    """Compare the results of two binary files."""
    dtype = {"Kia": KIA_DTYPE}.get(binary_format)
    oracle = np.fromfile(oracle_file, dtype=dtype)
    test = np.fromfile(test_file, dtype=dtype)
    np.testing.assert_array_equal(oracle, test)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-file", nargs="+", type=str, required=True)
    parser.add_argument("--input-format", choices=INPUT_FORMATS, required=True)
    parser.add_argument("--output-file", type=str, required=True)
    parser.add_argument("--output-format", choices=OUTPUT_FORMATS)
    parser.add_argument("--sort-by-time", action="store_true")
    parser.add_argument("--oracle", type=str, default=None)
    args = parser.parse_args()

    output = []
    for fname in args.input_file:
        output.append(parse_file(fname, args.input_format))
    output = pd.concat(output, ignore_index=True)

    if args.sort_by_time:
        output = output.sort_values(by="timestamp", ascending=True)

    save_binary(args.output_file, output, args.output_format)

    if args.oracle is not None:
        compare_binary(args.oracle, args.output_file, args.output_format)


if __name__ == "__main__":
    main()
