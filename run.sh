#!/usr/bin/bash

version=1
shards="0.001"

# vvv ERASE BELOW vvv
shards="0.001"
version=3
c=19
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
# ^^^ ERASE ABOVE ^^^

# python3 -m scripts.run_predictor -c 50 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18 -v 2 --shards 0.01 --lifetime-lru-only-mode --overwrite
python3 \
    -m scripts.run_predictor \
    -c 50 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18 \
    -v $version \
    --shards $shards \
    --overwrite \
    -p lru \
    -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
    -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"
python3 \
    -m scripts.run_predictor \
    -c 50 15 45 31 12 22 7 19 37 11 24 44 29 6 17 25 52 18 \
    -v $version \
    --shards $shards \
    --overwrite \
    -p lfu \
    -o "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.out" \
    -e "/home/david/projects/online_mrc/myresults/\$policy-ttl-v\$version-s\$shards/result-cluster\$cluster.err"

INPUT_TEMPLATE="/mnt/disk2-8tb/david/sari/cluster\$cluster.bin"

# python3 -m scripts.run_predictor -c 52 -v 1 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 1/size_t const nr_lfu_buckets = 2/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 2 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 2/size_t const nr_lfu_buckets = 4/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 4 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 4/size_t const nr_lfu_buckets = 8/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 8 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 8/size_t const nr_lfu_buckets = 16/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 16 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 16/size_t const nr_lfu_buckets = 32/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 32 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 32/size_t const nr_lfu_buckets = 64/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 64 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 64/size_t const nr_lfu_buckets = 128/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 128 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 128/size_t const nr_lfu_buckets = 256/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 256 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 256/size_t const nr_lfu_buckets = 512/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
# python3 -m scripts.run_predictor -c 52 -v 512 --shards $shards --overwrite -p lfu -i $INPUT_TEMPLATE
# sed -i 's/size_t const nr_lfu_buckets = 512/size_t const nr_lfu_buckets = 1/g' /home/david/projects/online_mrc/src/predictor/include/lib/predictive_lfu_ttl_cache.hpp
