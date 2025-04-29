#!/usr/bin/bash
### @brief  Build C/C++ and Rust components of this project.

meson compile -C build

pushd scripts/binarize-traces
# Build both debug and release modes because user could want either.
cargo build
cargo build --release
popd

pushd src/analysis/lifetime/david-lifespan
# Build both debug and release modes because user could want either.
cargo build
cargo build --release
popd
