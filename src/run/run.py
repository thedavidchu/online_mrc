#!/usr/bin/python3
"""
@note   The plotting functions are not run by default. They need to be
        invoked separately. I'm still trying to think of intuitive
        semantics to run these but make sure to never overwrite data
        that is expensive to generate.
"""
import argparse
import os
import shutil
from getpass import getpass
from pathlib import Path
from shlex import split, quote
from subprocess import run, CompletedProcess
from warnings import warn


EXE = "/home/david/projects/online_mrc/build/src/run/generate_mrc_exe"
ALGORITHMS = [
    # NOTE  I guarantee that "Olken" is the first algorithm in this list.
    #       I do not make guarantees about any other order.
    "Olken",
    "Evicting-Map",
    "Fixed-Rate-SHARDS",  # With adjustment (default algorithm)
    "Fixed-Rate-SHARDS-raw",  # No adjustment
    "Fixed-Size-SHARDS",  # No adjustment (it's good enough without it)
    "QuickMRC",  # My adaptation that uses Ashvin's
    "QuickMRC-10",  # QuickMRC with SHARDS applied with a threshold of 1/10
    "QuickMRC-100",
    "QuickMRC-1000",
    "Goel-QuickMRC",
    "Goel-QuickMRC-10",
    "Goel-QuickMRC-100",
    "Goel-QuickMRC-1000",
]
MAX_SIZE = 1 << 13


def abspath(path: str | Path) -> Path:
    """Create an absolute path."""
    return Path(path).resolve()


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
    oracle_dir: Path | None = None,
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
    if oracle_dir is not None:
        if not oracle_dir.exists():
            os.mkdir(oracle_dir)
        if not (oracle_dir / "hist").exists():
            os.mkdir(oracle_dir / "hist")
        if not (oracle_dir / "mrc").exists():
            os.mkdir(oracle_dir / "mrc")

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
    input_: Path, format: str, output_: Path, *, skip: set[str], oracle_path: Path
):
    def mrc(algo: str, path: Path | None = None):
        """Example return: <path|output_>/mrc/clusterX-Olken-mrc.bin"""
        if path is None:
            path = output_
        return os.path.join(str(path), "mrc", f"{input_.stem}-{algo}-mrc.bin")

    def hist(algo: str, path: Path | None = None):
        """Example return: <path|output_>/hist/clusterX-Olken-hist.bin"""
        if path is None:
            path = output_
        return os.path.join(str(path), "hist", f"{input_.stem}-{algo}-hist.bin")

    def stats(algo: str):
        return output_ / "stats" / f"{input_.stem}-{algo}-stats.bin"

    run_cmds = {
        "Olken": f'--oracle "Olken(mrc={mrc("Olken")},hist={hist("Olken")},stats_path={stats("Olken")})"',
        "Fixed-Rate-SHARDS": f'--run "Fixed-Rate-SHARDS(mrc={mrc("Fixed-Rate-SHARDS")},hist={hist("Fixed-Rate-SHARDS")},sampling=1e-3,adj=true,stats_path={stats("Fixed-Rate-SHARDS")})"',
        "Fixed-Rate-SHARDS-raw": f'--run "Fixed-Rate-SHARDS(mrc={mrc("Fixed-Rate-SHARDS-raw")},hist={hist("Fixed-Rate-SHARDS-raw")},sampling=1e-3,adj=false,stats_path={stats("Fixed-Rate-SHARDS-raw")})"',
        # NOTE  I place evicting map between the two SHARDS so that its
        #       cache is warmed up but also warms Fixed-Size SHARDS's.
        "Evicting-Map": f'--run "Evicting-Map(mrc={mrc("Evicting-Map")},hist={hist("Evicting-Map")},sampling=1e-1,max_size={MAX_SIZE},stats_path={stats("Evicting-Map")})"',
        "Fixed-Size-SHARDS": f'--run "Fixed-Size-SHARDS(mrc={mrc("Fixed-Size-SHARDS")},hist={hist("Fixed-Size-SHARDS")},sampling=1e-1,max_size={MAX_SIZE},stats_path={stats("Fixed-Size-SHARDS")})"',
        "Goel-QuickMRC": f'--run "Goel-QuickMRC(mrc={mrc("Goel-QuickMRC")},hist={hist("Goel-QuickMRC")},sampling=1e0,stats_path={stats("Goel-QuickMRC")})"',
        "Goel-QuickMRC-10": f'--run "Goel-QuickMRC(mrc={mrc("Goel-QuickMRC-10")},hist={hist("Goel-QuickMRC-10")},sampling=1e-1,stats_path={stats("Goel-QuickMRC-10")})"',
        "Goel-QuickMRC-100": f'--run "Goel-QuickMRC(mrc={mrc("Goel-QuickMRC-100")},hist={hist("Goel-QuickMRC-100")},sampling=1e-2,stats_path={stats("Goel-QuickMRC-100")})"',
        "Goel-QuickMRC-1000": f'--run "Goel-QuickMRC(mrc={mrc("Goel-QuickMRC-1000")},hist={hist("Goel-QuickMRC-1000")},sampling=1e-3,stats_path={stats("Goel-QuickMRC-1000")})"',
        "QuickMRC": f'--run "QuickMRC(mrc={mrc("QuickMRC")},hist={hist("QuickMRC")},sampling=1e0,stats_path={stats("QuickMRC")})"',
        "QuickMRC-10": f'--run "QuickMRC(mrc={mrc("QuickMRC-10")},hist={hist("QuickMRC-10")},sampling=1e-1,stats_path={stats("QuickMRC-10")})"',
        "QuickMRC-100": f'--run "QuickMRC(mrc={mrc("QuickMRC-100")},hist={hist("QuickMRC-100")},sampling=1e-2,stats_path={stats("QuickMRC-100")})"',
        "QuickMRC-1000": f'--run "QuickMRC(mrc={mrc("QuickMRC-1000")},hist={hist("QuickMRC-1000")},sampling=1e-3,stats_path={stats("QuickMRC-1000")})"',
    }
    # We specify a special place for the oracle to be placed. This should
    # reduce redundant oracle computations.
    if oracle_path:
        # NOTE  We provide a 'stats_path' just in case we end up running
        #       Olken again.
        run_cmds["Olken"] = (
            f'--oracle "Olken(runmode=tryread,mrc={mrc("Olken", oracle_path)},hist={hist("Olken", oracle_path)},stats_path={stats("Olken")})"'
        )
    # NOTE  I want to make sure that I don't forget to add any algorithms
    #       in one of these places. Ideally, this would be a static test
    #       but Python isn't that nice.
    assert set(run_cmds) == set(
        ALGORITHMS
    ), f"symmetric difference: {set(run_cmds) ^ set(ALGORITHMS)}"
    log = sh(
        f"{EXE} "
        f"--input {input_} "
        f"--format {format} "
        f"{' '.join(cmd for algo, cmd in run_cmds.items() if algo not in skip)}"
    )

    with open(os.path.join(output_, "log", f"{input_.stem}.log"), "w") as f:
        # I write the stderr first since it is flushed first.
        f.write(log.stderr)
        f.write(log.stdout)


