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

parser = argparse.ArgumentParser()
parser.add_argument(
    "--input-template",
    "-i",
    type=Template,
    default=Template(
        "/mnt/disk1-20tb/Sari-Traces/FilteredTwitter_Binary/cluster$cluster.bin"
    ),
    help="input path template with '$cluster'",
)
parser.add_argument(
    "--output-template",
    "-o",
    type=Template,
    default=Template("./myresults/lru_ttl/result-cluster$cluster-v$version.out"),
    help="output path template with '$cluster' and '$version'",
)
parser.add_argument(
    "--error-output-template",
    "-e",
    type=Template,
    default=Template("./myresults/lru_ttl/result-cluster$cluster-v$version.err"),
    help="error output path template with '$cluster' and '$version'",
)
parser.add_argument("--clusters", "-c", nargs="+", type=int, help="cluster ID(s)")
parser.add_argument("--version", "-v", type=int, required=True, help="version number")
parser.add_argument(
    "--lifetime-lru-only-mode", action="store_true", help="use LRU-only lifetimes"
)
parser.add_argument("--shards", "-s", type=float, default=1.0, help="shards ratio")
parser.add_argument(
    "--overwrite", action="store_true", help="overwrite existing output (versus append)"
)
parser.add_argument("--verbose", action="store_true", help="verbosely print outputs")
args = parser.parse_args()

INPUT_TEMPLATE = args.input_template
OUTPUT_TEMPLATE = args.output_template
ERROR_OUTPUT_TEMPLATE = args.error_output_template
cluster_ids = sorted(
    set(args.clusters),
    key=lambda nr: get_twitter_path(nr, "Sari", template=INPUT_TEMPLATE).stat().st_size,
)

version = args.version
shards_ratio = args.shards
overwrite = args.overwrite
lifetime_lru_only_mode = args.lifetime_lru_only_mode
modes = [
    "EvictionTime",
    # "LifeTime",
]
thresholds = [
    (0.0, 1.0),  # Accurate LRU-TTL Simulation
    # (0.25, 0.75),  # Conservative classifier
    (0.0, 0.0),  # TTLs-Only (with Volatile-TTLs)
    (0.25, 0.25),  # Biased-to-TTL binary classifier
    (0.5, 0.5),  # Unbiased binary classifier
    (0.75, 0.75),  # Biased-to-LRU binary classifier
    (1.0, 1.0),  # LRU-Only (with passive TTLs)
]
# AWS's serverless offers as little as 128MB of cache.
capacities = [
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
verbose = args.verbose


def stdout_file(cluster_id: int, version: int):
    return Path(
        OUTPUT_TEMPLATE.substitute(cluster=cluster_id, version=version)
    ).resolve()


def stderr_file(cluster_id: int, version: int):
    return Path(
        ERROR_OUTPUT_TEMPLATE.substitute(cluster=cluster_id, version=version)
    ).resolve()


def run_predictor(
    cluster_id: int,
    lower_threshold: float,
    upper_threshold: float,
    capacities: list[str],
    mode: str,
    lifetime_lru_mode: bool,
    shards_ratio: float,
    version: int,
    *,
    verbose: bool = False,
):
    cmd = [
        # NOTE  I use '/usr/bin/time' because 'time' alone seems
        #       grumpy with arguments.
        "/usr/bin/time",
        "-v",
        "./build/src/predictor/predictor_exe",
        INPUT_TEMPLATE.substitute(cluster=cluster_id),
        "Sari",
        str(lower_threshold),
        str(upper_threshold),
        " ".join(capacities),
        mode,
        str(lifetime_lru_mode).lower(),
        str(shards_ratio),
    ]
    print(cmd)
    r = run(cmd, capture_output=True, text=True)
    if verbose:
        print(r.stdout)
        print(r.stderr)
    with open(stdout_file(cluster_id, version), "a") as f:
        f.write(r.stdout)
    with open(stderr_file(cluster_id, version), "a") as f:
        f.write(r.stderr)


def main():
    print(f"Input: {INPUT_TEMPLATE.safe_substitute()}")
    print(f"stdout: {OUTPUT_TEMPLATE.safe_substitute()}")
    print(f"stderr: {ERROR_OUTPUT_TEMPLATE.safe_substitute()}")
    print(f"{version=}")
    print(f"{cluster_ids=}")
    print(f"{modes=}")
    print(f"{thresholds=}")
    print(f"{capacities=}")
    print(f"{lifetime_lru_only_mode=}")
    print(f"{shards_ratio=}")
    print(f"{overwrite=}")
    run("meson compile -C build".split())

    if overwrite:
        for c in cluster_ids:
            stdout_file(c, version).unlink(missing_ok=True)
            stderr_file(c, version).unlink(missing_ok=True)

    # Convert the 'product' to a list so that TQDM can see how many
    # elements there are.
    for mode, cluster, (l, u) in tqdm(list(product(modes, cluster_ids, thresholds))):
        run_predictor(
            cluster,
            l,
            u,
            capacities,
            mode,
            lifetime_lru_only_mode,
            shards_ratio,
            version,
            verbose=verbose,
        )


if __name__ == "__main__":
    main()
