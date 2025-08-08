#!/usr/bin/bash

v=0
shards=0.001

for c in 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18; do
    if [ $c -eq 15 ]; then
        shards=0.01
    else
        shards=0.001
    fi
    python3 \
        scripts/plot_compare_existing_caches.py \
        --inputs /home/david/projects/online_mrc/myresults/accurate/result-{Memcached,Redis,CacheLib,TTL}-cluster$c-v$v-s$shards.out \
        --cache-names Memcached Redis CacheLib Optimal \
        --trace-name "Twitter Cluster #$c" \
        --shards-ratio $shards \
        --memory-usage cluster$c-mem-usage.pdf \
        --relative-memory-usage cluster$c-rel-mem-usage.pdf >> stats.txt
done
