#!usr/bin/bash
import argparse
from pathlib import Path

import matplotlib.pyplot as plt

from plot_predictive_cache import (
    plot_lines,
    get_stat,
    MiB,
    GiB,
    parse_data,
    GiB_SHARDS_ARGS,
    SCALE_MS_TO_HOUR,
    IDENTITY_X_D,
)


def plot_remaining_lifetime_vs_lru_position():
    INPUT = Path(
        "/home/david/projects/online_mrc/myresults/lru-ttl-v2-s0.001/result-cluster50.out"
    )
    OUTPUT = Path("cluster50-remaining-lifetime-vs-lru-position.pdf")

    data = parse_data(INPUT)
    capacities = [0.256 * MiB, 1.0 * GiB, 4.0 * GiB]
    fig, axes = plt.subplots(
        1, len(capacities), squeeze=False, sharex=True, sharey=True
    )
    fig.set_size_inches(len(capacities) * 5, 5)
    for i, c in enumerate(capacities):
        plot_lines(
            axes[0, i],
            data,
            lambda d: None,
            f"LRU Cache Position [GiB]",
            lambda d: (
                get_stat(d, ["Extras", "remaining_lifetime", "Cache Sizes [B]"])
                if get_stat(d, ["Lower Ratio"]) == 0.0
                and get_stat(d, ["Upper Ratio"]) == 0.0
                and get_stat(d, ["Extras", "Nominal Capacity [B]"]) == c
                else None
            ),
            *GiB_SHARDS_ARGS,
            "Remaining Lifetime [h]",
            lambda d: d["Extras"]["remaining_lifetime"]["Remaining Lifetimes [ms]"],
            SCALE_MS_TO_HOUR,
            IDENTITY_X_D,
        )
        axes[0, i].set_title("")
    fig.savefig(OUTPUT)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--input")
    args = parser.parse_args()

    plot_remaining_lifetime_vs_lru_position()


if __name__ == "__main__":
    main()
