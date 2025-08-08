#!/usr/bin/bash

# Version number
v=0
shards="0.001"
policy="lfu"
dir=foo
ext=png

# for i in 50 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18; do
for i in 52; do
    odir="$dir/cluster$i"
    mkdir -p $odir
    python3 scripts/plot_accurate_cache.py \
        --input "./myresults/lru_ttl/result-$policy-cluster$i-v$v-s$shards.out" \
        --mrc "$odir/plot-$policy-cluster$i-v$v-s$shards-mrc.$ext" \
        --lifetime-threshold "$odir/plot-$policy-cluster$i-v$v-s$shards-lifetime-threshold.$ext"
done
