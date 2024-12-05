"""
Calculate the working set size of a trace.

@note   I use pure Python to calculate the working set sizes, so be
        careful on large traces!
"""

import argparse
from pathlib import Path

from tqdm import tqdm
import numpy as np

TRACE_DTYPES: dict[str, np.dtype] = {
    "Kia": np.dtype(
        [
            ("timestamp", np.uint64),
            ("command", np.uint8),
            ("key", np.uint64),
            ("size", np.uint32),
            ("ttl", np.uint32),
        ],
    ),
    # As per https://github.com/SariSultan/TTLsMatter-EuroSys24
    "Sari": np.dtype(
        [
            ("timestamp", np.uint32),
            ("key", np.uint64),
            ("size", np.uint32),
            ("eviction_time", np.uint32),
        ]
    ),
}


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", type=Path, required=True, help="input trace path"
    )
    parser.add_argument(
        "--format",
        "-f",
        type=str,
        choices=["Kia", "Sari"],
        default="Kia",
        help="input trace format",
    )
    args = parser.parse_args()

    x = np.fromfile(args.input, dtype=TRACE_DTYPES[args.format])
    if args.format == "Kia":
        x = x[x[:]["command"] == 0]

    # I separate the computationally cheap statistics from the expensive ones.
    print(f"Number accesses: {len(x)}")
    print(f"Number unique objects: {len(np.unique(x[:]["key"]))}")
    print(f"Total bytes requested: {np.sum(x[:]["size"])}")

    # Get working set sizes (may take a while!).
    max_cache_size = {}
    first_cache_size = {}
    last_cache_size = {}
    for obj in tqdm(x):
        key = obj["key"]
        size = obj["size"]
        max_cache_size[key] = max(max_cache_size.get(key, 0), size)
        last_cache_size[key] = size
        if key not in first_cache_size:
            first_cache_size[key] = size
    print(f"Working set size [bytes]: {sum(max_cache_size.values())}")
    print(f"Naive WSS (based on first size) [bytes]: {sum(first_cache_size.values())}")
    print(f"Naive WSS (based on last size) [bytes]: {sum(last_cache_size.values())}")


if __name__ == "__main__":
    main()
