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
from src.analysis.common.trace import (
    read_trace,
    TRACE_DTYPE_KEY,
    KIA_OP_NAMES,
    YANG_OP_NAMES,
)


def print_header(format: str):
    if format == "Kia":
        print(
            f"| {'ID':<5} | {'Position':<10} | {'Timestamp [ms]':<20} | Command | {'Key':<20} | {'Size [B]':<10} | {'TTL [s]':<10} |"
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
    elif format == "YangTwitterX":
        print(
            f"| {'ID':<5} | {'Position':<10} | {'Timestamp [ms]':<20} | Command | {'Key':<20} | {'Key Size [B]':<12} | {'Value Size [B]':<14} | {'TTL [s]':<10} | {'Client ID':<10} |"
        )
        print(
            f"|-------|------------|----------------------|---------|----------------------|--------------|----------------|------------|------------|"
        )
    else:
        raise ValueError(f"unrecognized format {format}")


def print_access(index: int, position: int, access: np.ndarray, format: str):
    if format == "Kia":
        cmd = KIA_OP_NAMES[access["command"]]
        print(
            f"| {index:>5} | {position:>10} | {access['timestamp_ms']:>20} | {cmd:>7} | {access['key']:>20} | {access['size_b']:>10} | {access['ttl_s']:>10} |"
        )
    elif format == "Sari":
        print(
            f"| {index:>5} | {position:>10} | {access['timestamp_s']:>20} | {access['key']:>20} | {access['size_b']:>10} | {access['ttl_s']:>10} |"
        )
    elif format == "YangTwitterX":
        key_size_b = access["key_value_size"] >> 22
        value_size_b = access["key_value_size"] & 0x003F_FFFF
        op = YANG_OP_NAMES[access["op_ttl_s"] >> 24]
        ttl_s = access["op_ttl_s"] & 0x00FF_FFFF
        print(
            f"| {index:>5} | {position:>10} | {access['timestamp_ms']:>20} | {op:>7} | {access['key']:>20} | {key_size_b:>12} | {value_size_b:>14} | {ttl_s:>10} | {access['client_id']:>10} |"
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
        choices=TRACE_DTYPE_KEY,
        default=TRACE_DTYPE_KEY[0],
        help=f"format of the input trace. Options: {'|'.join(TRACE_DTYPE_KEY)}",
    )
    parser.add_argument(
        "--start",
        "-s",
        type=int,
        default=0,
        help="index to begin (may be negative, but no less than -(length of trace)). Default: 0.",
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
