"""
@brief  Create a trace from a CSV file.
"""

import argparse
import csv
import struct
from pathlib import Path

import numpy as np


def sari_csv_to_bin(args: list[str]) -> np.ndarray:
    if len(args) != 4:
        raise ValueError(f"got '{','.join(args)}', expected 'tm[s],key,size[B],TTL[s]'")
    timestamp_s, key, size_b, ttl_s = args
    return struct.pack("<IQII", int(timestamp_s), int(key), int(size_b), int(ttl_s))


def parse_sari_file(ipath: Path, opath: Path):
    with open(ipath) as f:
        r = [sari_csv_to_bin(row) for row in csv.reader(f)]

    with open(opath, "wb") as f:
        for row in r:
            f.write(row)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--format", "-f", choices=["Sari"], default="Sari")
    parser.add_argument("--input", "-i", type=Path, help="input path")
    parser.add_argument("--output", "-o", type=Path, help="output path")
    args = parser.parse_args()

    if not args.input.exists():
        raise FileNotFoundError(f"'{str(args.input)}' DNE")
    if args.output.exists():
        raise FileExistsError(f"'{str(args.output)}' already exists!")
    match args.format:
        case "Sari":
            parse_sari_file(args.input, args.output)
        case _:
            raise ValueError(f"unexpected format: '{args.format}'")


if __name__ == "__main__":
    main()
