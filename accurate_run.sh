#!/usr/bin/bash
# @brief    Run the non-Psyche simulators (i.e. accurate).
# @note We use the following traces (ordered from smallest to largest):
#       45 22 7 19 11 24 29 6 17 25 52 18.

date=20250915
shards="0.001"
cache_capacities="32GiB" 

meson compile -C build

odir="./myresults/accurate-$date/"
mkdir -p "$odir"

# Debug an implementation on cluster 7.
if false; then
    v=0
    shards=0.001
    echo "Running debug"
    # for c in 45 22 7 19 11 24 29 6 17 25 52 18; do
    for c in 6; do
        for policy in Memcached; do
            echo "> Policy $policy, Cluster #$c"
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
    exit;
fi

# Run un-SHARDS version of the experiments
if true; then
    date=20250920
    v=0
    shards=1
    
    odir="./myresults/accurate-$date-v$v-s$shards/"
    mkdir -p "$odir"

    echo "Running unsharded"
    # for c in 45 22 7 19 11 24 29 6 17 25 52 18; do
    for c in 7; do
        for policy in Redis; do
            echo "Policy $policy, Cluster $c"
            time \
                ./build/src/predictor/accurate_exe \
                /mnt/disk1-20tb/Sari-Traces/FilteredTwitter_Binary/cluster$c.bin \
                Sari \
                $policy \
                $cache_capacities \
                $shards \
                > "$odir/result-$policy-cluster$c.out" \
                2> "$odir/result-$policy-cluster$c.err"
        done
    done
    exit;
fi

## Run everything.
if false; then
    v=20250915
    echo "Running all experiments!"
    ## Sometimes cluster #50 is insightful; it has too few objects, though.
    for c in 45 22 7 19 11 24 29 6 17 25 52 18; do
        echo "Cluster $c"
        for policy in TTL CacheLib Memcached Redis; do
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
    exit;
fi