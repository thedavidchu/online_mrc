#!/usr/bin/python3
"""
@brief  Generate the oracle for a directory of traces.

Usage: run this file with `nohup python3 <path-to-create_oracles.py>`
"""
import argparse
import os
from pathlib import Path
from multiprocessing import Pool
from subprocess import run

EXECUTABLE = (
    "/home/david/projects/online_mrc/build/src/analysis/mrc/generate_oracle_exe"
)


def setup_env():
    top_level_dir = run(
        "git rev-parse --show-toplevel".split(),
        capture_output=True,
        timeout=60,
        text=True,
    ).stdout.strip()
    build_dir = os.path.join(top_level_dir, "build")
    os.chdir(build_dir)
    run("meson compile".split())


def run_trace(
    input_abspath: str, format: str, output_abspath: str, stem: str, overwrite: bool
):
    print(f"Started {stem}.bin")
    trace_abspath = os.path.join(input_abspath, f"{stem}.bin")
    hist_abspath = os.path.join(output_abspath, f"{stem}-olken-histogram.bin")
    mrc_abspath = os.path.join(output_abspath, f"{stem}-olken-mrc.bin")
    log_abspath = os.path.join(output_abspath, f"{stem}-olken.log")
    if all(map(os.path.exists, [hist_abspath, mrc_abspath, log_abspath])):
        if not overwrite:
            print(f"Skipped {stem}.bin")
            return
        else:
            print(f"All outputs for {stem}.bin already exist (running anyway)")
    elif any(map(os.path.exists, [hist_abspath, mrc_abspath, log_abspath])):
        print(f"Some outputs for {stem}.bin already exist (running anyway)")
    else:
        print(f"No outputs for {stem}.bin exist (running)")
    output = run(
        f"nohup {EXECUTABLE} -i {trace_abspath} -f {format} -h {hist_abspath} -m {mrc_abspath}".split(),
        capture_output=True,
        text=True,
    )
    with open(log_abspath, "w") as f:
        f.write(output.stdout)
    print(f"Finished {stem}.bin")


def get_stems(input_path: Path, exclude: str) -> list[str]:
    paths = os.listdir(input_path)
    roots = [root for root, ext in [os.path.splitext(path) for path in paths]]
    stems = [os.path.basename(root) for root in roots if root not in exclude.split(",")]
    return stems


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input", "-i", type=Path, help="directory of inputs")
    parser.add_argument(
        "--exclude",
        "-e",
        type=str,
        default=None,
        help="comma separated pattern of stems to exclude (e.g. 'junk,garbage')",
    )
    parser.add_argument("--output", "-o", type=Path, help="directory for outputs")
    parser.add_argument(
        "--format",
        "-f",
        choices=["Kia", "Sari"],
        required=True,
        help="format of the input traces",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="overwrite files even if all of the outputs exist",
    )
    args = parser.parse_args()
    stems = get_stems(args.input, args.exclude)
    print(stems)
    input_abspath, output_abspath = str(args.input), str(args.output)
    with Pool(8) as p:
        p.starmap(
            run_trace,
            [
                (input_abspath, args.format, output_abspath, stem, args.overwrite)
                for stem in stems
            ],
        )


if __name__ == "__main__":
    main()
