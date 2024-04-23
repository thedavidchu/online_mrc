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
- GLib 2.0 (for my hash tables)
- A C99 compiler that supports:
    - `#pragma once` (see Note)
    - __sync_bool_compare_and_swap(...)
- A C++17 compiler that supports:
    - `#pragma once` (see Note)

Note: I'm too lazy to write proper header guards; especially since they are so
fragile if you make them match the path but then move a file around!

Historic Notes
--------------------------------------------------------------------------------

This project started as my ECE1759 (Advances in Operating Systems) Final Project
and became my ECE1755 (Parallel Computer Architecture) Final project.
