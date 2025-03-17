#!/usr/bin/python3
"""
@brief  Print the contents of a trace in human readable form.

@example    `python3 -m src.analysis.print_trace -i <input> -f <format>`

# TODO - Make ID formattable
# TODO - Add position parameter
"""
import argparse
import numpy as np
from pathlib import Path

from src.analysis.common.common import abspath
from src.analysis.common.trace import read_trace


def print_header(format: str):
    if format == "Kia":
        print(
            f"| {'ID':<5} | {'Position':<10} | {'Timestamp [s]':<20} | Command | {'Key':<20} | {'Size [B]':<10} | {'TTL [s]':<10} |"
        )
        print(
            f"|-------|------------|----------------------|---------|----------------------|------------|------------|"
        )
    elif format == "Sari":
        print(
            f"| {'ID':<5} | {'Position':<10} | {'Timestamp [s]':<20} | {'Key':<20} | {'Size [B]':<10} | {'TTL [s]':<10} |"
        )
        print(
            f"|-------|------------|----------------------|----------------------|------------|------------|"
        )
    else:
        raise ValueError(f"unrecognized format {format}")


def print_access(index: int, position: int, access: np.ndarray, format: str):
    if format == "Kia":
        print(
            f"| {index:>5} | {position:>10} | {access['timestamp_ms']:>20} | {access['command']:>7} | {access['key']:>20} | {access['size_b']:>10} | {access['ttl_s']:>10} |"
        )
    elif format == "Sari":
        print(
            f"| {index:>5} | {position:>10} | {access['timestamp_s']:>20} | {access['key']:>20} | {access['size_b']:>10} | {access['ttl_s']:>10} |"
        )
    else:
        raise ValueError(f"unrecognized format {format}")


def print_trace(path: Path, format: str, start: int, length: int):
    trace = read_trace(path, format, "r")
    print_header(format)

    assert start + len(trace) >= 0
    assert start + length + len(trace) >= 0

    begin = (start + len(trace)) % len(trace)
    end = begin + length
    for i, access in enumerate(trace[begin:end]):
        print_access(i, begin + i, access, format)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=abspath, help="path to the input trace")
    parser.add_argument(
        "--format",
        "-f",
        choices=["Kia", "Sari"],
        default="Kia",
        help="format of the input trace. Options: Kia|Sari",
    )
    parser.add_argument(
        "--start", "-s", type=int, default=0, help="index to begin. Default: 0."
    )
    parser.add_argument(
        "--length", "-l", type=int, default=10, help="length to print. Default: 10."
    )
    args = parser.parse_args()

    if args.length < 0:
        raise ValueError("length must be non-negative")
    print_trace(args.input, args.format, args.start, args.length)


if __name__ == "__main__":
    main()
