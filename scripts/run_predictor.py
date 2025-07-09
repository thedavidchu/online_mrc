#!/usr/bin/python3

"""@brief Run the predictive cache simulations.

Full recommended set sorted in increasing trace size (according to Sari):
{15, 45, 31, 12, 22, 7, 19, 37, 11, 24, 44, 29, 6, 17, 25, 52, 18}

Or without commas:
15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18

"""

import argparse
from itertools import product
from string import Template
from pathlib import Path
from subprocess import run

from tqdm import tqdm

from src.analysis.common.trace import get_twitter_path

DEFAULT_THRESHOLDS = [
    (0.0, 1.0),  # Accurate LRU-TTL Simulation
    # (0.25, 0.75),  # Conservative classifier
    (0.0, 0.0),  # TTLs-Only (with Volatile-TTLs)
    # (0.25, 0.25),  # Biased-to-TTL binary classifier
    (0.5, 0.5),  # Unbiased binary classifier
    # (0.75, 0.75),  # Biased-to-LRU binary classifier
    (1.0, 1.0),  # LRU-Only (with passive TTLs)
]
# AWS's serverless offers as little as 128MB of cache.
DEFAULT_CAPACITIES = [
    "128MiB",
    "256MiB",
    "512MiB",
    "1GiB",
    "2GiB",
    "3GiB",
    "4GiB",
    "5GiB",
    "6GiB",
    "7GiB",
    "8GiB",
    # "12GiB",
    # "16GiB",
]
DEFAULT_INPUT_TEMPLATE = Template(
    "/mnt/disk1-20tb/Sari-Traces/FilteredTwitter_Binary/cluster$cluster.bin"
)
DEFAULT_OUTPUT_TEMPLATE = Template(
    "/home/david/projects/online_mrc/myresults/lru_ttl/result-$policy-cluster$cluster-v$version-s$shards.out"
)
DEFAULT_ERROR_TEMPLATE = Template(
    "/home/david/projects/online_mrc/myresults/lru_ttl/result-$policy-cluster$cluster-v$version-s$shards.err"
)


def resolve_template_file(
    template: Template,
    eviction_policy: str,
    cluster_id: int,
    version: int,
    shards_ratio: float,
):
    return Path(
        template.substitute(
            policy=eviction_policy,
            cluster=cluster_id,
            version=version,
            shards=shards_ratio,
        )
    ).resolve()


def ensure_parent_dirs_exist(path: Path):
    path.parent.mkdir(parents=True, exist_ok=True)


def overwrite(args):
    for c in args.clusters:
        resolve_template_file(
            args.output_template,
            args.eviction_policy,
            c,
            args.version,
            args.shards_ratio,
        ).unlink(missing_ok=True)
        resolve_template_file(
            args.error_template,
            args.eviction_policy,
            c,
            args.version,
            args.shards_ratio,
        ).unlink(missing_ok=True)


