#pragma once
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "histogram/histogram.h"

enum RunnerMode {
    RUNNER_MODE_INVALID,
    // Run normally (meaning: overwrite any files)
    RUNNER_MODE_RUN,
    // Try to read the existing files instead of running. If they are
    // not found, fall back to running.
    RUNNER_MODE_TRY_READ,
};

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
    MRC_ALGORITHM_FIXED_SIZE_SHARDS,
    MRC_ALGORITHM_QUICKMRC,
    MRC_ALGORITHM_GOEL_QUICKMRC,
    MRC_ALGORITHM_EVICTING_MAP,
    MRC_ALGORITHM_EVICTING_QUICKMRC,
    MRC_ALGORITHM_AVERAGE_EVICTION_TIME,
    MRC_ALGORITHM_THEIR_AVERAGE_EVICTION_TIME,
};

/// @note   Importers will not be able to see the size of this array!
extern char *algorithm_names[];

/// @brief  Print algorithms by name in format: "{Olken,Fixed-Rate-SHARDS,...}".
/// @note   Why is the function here? No good reason except that it uses
///         the 'algorithm_names' variable.
void
print_available_algorithms(FILE *stream);

/// @brief  The arguments for running an instance.
/// @details
///     The standard algorithm contains the following information. Be
///     aware that the defaults and exact list are subject to change.
///     Please read the code for the correct values.
///     - Algorithm
///     - Output MRC path
///     - Output histogram path [optional]
///     - Sampling rate (if applicable) [optional. Default = by algorithm]
///     - Number of histogram bins [optional. Default = 1 << 20]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = reallocate]
///     - SHARDS adjustment [optional. Default = true for Fixed-Rate SHARDS]
///     The oracle contains:
///     - MRC path [both input/output]
///     - Histogram path [both input/output]
///     - Number of histogram bins [optional. Default = ~1 million]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = reallocate]
struct RunnerArguments {
    bool ok;
    enum RunnerMode run_mode;

    enum MRCAlgorithm algorithm;
    // The path to save the generated MRC.
    // TODO Include an 'overwrite' argument, which determines whether we
    //      use this path as an input (to simply calculate the accuracy)
    //      or an output (to save the generated MRC).
    char *mrc_path;
    // Similar to above but for the histogram.
    char *hist_path;
    // The initial rate at which to sample.
    double sampling_rate;
    // The initial number of bins in the histogram.
    size_t num_bins;
    // The initial size of each histogram bin.
    size_t bin_size;
    // The maximum number of unique elements to track.
    size_t max_size;
    // How to deal with histogram values that are larger than the array.
    enum HistogramOutOfBoundsMode out_of_bounds_mode;
    // Whether to adjust fixed-rate SHARDS to make up for the descrepancy
    // between the number of entries sampled and the expected number of
    // samples.
    // TODO Enable this for fixed-size SHARDS.
    bool shards_adj;
    // The number of buckets allotted to the QuickMRC buffers.
    size_t qmrc_size;
};

/// @brief  Parse an initialization string.
/// @details    My arbitrary format is thus:
///     "Algorithm(mrc=A,hist=B,sampling=C,num_bins=D,bin_size=E,mode=F,adj=G)"
/// @note   I do not allow spaces in case they are weirdly tokenized by
///         the shell.
/// @note   I do not follow the standard POSIX convention of arguments
///         that begin with a dash because, again, I do not want the
///         shell to parse these.
bool
RunnerArguments__init(struct RunnerArguments *const me, char const *const str);

bool
RunnerArguments__println(struct RunnerArguments const *const me,
                         FILE *const fp);

void
RunnerArguments__destroy(struct RunnerArguments *const me);
