#!/usr/bin/bash

v=2
shards="0.001"
cache_capacities="8GiB" 

meson compile -C build

odir="./myresults/accurate/"
mkdir -p "$odir"

for policy in TTL CacheLib Memcached Redis; do
    # Sometimes cluster #50 is insightful; it has too few objects, though.
    for c in 45 22 7 19 11 24 29 6 17 25 52 18; do
        time \
            ./build/src/predictor/accurate_exe \
            /mnt/disk1-20tb/Sari-Traces/FilteredTwitter_Binary/cluster$c.bin \
            Sari \
            $policy \
            $cache_capacities \
            $shards \
            > "$odir/result-$policy-cluster$c-v$v-s$shards.out" \
            2> "$odir/result-$policy-cluster$c-v$v-s$shards.err"
    done
done