def run_predictor(
    cluster_id: int,
    eviction_policy: str,
    lower_threshold: float,
    upper_threshold: float,
    capacities: list[str],
    shards_ratio: float,
    version: int,
    input_template: Template,
    output_template: Template,
    error_template: Template,
    *,
    verbose: bool = False,
    gdb: bool = False,
):
    out = resolve_template_file(
        output_template, eviction_policy, cluster_id, version, shards_ratio
    )
    err = resolve_template_file(
        error_template, eviction_policy, cluster_id, version, shards_ratio
    )
    ensure_parent_dirs_exist(out)
    ensure_parent_dirs_exist(err)
    # NOTE  I use '/usr/bin/time' because 'time' alone seems
    #       grumpy with arguments.
    cmd = ["/usr/bin/time", "-v"]
    my_exe = ["./build/src/predictor/predictor_exe"]
    my_args = [
        input_template.substitute(cluster=cluster_id),
        "Sari",
        str(lower_threshold),
        str(upper_threshold),
        " ".join(capacities),
        str(shards_ratio),
        eviction_policy,
    ]
    print(cmd + my_exe + my_args)
    if gdb:
        gdb_args = Path("gdb_args.txt").resolve()
        x = f"r {' '.join(my_args)}\nbt\n\nq\ny\n"
        gdb_args.write_text(x)
        r = run(
            ["gdb"] + my_exe + ["-x", str(gdb_args)], capture_output=True, text=True
        )
        # If you wanted to delete the GDB arguments file, you could run
        # the command: 'gdb_args.unlink()'. But I want to leave it so
        # that it's easy for me to reuse in the future.
    else:
        r = run(cmd + my_exe + my_args, capture_output=True, text=True)
    if verbose:
        print(r.stdout)
        print(r.stderr)
    with out.open("a") as f:
        f.write(r.stdout)
    with err.open("a") as f:
        f.write(r.stderr)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--input-template",
        "-i",
        type=Template,
        default=DEFAULT_INPUT_TEMPLATE,
        help="input path template with '$cluster'",
    )
    parser.add_argument(
        "--output-template",
        "-o",
        type=Template,
        default=DEFAULT_OUTPUT_TEMPLATE,
        help="output path template with '$policy', '$cluster', '$version', and '$shards'",
    )
    parser.add_argument(
        "--error-template",
        "-e",
        type=Template,
        default=DEFAULT_ERROR_TEMPLATE,
        help="error path template with '$policy', '$cluster', '$version', and '$shards'",
    )
    parser.add_argument("--clusters", "-c", nargs="+", type=int, help="cluster ID(s)")
    parser.add_argument(
        "--capacities",
        "-k",
        nargs="+",
        default=DEFAULT_CAPACITIES,
        help="cache capacities, e.g. '128MiB 256MiB 512MiB 1GiB'",
    )
    parser.add_argument(
        "--version", "-v", type=int, required=True, help="version number"
    )
    parser.add_argument(
        "--shards-ratio", "-s", type=float, default=1.0, help="shards ratio"
    )
    parser.add_argument(
        "--eviction-policy", "-p", choices=["lru", "lfu"], help="eviction policy"
    )
    parser.add_argument(
        "--dry-run",
        "-n",
        action="store_true",
        help="dry run",
    )
    parser.add_argument(
        "--overwrite",
        action="store_true",
        help="overwrite existing output (versus append)",
    )
    parser.add_argument("--gdb", action="store_true", help="run with GDB")
    parser.add_argument(
        "--verbose", action="store_true", help="verbosely print outputs"
    )
    args = parser.parse_args()

    cluster_ids = sorted(
        set(args.clusters),
        key=lambda nr: get_twitter_path(nr, "Sari", template=args.input_template)
        .stat()
        .st_size,
    )
    print(f"Input: {args.input_template.safe_substitute()}")
    print(f"stdout: {args.output_template.safe_substitute()}")
    print(f"stderr: {args.error_template.safe_substitute()}")
    print(f"{args.version=}")
    print(f"{cluster_ids=}")
    print(f"{DEFAULT_THRESHOLDS=}")
    print(f"{args.capacities=}")
    print(f"{args.shards_ratio=}")
    print(f"{args.overwrite=}")
    print(f"{args.eviction_policy=}")
    print(f"{args.gdb=}")

    run("meson compile -C build".split())

    if args.dry_run:
        exit(0)

    # TODO  Make the overwrite lazy and only occur before we write to a
    #       file for the first time.
    if args.overwrite:
        overwrite(args)

    # Convert the 'product' to a list so that TQDM can see how many
    # elements there are.
    for cluster, (l, u) in tqdm(list(product(cluster_ids, DEFAULT_THRESHOLDS))):
        run_predictor(
            cluster,
            args.eviction_policy,
            l,
            u,
            args.capacities,
            args.shards_ratio,
            args.version,
            args.input_template,
            args.output_template,
            args.error_template,
            verbose=args.verbose,
            gdb=args.gdb,
        )


if __name__ == "__main__":
    main()
