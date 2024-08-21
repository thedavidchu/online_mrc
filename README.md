[![cpp-linter](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml/badge.svg)](https://github.com/cpp-linter/cpp-linter-action/actions/workflows/cpp-linter.yml)
[![Meson Test](https://github.com/thedavidchu/online_mrc/workflows/meson-build/badge.svg)](https://github.com/thedavidchu/online_mrc/actions)

Online Miss Rate Curve Generation
================================================================================

This repository analyzes the performance of online miss rate curve (MRC)
generation algorithms. For completeness, I also implement MRC algorithms that
are traditionally considered offline.

Dependencies
--------------------------------------------------------------------------------

This project depends on the following:
- The Meson build system (which uses Ninja by default)
    - Requires Python 3.7+
- GLib 2.0 (for my hash tables, etc.)
    - This requires pkg-config
    - Thre requires Python's `packaging` module
    - Requires `apt install ninja-build`
- The Boost library (version 1.81 or newer)
- A C99 compiler that supports:
    - `#pragma once` (see Note)
    - __sync_bool_compare_and_swap(...)
- A C++17 compiler that supports:
    - `#pragma once` (see Note)

Note: I'm too lazy to write proper header guards; especially since they are so
fragile if you make them match the path but then move a file around!

The Python files are formatted with the Black formatter and are tested
using Python version 3.11.

Project Structure
--------------------------------------------------------------------------------

```
|--bench/               : Benchmarking the {speed,accuracy} performance of
|                         various algorithms.
|--data/                : Directory for data.
|--examples/            : Example usage and scripts.
|--scripts/             : Directory for simple scripts and examples.
|--src/
|  |--analysis/         : Scripts for analyzing data.
|  |--lib/              : Common utilities (types and a logger).
|  `--mrc/              : Miss Rate Curve algorithms.
|
|--test/
|  |--include/test/     : Testing header library.
|  |--lib_test/         : Test general common utilities.
|  `--mrc_test/         : Unit tests for our MRC algorithms.
|--.clang-format        : Formatting file for clangd. Please use this if you
|                         wish to make a pull request.
|--.clang-tidy          : Static analysis file.
|--INSTALLME.sh         : Run this script to install the dependencies for this
|                         project!
|--LICENSE              : MIT license.
|--meson.build          : Top-level build directory folder.
`--README.md            : Me!
```

You will notice that the `.gitignore` ignores `/mydata/`, `myresults/`, and
`/myscripts/`. Feel free to use these files in your local repository to house
your personal data, results, and scripts.

If you create a very useful, generalizable script, feel free to move it to the
global `/scripts/` directory and share it!

Historic Notes
--------------------------------------------------------------------------------

This project started as my ECE1759 (Advances in Operating Systems) Final Project
and became my ECE1755 (Parallel Computer Architecture) Final project.
