# Testing Framework

Tests are a lot of work. I still have not decided on a testing framework.

The command `meson test` is nice.

I was considering using GLib's testing framework, but it honestly does not add
much value compared to my custom macro library.

I was also considering ThrowTheSwitch's Unity framework. It's in CMake and I am
not yet up for the challenge of integrating a CMake subproject into a Meson one.
However, most of their testing files are contained within a few C headers, so it
would be easy to just copy the source code in a hacky way.

## Contributing

The

```
|--include                              : Common headers in my custom testing framework
|--unit_test                            : Unit tests
|  \
|   |--lib_test                         : Test basic library modules
|   |--mrc_test                         : Test MRC algorithms
|--perf_test                            : Performance tests
```
