"""
@brief  Set the size of a trace to a constant.

@details    Kia supports variable sizes in his MRCs and his images
            reflect that. This script creates traces with a fixed size
            so that we can generate oracles that reflect my MRCs.
"""

import argparse
import os

import numpy as np


FORMATS = {
    "Kia": np.dtype(
        [
            ("timestamp", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size", np.uint32),
            ("ttl", np.uint32),
        ]
    ),
    "Sari": np.dtype(
        [
            ("timestamp", np.uint32),
            ("key", np.uint64),
            ("size", np.uint32),
            ("eviction-time", np.uint32),
        ]
    ),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=str, required=True, help="input path")
    parser.add_argument(
        "--output", "-o", type=str, required=True, help="output path (cannot exist)"
    )
    parser.add_argument(
        "--format", "-f", choices=["Kia", "Sari"], default="Kia", help="Data format"
    )
    parser.add_argument("--size", type=int, default=1, help="new trace size")
    args = parser.parse_args()

    if args.input == args.output:
        raise ValueError(
            f"input path '{args.input}' must not match output path '{args.output}'"
        )
    if os.path.exists(args.output):
        raise ValueError(f"output path '{args.output}' already exists")

    format = FORMATS.get(args.format, None)
    if format is None:
        raise ValueError(
            f"invalid format '{args.format}', expected {list(FORMATS.keys())}"
        )

    with open(args.input) as f:
        data = np.fromfile(f, dtype=format)
    data[:]["size"] = args.size

    with open(args.output, "wb") as f:
        data.tofile(f)


if __name__ == "__main__":
    main()
