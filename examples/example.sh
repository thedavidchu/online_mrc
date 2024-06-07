#!/usr/bin/bash
# This is an example of how to use this repository.

TOPLEVEL=$(git rev-parse --show-toplevel)

# Change to the build directory, because this is where Meson likes to do
# all of its operations.
cd $TOPLEVEL/build

# Define some common variables.
EXE=$TOPLEVEL/build/bench/generate_mrc_exe
INPUT_PATH=$TOPLEVEL/data/src2.bin

# This builds my code using the Meson build system.
meson compile

# Run and time a variety of experiments.
time $EXE -i $INPUT_PATH -a Olken -o olken-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS -s 1e-3 -o frs1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS-Adj -s 1e-3 -o frsa1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a Fixed-Size-SHARDS -s 1e-1 -o fss1e-1-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1.0 -o qmrc-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-3 -o qmrcs1e-3-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-2 -o qmrcs1e-2-mrc.bin
time $EXE -i $INPUT_PATH -a QuickMRC -s 1e-1 -o qmrcs1e-1-mrc.bin

# Hash only timing test (we will not plot the results).
time $EXE -i $INPUT_PATH -a Fixed-Rate-SHARDS-Adj -s 1e-12 -o hash-only.bin

# Plot the MRCs.
python3 ../scripts/plot_mrc.py \
    --oracle olken-mrc.bin \
    --input \
    frs1e-3-mrc.bin frsa1e-3-mrc.bin fss1e-1-mrc.bin \
    qmrc-mrc.bin qmrcs1e-3-mrc.bin qmrcs1e-2-mrc.bin qmrcs1e-1-mrc.bin \
    --output mrc.png
