[![cpp-linter](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml)
[![Meson Test](https://github.com/thedavidchu/online_mrc/workflows/meson-build/badge.svg)](https://github.com/thedavidchu/online_mrc/actions)

# On-Line Miss-Rate Curves

This repository analyzes the performance of on-line miss-rate curve (MRC)
generation algorithms. For completeness, I also implement MRC algorithms that
are traditionally considered off-line.

## Dependencies

This project depends on the following:
- The Meson build system (which uses Ninja by default)
- GLib 2.0 (for my hash tables)
- A C99 compiler that supports `#pragma once` (see Note)
- A C++17 compiler that supports `#pragma once` (see Note)

Note: I'm too lazy to write proper header guards; especially since they are so
fragile if you make them match the path but then move a file around!

## Historic Notes

Originally, this was my project repository for my ECE1759
(Advances in Operating Systems) Final Project.

Originally, I was going to write it in Rust. Then, I realized I would be much
too slow at writing Rust.

