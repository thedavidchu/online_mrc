"""
Verify a trace is as expected.
    - Non-decreasing time stamps

@note   I use pure Python, so be careful on large traces!
"""

import argparse
from pathlib import Path

import numpy as np

from src.analysis.common.trace import TRACE_DTYPE


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
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="verbosely print the mismatches"
    )
    args = parser.parse_args()

    x = np.fromfile(args.input, dtype=TRACE_DTYPE[args.format])

    # Check for non-decreasing timestamps
    tmstr = {"Kia": "timestamp_ms", "Sari": "timestamp_s"}[args.format]
    ok_tm = x[:-1][tmstr] <= x[1:][tmstr]
    cnt_ok_tm = np.count_nonzero(ok_tm)
    # There is one fewer comparison than there are accesses, so that is
    # why we subtract 1 from the number of accesses.
    cnt_decr_tm = len(x) - cnt_ok_tm - 1
    print(f"Decreasing timestamps: {cnt_decr_tm}")
    if args.verbose and cnt_decr_tm:
        cnt = 0
        max_diff = 0
        sum_diff = 0
        arg_max = (0, 0)
        print("Decreasing Timestamps:")
        for pos, y in enumerate(ok_tm):
            if not y:
                diff = x[pos][tmstr] - x[pos + 1][tmstr]
                sum_diff += diff
                if diff > max_diff:
                    max_diff = diff
                    arg_max = (pos, pos + 1)
                cnt += 1
                # print(
                #     f"[{cnt}/{cnt_decr_tm}] At positions {pos}, {pos+1}: {x[pos][tmstr]} > {x[pos+1][tmstr]}"
                # )
        print(
            f"Max Diff: {max_diff} from {x[arg_max[0]][tmstr]} and {x[arg_max[1]][tmstr]} (at {arg_max})"
        )
        print(f"Mean Diff: {sum_diff/cnt_decr_tm}")
        print(f"Ratio decreasing: {cnt_decr_tm/len(x)}")


if __name__ == "__main__":
    main()
