#!/usr/bin/bash

version=1
shards="0.001"

if false; then
    # Run with 0.0,0.0:0.25,0.25:0.75,0.75 ratios.
    shards="0.001"
    version=20250910
    c="19 52"
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        --overwrite \
        -p lru \
        -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
        -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
    exit
fi

## Run Decay
if false; then
    # Run with hourly-no-decay:15min-no-decay:hourly-50%-decay:15min-50%-decay
    shards="0.001"
    version=20250911
    c="19"
    # output="-hour-50p-decay"
    output="-15min-no-decay"
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        --overwrite \
        -k "4GiB" \
        -p lru \
        -o "/home/david/projects/online_mrc/myresults/decay-\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster${output}.out" \
        -e "/home/david/projects/online_mrc/myresults/decay-\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster${output}.err"
    exit
fi

# Mean
if true; then
    shards="0.001"
    version=42
    # c="45 22 7 19 11 24 29 6 17 25 52 18"
    c="19"
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        --overwrite \
        -p lru \
        -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
        -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
    exit
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        --overwrite \
        -p lfu \
        -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
        -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
    # Run SHARDS with 0.01 and 0.1. 
    c="52"
    shards="0.01"
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        -k "128MiB"
        --overwrite \
        -p lfu \
        -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
        -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
    shards="0.1"
    python3 \
        -m scripts.run_predictor \
        -c $c \
        -v $version \
        --shards $shards \
        -k "128MiB" \
        --overwrite \
        -p lfu \
        -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
        -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
    # Exit the program now!
    exit
fi

if false; then
    INPUT_TEMPLATE="/mnt/disk2-8tb/david/sari/cluster\$cluster.bin"
    python3 -m scripts.run_predictor -c 52 -v 1 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 1/size_t const nr_lfu_buckets = 2/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 2 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 2/size_t const nr_lfu_buckets = 4/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 4 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 4/size_t const nr_lfu_buckets = 8/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 8 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 8/size_t const nr_lfu_buckets = 16/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 16 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 16/size_t const nr_lfu_buckets = 32/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 32 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 32/size_t const nr_lfu_buckets = 64/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 64 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 64/size_t const nr_lfu_buckets = 128/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 128 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 128/size_t const nr_lfu_buckets = 256/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 256 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 256/size_t const nr_lfu_buckets = 512/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
    python3 -m scripts.run_predictor -c 52 -v 512 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
    sed -i 's/size_t const nr_lfu_buckets = 512/size_t const nr_lfu_buckets = 1/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
fi
