"""
@brief  Analyze the access patterns of clients in the Twitter traces.
"""

import csv
from itertools import count

import matplotlib.pyplot as plt
import numpy as np


fmt = dict(marker=".", s=1)

i = 39

PREFIX = "/home/david/projects/online_mrc/myresults/rw"
FINAL = f"{PREFIX}/cluster{i}-client-final-client.log"
READS = f"{PREFIX}/cluster{i}-client-reads-per-client.log"
WRITES = f"{PREFIX}/cluster{i}-client-writes-per-client.log"


def splitenum(x: iter) -> tuple[list, list]:
    """Split and de-lazify an iterable"""
    # Let x = [x0, x1, ...]
    x = list(enumerate(x))  # x = [(0, x0), (1, x1), ...]
    return [a for a, _ in x], [b for _, b in x]


final, rd, wr = {}, {}, {}
with open(FINAL) as f:
    final_client = csv.reader(f)
    # Skip the header row
    next(final_client)
    for total, bkt, frq, pdf, cdf in final_client:
        final[int(bkt)] = int(frq)
final_ranked = sorted(final.values())
final_sorted = np.array(sorted(final.items()))

fig, (ax0, ax1) = plt.subplots(1, 2)
ax0.set_title("Keys per Client ID")
ax0.set_xlabel("Client ID")
ax0.scatter(final_sorted[:, 0], final_sorted[:, 1], **fmt)
ax1.set_title("Keys per-client ranked")
ax1.set_xlabel("Rank")
ax1.scatter(*splitenum(final_ranked), **fmt)
fig.savefig(f"{PREFIX}/cluster{i}-final-client.pdf")

# Reads-per-client
with open(READS) as f:
    rd_per_client = csv.reader(f)
    next(rd_per_client)
    for total, bkt, frq, pdf, cdf in rd_per_client:
        rd[int(bkt)] = int(frq)
rd_ranked = sorted(rd.values())
rd_sorted = np.array(sorted(rd.items()))
fig, (ax0, ax1) = plt.subplots(1, 2)
ax0.set_title("Per-Client Reads Ordered by Client ID")
ax0.set_xlabel("Client ID")
ax0.scatter(rd_sorted[:, 0], rd_sorted[:, 1], **fmt)
ax1.set_title("Ranked per-client")
ax1.set_xlabel("Rank")
ax1.scatter(*splitenum(rd_ranked), **fmt)
fig.savefig(f"{PREFIX}/cluster{i}-rd-per-client.pdf")

# Writes-per-client
with open(WRITES) as f:
    wr_per_client = csv.reader(f)
    next(wr_per_client)
    for total, bkt, frq, pdf, cdf in wr_per_client:
        wr[int(bkt)] = int(frq)
wr_ranked = sorted(wr.values())
wr_sorted = np.array(sorted(wr.items()))
fig, (ax0, ax1) = plt.subplots(1, 2)
ax0.set_title("Per-Client Writes Ordered by Client ID")
ax0.set_xlabel("Client ID")
ax0.scatter(wr_sorted[:, 0], wr_sorted[:, 1], **fmt)
ax1.set_title("Ranked per-client")
ax1.set_xlabel("Rank")
ax1.scatter(*splitenum(wr_ranked), **fmt)
fig.savefig(f"{PREFIX}/cluster{i}-wr-per-client.pdf")

# Read-Writes per Client
rw = np.concatenate([rd_sorted, wr_sorted], axis=1)
assert np.all(rw[:, 0] == rw[:, 2])
fig, (ax0, ax1, ax2) = plt.subplots(1, 3)
ax0.set_title("Per-Client R/W Ordered by Client ID")
ax0.set_xlabel("Client ID")
ax0.scatter(rw[:, 0], rw[:, 1], c="g", **fmt, label="Reads")
ax0.scatter(rw[:, 0], rw[:, 3], c="r", **fmt, label="Writes")
ax0.legend()
rw = [tuple(x) for x in rw]
# Per-client R/W ordered by either number of reads or number of writes.
rw_ranked_by_r = np.array(sorted(rw, key=lambda x: x[1]))
rw_ranked_by_w = np.array(sorted(rw, key=lambda x: x[3]))
ax1.set_title("Per-Client R/W Ordered by Number of Reads")
ax1.set_xlabel("Rank")
ax1.scatter(*splitenum(rw_ranked_by_r[:, 1]), c="g", **fmt, label="Reads")
ax1.scatter(*splitenum(rw_ranked_by_r[:, 3]), c="r", **fmt, label="Writes")
ax1.legend()

ax2.set_title("Per-Client R/W Ordered by Number of Writes")
ax2.set_xlabel("Rank")
ax2.scatter(*splitenum(rw_ranked_by_w[:, 1]), c="g", **fmt, label="Reads")
ax2.scatter(*splitenum(rw_ranked_by_w[:, 3]), c="r", **fmt, label="Writes")
ax2.legend()
fig.savefig(f"{PREFIX}/cluster{i}-rw-per-client.pdf")
