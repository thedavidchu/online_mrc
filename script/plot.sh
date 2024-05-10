#!/usr/bin/bash

TOPLEVEL=$(git rev-parse --show-toplevel)
cd $TOPLEVEL/build

EXE=./bench/generate_mrc_exe
INPUT_PATH=zipf

meson compile

time $EXE -i $INPUT_PATH -a Olken -o olken-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS -s 1e-3 -o frs1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS-Adj -s 1e-3 -o frsa1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS-Adj -s 1e-12 -o hash-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Size-SHARDS -s 1e-1 -o fss1e-1-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1.0 -o qmrc-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-3 -o qmrcs1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-2 -o qmrcs1e-2-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-1 -o qmrcs1e-1-mrc.bin

python3 ../script/plot_mrc.py \
    --oracle olken-mrc.bin \
    --input \
    frs1e-3-mrc.bin frsa1e-3-mrc.bin hash-mrc.bin fss1e-1-mrc.bin \
    qmrc-mrc.bin qmrcs1e-3-mrc.bin qmrcs1e-2-mrc.bin qmrcs1e-1-mrc.bin \
    --output mrc.png
