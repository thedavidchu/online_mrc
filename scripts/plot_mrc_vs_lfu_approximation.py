#!/usr/bin/python3
"""
This plots the MRC vs LFU approximations.

Yes, it's ugly and hard-coded. I don't put it in a separate directory
because otherwise Python complains about imports. Gosh, Python has some
ugly wrinkles when you try to scale it.
"""

from pathlib import Path

import matplotlib.pyplot as plt

from plot_predictive_cache import parse_data
from plot_main_cache_metrics import plot_mrc

N = 10
fig, axes = plt.subplots(1, N)
fig.suptitle("Miss Ratio Curves for Cluster #52 (SHARDS: 0.001)")
fig.set_size_inches(5 * N, 5)
for i, ax in enumerate(axes):
    path = Path(
        f"/home/david/projects/online_mrc/myresults/lru_ttl/result-lfu-cluster52-v{i+1}-s0.001.out"
    )
    data = parse_data(path)
    plot_mrc(ax, data, "LFU")
    ax.set_title(f"{i + 1} LFU Bucket{'s' if i else ''}")
    # Metadata Objects Over Time
fig.savefig("mrc_vs_lfu_approximation.pdf")
