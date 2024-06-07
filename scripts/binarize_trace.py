"""Convert a trace into Kia's data type"""

#!/usr/bin/python3

import argparse
import os

import numpy as np
import pandas as pd

INPUT_FORMATS = ["MSR"]
OUTPUT_FORMATS = ["Kia"]


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


def parse_file(fname: str, input_format: str):
    if not os.path.exists(fname):
        raise ValueError(f"file '{fname}' does not exist")
    if not os.path.isfile(fname):
        raise ValueError(f"file '{fname}' is not a file")
    if input_format not in INPUT_FORMATS:
        raise ValueError(f"got {input_format}, expected {INPUT_FORMATS}")
    match input_format:
        case "MSR":
            parsed_lines = msr_parser(fname)
        case _:
            raise ValueError(f"unrecognized input format: {input_format}")
    return parsed_lines


def save_binary(output_file: str, binary: pd.DataFrame, output_format: str):
    kia_dtype = np.dtype(
        [
            ("timestamp", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size", np.uint32),
            ("ttl", np.uint32),
        ]
    )
    if output_format not in OUTPUT_FORMATS:
        raise ValueError(f"got {output_format}, expected {OUTPUT_FORMATS}")
    match output_format:
        case "Kia":
            output = binary.to_numpy(dtype=kia_dtype)
            print(output)
            bytes = output[0].tobytes()
            print(output[0], bytes)
            print(len(bytes))
            output.tofile(output_file)
            pass
        case "Sari":
            raise NotImplementedError("Sari's format is not currently supported")
        case _:
            raise ValueError(f"unsupported format '{output_format}'")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-file", nargs="+", type=str, required=True)
    parser.add_argument("--input-format", choices=INPUT_FORMATS, required=True)
    parser.add_argument("--output-file", type=str, required=True)
    parser.add_argument("--output-format", choices=OUTPUT_FORMATS)
    parser.add_argument("--sort-by-time", action="store_true")
    args = parser.parse_args()

    binaries = []
    for fname in args.input_file:
        binaries.append(parse_file(fname, args.input_format))

    binary = pd.concat(binaries, ignore_index=True)

    if args.sort_by_time:
        binary.sort_values(by="timestamp", ascending=True)

    save_binary(args.output_file, binary, args.output_format)


if __name__ == "__main__":
    main()
