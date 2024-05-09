#!/usr/bin/bash

TOPLEVEL=$(git rev-parse --show-toplevel)
cd $TOPLEVEL/build

EXE=./bench/generate_mrc_exe
INPUT_PATH=zipf

meson compile

$EXE -i $INPUT_PATH -a Olken -o olken-mrc.bin
$EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS -o frs-mrc.bin
$EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS-Adj -o frsa-mrc.bin
$EXE -i $INPUT_PATH -a Fixed-Size-SHARDS -o fss-mrc.bin
$EXE -i $INPUT_PATH -a QuickMRC -o qmrc-mrc.bin
$EXE -i $INPUT_PATH -a QuickMRC-SHARDS-3 -o qmrcs3-mrc.bin
$EXE -i $INPUT_PATH -a QuickMRC-SHARDS-2 -o qmrcs2-mrc.bin
$EXE -i $INPUT_PATH -a QuickMRC-SHARDS-1 -o qmrcs1-mrc.bin

python3 ../script/plot_mrc.py \
    --input olken-mrc.bin frs-mrc.bin frsa-mrc.bin fss-mrc.bin qmrc-mrc.bin qmrcs3-mrc.bin qmrcs2-mrc.bin qmrcs1-mrc.bin \
    --output mrc.png
