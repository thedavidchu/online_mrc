#!/usr/bin/bash

policy=lfu
v=0
shards=0.001

indir=myresults/$policy-ttl-v$v-s$shards
outdir=plots/$policy-ttl-v$v-s$shards

mkdir -p $outdir

for c in 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18; do
    python3 scripts/calculate_accuracy.py \
        --input $indir/result-cluster$c.out \
        > $outdir/stats-cluster$c.txt
    python3 scripts/plot_main_cache_metrics.py \
        --input $indir/result-cluster$c.out \
        --policy $policy \
        --name "Twitter Cluster $c" \
        --shards-ratio $shards \
        --output $outdir/plot-cluster$c.pdf
done