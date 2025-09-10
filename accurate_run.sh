#!/usr/bin/bash

v=2
shards="0.001"
cache_capacities="8GiB" 

meson compile -C build

odir="./myresults/accurate/"
mkdir -p "$odir"

for policy in TTL CacheLib Memcached Redis; do
    for c in 50 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18 25; do
    # for c in 12 15 31 37; do
        # if [[ $c -eq 12 || $c -eq 15 || $c -eq 31 || $c -eq 37 ]]; then
        #     shards=0.1
        # else
        #     shards=0.001
        # fi
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
