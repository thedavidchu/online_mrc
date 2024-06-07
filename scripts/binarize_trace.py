"""Convert a trace into Kia's data type"""

#!/usr/bin/python3

import argparse
import os

import numpy as np
import pandas as pd

kia_dtype = np.dtype(
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
        usecols=[
            "timestamp",
            "command",
            "key",
            "size",
        ],
        dtype=np.dtype(
            [
                ("timestamp", np.uint64),
                ("workload", str),
                ("_unknown_0", str),
                ("command", np.uint8),  # Note: this is overwritten by the converter
                ("key", np.uint64),
                ("size", np.uint32),
                ("_unknwon_1", str),
            ]
        ),
        converters={"command": lambda x: 0 if x == "Read" else 1},
        on_bad_lines="error",
    )

    df["command"] = df["command"].astype(np.uint8)
    df["ttl"] = np.zeros_like(df["timestamp"], dtype=np.uint32)
    print(df, df.dtypes)
    return df


INPUT_FORMATS = {"MSR": msr_parser}


def parse_file(fname: str, input_format: str):
    if not os.path.exists(fname):
        raise ValueError(f"file '{fname}' does not exist")
    if not os.path.isfile(fname):
        raise ValueError(f"file '{fname}' is not a file")
    parser = INPUT_FORMATS.get(input_format, None)
    if parser is None:
        raise ValueError(f"unrecognized input format '{input_format}'")
    parsed_lines = parser(fname)
    return parsed_lines


def save_binary(output_file: str, binary: pd.DataFrame, output_format: str):
    match output_format:
        case "Kia":
            # TODO
            output = binary.to_numpy(dtype=kia_dtype)
            bytes = output[0].tobytes()
            print(output[0], bytes)
            print(len(bytes))
            output.tofile(output_file)
            pass
        case "Sari":
            raise ValueError("Sari's format is not currently supported")
        case _:
            raise ValueError(f"unsupported format '{output_format}'")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input-file", nargs="+", type=str, required=True)
    parser.add_argument("--input-format", choices=INPUT_FORMATS.keys(), required=True)
    parser.add_argument("--output-file", type=str, required=True)
    parser.add_argument("--output-format", choices=["Kia"])
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
