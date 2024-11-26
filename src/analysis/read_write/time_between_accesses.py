"""Create a histogram of the time between accesses."""

import argparse
from subprocess import run, CompletedProcess
from shlex import split, quote
from pathlib import Path

EXE = "/home/david/projects/online_mrc/build/src/analysis/read_write/time_between_accesses_exe"


def sh(cmd: str, **kwargs) -> CompletedProcess:
    """
    Automatically run nohup on every script. This is because I have
    a bad habit of killing my scripts on hangup.

    @note   I will echo the commands to terminal, so don't stick your
            password in commands, obviously!
    """
    my_cmd = f"nohup {cmd}"
    print(f"Running: '{my_cmd}'")
    r = run(split(my_cmd), capture_output=True, text=True, **kwargs)
    if r.returncode != 0:
        print(
            "=============================================================================="
        )
        print(f"Return code: {r.returncode}")
        print(
            "----------------------------------- stderr -----------------------------------"
        )
        print(r.stderr)
        print(
            "----------------------------------- stdout -----------------------------------"
        )
        print(r.stdout)
        print(
            "=============================================================================="
        )
    return r


def run_plotter_over_dir(trace_dir: Path, plot_dir: Path, trace_format: str):
    if not trace_dir.is_dir():
        raise FileNotFoundError(f"trace directory '{str(trace_dir)}' DNE")
    if not plot_dir.is_dir():
        raise FileNotFoundError(f"plot directory '{str(plot_dir)}' DNE")
    # Perform a single-level walk of the trace directory. Sort by file size.
    trace_paths = sorted(
        [child for child in trace_dir.iterdir() if child.is_file()],
        key=lambda p: p.stat().st_size,
    )

    for i, trace_path in enumerate(trace_paths):
        print(f"Working on {i}: {str(trace_path)}")
        full_hist_path = plot_dir / f"{trace_path.stem}-full-hist.bin"
        rd_hist_path = plot_dir / f"{trace_path.stem}-read-hist.bin"
        wr_hist_path = plot_dir / f"{trace_path.stem}-write-hist.bin"
        sh(
            f"{EXE} "
            f"--trace {trace_path} "
            f"--full-histogram {full_hist_path} "
            f"--read-histogram {rd_hist_path} "
            f"--write-histogram {wr_hist_path} "
            f"--format {trace_format} "
        )

        plot_path = plot_dir / f"{trace_path.stem}-histogram.pdf"
        sh(
            f"python3 ./src/analysis/plot/plot_histogram.py "
            f"--input-paths {full_hist_path} {rd_hist_path} {wr_hist_path} "
            f"--output-path {plot_path} "
            f"--logx --logy "
        )


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--trace-dir",
        "-t",
        type=Path,
        required=True,
        help="path to input directory with traces",
    )
    parser.add_argument(
        "--plot-dir",
        "-p",
        type=Path,
        required=True,
        help="path to output directory for plots",
    )
    parser.add_argument(
        "--format",
        "-f",
        type=str,
        choices=["Kia", "Sari"],
        default="Kia",
        help="trace format",
    )
    args = parser.parse_args()
    sh("cd $(git rev-parse --show-toplevel)")
    run_plotter_over_dir(args.trace_dir, args.plot_dir, args.format)


if __name__ == "__main__":
    main()
