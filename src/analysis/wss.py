#!/usr/bin/python3
"""
Calculate the working set size of a trace.

@note   I use pure Python to calculate the working set sizes, so be
        careful on large traces!
@example    `python3 -m src.analysis.wss [...args]`
"""

import argparse
from pathlib import Path

from tqdm import tqdm
import numpy as np

from src.analysis.common.trace import TRACE_DTYPE
from src.analysis.common.common import format_memory_size as fmtmem


def wss(path: Path, format: str) -> dict[str, float]:
    x = np.fromfile(path, dtype=TRACE_DTYPE[format])
    size = path.stat().st_size
    print(f"File size {str(path)}: {size} ({fmtmem(size)})")
    if format == "Kia":
        # Ignore accesses whose size is zero.
        x = x[np.logical_and(x[:]["command"] == 0, x[:]["size_b"] != 0)]

    # I separate the computationally cheap statistics from the expensive ones.
    num_accesses = len(x)
    print(f"Number accesses: {num_accesses}")
    num_unique_keys = len(np.unique(x[:]["key"]))
    print(f"Number unique objects: {num_unique_keys}")

    bytes_requested = np.sum(x[:]["size_b"])
    print(f"Total bytes requested: {bytes_requested} ({fmtmem(bytes_requested)})")

    # Get working set sizes (may take a while!).
    max_cache_size = {}
    first_cache_size = {}
    last_cache_size = {}
    for obj in tqdm(x):
        key = obj["key"]
        size = obj["size_b"]
        max_cache_size[key] = max(max_cache_size.get(key, 0), size)
        last_cache_size[key] = size
        if key not in first_cache_size:
            first_cache_size[key] = size
    real_wss = sum(max_cache_size.values())
    print(f"Working set size [bytes]: {real_wss} ({fmtmem(real_wss)})")
    first_wss = sum(first_cache_size.values())
    print(f"Naive WSS (based on first size) [bytes]: {first_wss} ({fmtmem(first_wss)})")
    last_wss = sum(last_cache_size.values())
    print(f"Naive WSS (based on last size) [bytes]: {last_wss} ({fmtmem(last_wss)})")

    return dict(
        num_accesses=num_accesses,
        num_unique_keys=num_unique_keys,
        bytes_requested=bytes_requested,
        real_wss=real_wss,
        first_wss=first_wss,
        last_wss=last_wss,
    )


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

    _ = wss(args.input, args.format)
    print(_)


if __name__ == "__main__":
    main()
