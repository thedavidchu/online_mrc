#!/usr/bin/bash

# Version number
v=5
dir=blah
ext=pdf
policy=lfu
shards=0.001

if [ ! -d $dir ]; then
    echo "Directory '$dir' DNE; making it now..."
    mkdir -p $dir
fi

# for i in 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18; do
for i in 52; do
    odir="$dir/cluster$i"
    mkdir -p $odir
    python3 scripts/plot_predictive_cache.py \
        --input "/home/david/projects/online_mrc/myresults/lru_ttl/result-$policy-cluster$i-v$v-s$shards.out" \
        --policy $policy \
        --mrc "$odir/plot-$policy-cluster$i-v$v-s$shards-mrc.$ext" \
        --removal-stats "$odir/plot-$policy-cluster$i-v$v-s$shards-stats.$ext" \
        --sizes "$odir/plot-$policy-cluster$i-v$v-s$shards-sizes.$ext" \
        --eviction-time "$odir/plot-$policy-cluster$i-v$v-s$shards-eviction.$ext" \
        --remaining-lifetime "$odir/plot-$policy-cluster$i-v$v-s$shards-remaining.$ext" \
        --temporal-statistics "$odir/plot-$policy-cluster$i-v$v-s$shards-temporal.$ext" \
        --eviction-histograms "$odir/plot-$policy-cluster$i-v$v-s$shards-histogram.$ext" \
        --other "$odir/plot-$policy-cluster$i-v$v-s$shards-other.$ext"
    # python3 scripts/plot_predictive_cache.py \
    #     --input "/home/david/projects/online_mrc/myresults/lru_ttl/result-cluster$i-v$v.out" \
    #     --mrc "$odir/plot-cluster$i-v$v-mrc.$ext" \
    #     --removal-stats "$odir/plot-cluster$i-v$v-stats.$ext" \
    #     --sizes "$odir/plot-cluster$i-v$v-sizes.$ext" \
    #     --eviction-time "$odir/plot-cluster$i-v$v-eviction.$ext" \
    #     --remaining-lifetime "$odir/plot-cluster$i-v$v-remaining.$ext" \
    #     --temporal-statistics "$odir/plot-cluster$i-v$v-temporal.$ext" \
    #     --eviction-histograms "$odir/plot-cluster$i-v$v-histogram.$ext" \
    #     --other "$odir/plot-cluster$i-v$v-other.$ext"
done
