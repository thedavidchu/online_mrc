#!/usr/bin/python3
import argparse
import os
import shutil
from getpass import getpass
from pathlib import Path
from shlex import split, quote
from subprocess import run, CompletedProcess
from warnings import warn

EXE = "/home/david/projects/online_mrc/build/src/run/generate_mrc_exe"


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


def practice_sh(cmd: str, **kwargs) -> CompletedProcess:
    # Adding the '--help' flag should call the executable but return
    # very quickly!
    return sh(cmd=f"{cmd} --help", **kwargs)


def setup_env(
    *,
    output_dir: Path | None = None,
    setup_output: bool = True,
    setup_build: bool = True,
):
    top_level_dir = sh("git rev-parse --show-toplevel").stdout.strip()
    if setup_output:
        if os.path.exists(output_dir):
            raise FileExistsError(f"{str(output_dir)} exists")
        os.mkdir(output_dir)
        os.mkdir(output_dir / "mrc")
        os.mkdir(output_dir / "hist")
        os.mkdir(output_dir / "log")
        os.mkdir(output_dir / "plot")
        os.mkdir(output_dir / "stats")
    if setup_build:
        build_dir = os.path.join(top_level_dir, "build")
        os.chdir(build_dir)
        sh("meson setup --wipe")
        sh("meson compile")
    os.chdir(top_level_dir)


def get_file_tree(path: Path | list[Path]) -> list[Path]:
    """Return a list of all the files belonging to a directory."""
    if isinstance(path, list):
        return [file for dir_or_file in path for file in get_file_tree(dir_or_file)]
    if os.path.isfile(path):
        return [path]
    return [
        Path(os.path.join(dirpath, f)).absolute()
        for dirpath, dirnames, filenames in os.walk(path)
        for f in filenames
    ]


def filter_files_by_extension(files: list[Path], extensions: list[str]) -> list[Path]:
    return [f for f in files if f.suffix in extensions]


def sort_files_by_size(files: list[Path]) -> list[Path]:
    """Return a list of the smallest sizes first."""
    return sorted(files, key=lambda f: os.path.getsize(f))


def get_filtered_sorted_files(path: Path, extensions: list[str]) -> list[Path]:
    files = get_file_tree(path)
    files = filter_files_by_extension(files, extensions)
    files = sort_files_by_size(files)
    return files


def run_trace(
    input_: Path,
    format: str,
    output_: Path,
    run_oracle: bool,
):
    def mrc(algo: str):
        return os.path.join(str(output_), "mrc", f"{input_.stem}-{algo}-mrc.bin")

    def hist(algo: str):
        return os.path.join(str(output_), "hist", f"{input_.stem}-{algo}-hist.bin")

    def stats(algo: str):
        return output_ / "stats" / f"{input_.stem}-{algo}-stats.bin"

    log = sh(
        f"{EXE} "
        f"--input {input_} "
        f"--format {format} "
        + (
            f'--oracle "Olken(mrc={mrc("Olken")},hist={hist("Olken")},stats_path={stats("Olken")})" '
            if run_oracle
            else ""
        )
        + f'--run "Fixed-Rate-SHARDS(mrc={mrc("Fixed-Rate-SHARDS")},hist={hist("Fixed-Rate-SHARDS")},sampling=1e-3,adj=true,stats_path={stats("Fixed-Rate-SHARDS")})" '
        f'--run "Evicting-Map(mrc={mrc("Evicting-Map")},hist={hist("Evicting-Map")},sampling=1e-1,max_size=8192,stats_path={stats("Evicting-Map")})" '
        f'--run "Fixed-Size-SHARDS(mrc={mrc("Fixed-Size-SHARDS")},hist={hist("Fixed-Size-SHARDS")},sampling=1e-1,max_size=8192,stats_path={stats("Fixed-Size-SHARDS")})" '
    )

    with open(os.path.join(output_, "log", f"{input_.stem}.log"), "w") as f:
        # I write the stderr first since it is flushed first.
        f.write(log.stderr)
        f.write(log.stdout)


