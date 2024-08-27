#!/usr/bin/python3
import argparse
import os
from pathlib import Path
from shlex import split
from subprocess import run, CompletedProcess

EXE = "/home/david/projects/online_mrc/build/src/run/generate_mrc_exe"


def sh(cmd: str, **kwargs) -> CompletedProcess[str]:
    """Automatically run nohup on every script. This is because I have
    a bad habit of killing my scripts on hangup."""
    return run(split(f"nohup {cmd}"), capture_output=True, text=True, **kwargs)


def practice_sh(cmd: str, **kwargs) -> CompletedProcess[str]:
    # Adding the '--help' flag should call the executable but return
    # very quickly!
    return sh(cmd=f"{cmd} --help", **kwargs)


def setup_env(output_dir: Path):
    top_level_dir = sh("git rev-parse --show-toplevel", timeout=60).stdout.strip()
    build_dir = os.path.join(top_level_dir, "build")

    # Setup output_dir
    if os.path.exists(output_dir) and os.listdir(output_dir):
        raise FileExistsError("cannot overwrite old data")
    if not os.path.exists(output_dir):
        os.mkdir(output_dir)
    os.mkdir(os.path.join(output_dir, "mrc"))
    os.mkdir(os.path.join(output_dir, "hist"))
    os.mkdir(os.path.join(output_dir, "log"))
    os.mkdir(os.path.join(output_dir, "plot"))

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


def run_trace(
    input_: Path,
    format: str,
    output_: Path,
):
    def mrc(algo: str):
        return os.path.join(str(output_), "mrc", f"{input_.stem}-{algo}-mrc.bin")

    def hist(algo: str):
        return os.path.join(str(output_), "hist", f"{input_.stem}-{algo}-hist.bin")

    log = sh(
        f"{EXE} "
        f"--input {input_} "
        f"--format {format} "
        f'--oracle "Olken(mrc={mrc("Olken")},hist={hist("Olken")})" '
        f'--run "Fixed-Rate-SHARDS(mrc={mrc("Fixed-Rate-SHARDS")},hist={hist("Fixed-Rate-SHARDS")},sampling=1e-3,adj=true)" '
        f'--run "Evicting-Map(mrc={mrc("Evicting-Map")},hist={hist("Evicting-Map")},sampling=1e-1,max_size=8192)" '
        f'--run "Fixed-Size-SHARDS(mrc={mrc("Fixed-Size-SHARDS")},hist={hist("Fixed-Size-SHARDS")},sampling=1e-1,max_size=8192)" '
    )

    with open(os.path.join(output_, "log", f"{input_.stem}.log"), "w") as f:
        # I write the stderr first since it is flushed first.
        f.write(log.stderr)
        f.write(log.stdout)


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
    args = parser.parse_args()

    setup_env(args.output)

    files = get_file_tree(args.input)
    files = filter_files_by_extension(files, args.extensions)
    files = sort_files_by_size(files)

    for f in files:
        run_trace(f, args.format, args.output)

    # Recall Python's pathlib syntax for concatenating file names. Yes,
    # it's weird. And yes, I don't actually need this comment.
    hist_dir = args.output / "hist"
    log_dir = args.output / "log"
    mrc_dir = args.output / "mrc"
    plot_dir = args.output / "plot"

    print(f"{hist_dir, log_dir, mrc_dir, plot_dir}")

    for f in files:
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

    sh(
        f"python3 src/analysis/log/analyze_log.py "
        f"--input {log_dir} "
        f"--time {args.output / 'time.pdf'} "
        f"--olken-time {args.output / 'olken-time.pdf'} "
        f"--trace-time {args.output / 'trace-time.pdf'} "
        f"--accuracy {args.output / 'accuracy.pdf'} "
    )


if __name__ == "__main__":
    main()