def plot_mrc(f: Path, mrc_dir: Path, plot_dir: Path, oracle: Path | None):
    if oracle is None:
        oracle_path, *input_paths = [
            mrc_dir / f"{f.stem}-{algo}-mrc.bin" for algo in ALGORITHMS
        ]
    else:
        oracle_path = oracle / "mrc" / f"{f.stem}-Olken-mrc.bin"
        input_paths = [mrc_dir / f"{f.stem}-{algo}-mrc.bin" for algo in ALGORITHMS]

    input_paths = [str(f) for f in input_paths if f.exists()]
    output_paths = [plot_dir / f"{f.stem}-mrc.png", plot_dir / f"{f.stem}-mrc.pdf"]
    sh(
        f"python3 src/analysis/plot/plot_mrc.py "
        + (f"--oracle {oracle_path} " if oracle_path.exists() else "")
        + (f"--input {' '.join(input_paths)} " if input_paths else "")
        + f"--output {' '.join(str(x) for x in output_paths)}"
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
        "--input", "-i", type=abspath, required=True, help="directory of inputs"
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
        "--output", "-o", type=abspath, required=True, help="directory for outputs"
    )
    parser.add_argument(
        "--oracle",
        type=abspath,
        default=None,
        help="path to the oracle directory (e.g. './olken-oracle/{mrc,hist}')",
    )
    parser.add_argument(
        "--format",
        "-f",
        choices=["Kia", "Sari"],
        required=True,
        help="format of the input traces",
    )
    group = parser.add_mutually_exclusive_group()
    group.add_argument(
        "--skip",
        nargs="*",
        choices=ALGORITHMS,
        help="skip running certain algorithms",
    )
    group.add_argument(
        "--select",
        nargs="*",
        choices=ALGORITHMS,
        help="select certain algorithms to run",
    )
    parser.add_argument("--sudo", action="store_true", help="run as sudo")
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
    # Select (or skip) algorithms
    if args.skip == [] and args.select == []:
        skip = []
    elif args.skip:
        skip = args.skip
    else:
        skip = set(ALGORITHMS) - set(args.select)

    setup_env(
        output_dir=args.output,
        oracle_dir=args.oracle,
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

    for f in files:
        run_trace(
            f,
            args.format,
            args.output,
            skip=skip,
            oracle_path=args.oracle,
        )
    for f in files:
        plot_mrc(f, mrc_dir, plot_dir, args.oracle)
    analyze_log(args.output, log_dir)


if __name__ == "__main__":
    main()
