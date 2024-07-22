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
    print(f"Working set size: {len({i for i in tqdm(x[:]["key"])})}")


if __name__ == "__main__":
    main()
