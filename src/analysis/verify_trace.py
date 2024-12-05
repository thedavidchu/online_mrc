"""
Verify a trace is as expected.
    - Non-decreasing time stamps

@note   I use pure Python, so be careful on large traces!
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

    # Check for non-decreasing timestamps
    tm_cmp = x[:-1]["timestamp"] <= x[1:]["timestamp"]
    non_decreasing_timestamps = np.all(tm_cmp)
    print(f"Non-decreasing timestamps: {non_decreasing_timestamps}")
    if not non_decreasing_timestamps:
        for i, y in enumerate(tm_cmp):
            if not y:
                print(
                    f"Non-decreasing at positions {i}, {i+1}: {x[i]['timestamp']} vs {x[i+1]['timestamp']}"
                )


if __name__ == "__main__":
    main()