def plot_mrc(f: Path, mrc_dir: Path, plot_dir: Path):
    oracle_path = mrc_dir / f"{f.stem}-Olken-mrc.bin"
    frs_path = mrc_dir / f"{f.stem}-Fixed-Rate-SHARDS-mrc.bin"
    emap_path = mrc_dir / f"{f.stem}-Evicting-Map-mrc.bin"
    fss_path = mrc_dir / f"{f.stem}-Fixed-Size-SHARDS-mrc.bin"
    output_path = plot_dir / f"{f.stem}-mrc.pdf"
    sh(
        f"python3 src/analysis/plot/plot_mrc.py "
        f"--oracle {oracle_path} "
        f"--input {frs_path} {emap_path} {fss_path} "
        f"--output {output_path}"
    )


def analyze_log(output_dir: Path, log_dir: Path):
    r = sh(
        f"python3 src/analysis/log/analyze_log.py "
        f"--input {str(log_dir)} "
        f"--time {str(output_dir / 'time.pdf')} "
        f"--olken-time {str(output_dir / 'olken-time.pdf')} "
        f"--trace-time {str(output_dir / 'trace-time.pdf')} "
        f"--accuracy {str(output_dir / 'accuracy.pdf')} "
    )
    r.check_returncode()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input", "-i", type=Path, required=True, help="directory of inputs"
    )
    parser.add_argument(
        "--extensions",
        "-e",
        nargs="*",
        type=str,
        default=[".bin"],
        help="list of extensions to include (e.g. '.bin .dat')",
    )
    parser.add_argument(
        "--output", "-o", type=Path, required=True, help="directory for outputs"
    )
    parser.add_argument(
        "--format",
        "-f",
        choices=["Kia", "Sari"],
        required=True,
        help="format of the input traces",
    )
    parser.add_argument(
        "--skip-oracle", action="store_true", help="skip running the oracle"
    )
    parser.add_argument("--sudo", action="store_true", help="run as sudo")
    parser.add_argument(
        "--run-only-plot-mrc", action="store_true", help="run MRC plotters"
    )
    parser.add_argument(
        "--run-only-analyze-log", action="store_true", help="run log analyzer"
    )
    parser.add_argument(
        "--overwrite", action="store_true", help="overwrite ALL CONTENTS in output file"
    )
    args = parser.parse_args()

    if args.sudo:
        warn("the 'sudo' option doesn't do anything... yet!")
        # NOTE  I try to replicate sudo's interface.
        sudo_password = getpass(prompt=f"[sudo] password for {os.getlogin()}: ")
        # NOTE  You need to quote the password to prevent arbitrary
        #       string injection attacks.
        sh(f"echo {quote( sudo_password )} | sudo -S echo Testing sudo password")
    if args.overwrite and args.output.exists():
        shutil.rmtree(args.output)

    # NOTE  We run the traces unless explicitly told by the user to only
    #       run the plotter or analyzer.
    run_traces: bool = not (args.run_only_plot_mrc or args.run_only_analyze_log)
    setup_env(
        output_dir=args.output,
        setup_output=run_traces,
        setup_build=run_traces,
    )
    # Recall Python's pathlib syntax for concatenating file names. Yes,
    # it's weird. And yes, I don't actually need this comment.
    hist_dir = args.output / "hist"
    log_dir = args.output / "log"
    mrc_dir = args.output / "mrc"
    plot_dir = args.output / "plot"

    files = get_filtered_sorted_files(args.input, args.extensions)
    if not files:
        warn(f"no files in {str(args.input)}")

    if run_traces:
        for f in files:
            run_trace(f, args.format, args.output, not args.skip_oracle)
    if args.run_only_plot_mrc:
        for f in files:
            plot_mrc(f, mrc_dir, plot_dir)
    if args.run_only_analyze_log:
        analyze_log(args.output, log_dir)


if __name__ == "__main__":
    main()
