#!/usr/bin/python3
"""
@brief  Generate the oracle for a directory of traces.

@example    Here's an example of how to call this file:
`nohup python3 <this-file> -i /mnt/disk1-20tb/Traces/FilteredTwitter_Binary/ -o /mnt/disk1-20tb/same_size_oracle/twitter/ -f Sari -e hashes`
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
            print(f"All outputs for {stem}.bin already exist (skipping)")
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
    """
    @brief  Get the stems from the input path.

    @details    I define a stem as follows:
                ```
                "/my/path/to/a/file.txt"
                               ^^^^-------- this is the "stem".
                ```
    @note   I only process the leaf files of the input_path
    """
    # NOTE  I sort from smallest to largest so that we can quickly see
    #       if the script works!
    paths = sorted(
        [os.path.join(str(input_path), path) for path in os.listdir(input_path)],
        key=lambda f: os.path.getsize(f),
    )
    # I filter paths that are not leaf files because we obviously cannot
    # read from those paths.
    paths = [path for path in paths if os.path.isfile(path)]
    roots = [root for root, ext in [os.path.splitext(path) for path in paths]]
    stems = [os.path.basename(root) for root in roots]
    filtered_stems = [stem for stem in stems if stem not in exclude.split(",")]
    return filtered_stems


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
    for stem in stems:
        # NOTE  I was having trouble with 'nohup' and child processes,
        #       so I decided to just run it all single threaded because
        #       I have all weekend to run it, yay!
        run_trace(input_abspath, args.format, output_abspath, stem, args.overwrite)


if __name__ == "__main__":
    main()
