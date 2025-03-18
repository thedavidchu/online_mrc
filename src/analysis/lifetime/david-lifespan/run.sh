#!/usr/bin/bash

cd "$(git rev-parse --show-toplevel)/src/analysis/lifetime/david-lifespan"
for i in 52 22 25 11 18 19 6 7; do
    cargo +nightly  run -r -- -p /mnt/disk1-20tb/kia/twitter/cluster$i.bin -s 16GiB -n 10 --logspace -o cluster$i-lru.pdf
done