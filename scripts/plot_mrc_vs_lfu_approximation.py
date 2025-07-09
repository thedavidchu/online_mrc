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


N = [1, 2, 4, 8, 16, 32, 64, 128, 256, 512]
shards = 0.01
fig, axes = plt.subplots(1, len(N))
fig.suptitle(f"Miss Ratio Curves for Cluster #52 (SHARDS: {shards})")
fig.set_size_inches(5 * len(N), 5)
for i, ax in zip(N, axes):
    path = Path(
        f"/home/david/projects/online_mrc/myresults/lru_ttl/result-lfu-cluster52-v{i}-s{shards}.out"
    )
    data = parse_data(path)
    plot_mrc(ax, data, "LFU")
    # Use proper grammar (because English...).
    ax.set_title(f"{i} LFU Bucket{'s' if i != 1 else ''}")
    # Metadata Objects Over Time
fig.savefig(f"mrc_vs_lfu_approximation_s{shards}.pdf")
