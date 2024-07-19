#pragma once
#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include "histogram/histogram.h"

enum MRCAlgorithm {
    MRC_ALGORITHM_INVALID,
    MRC_ALGORITHM_OLKEN,
    MRC_ALGORITHM_FIXED_RATE_SHARDS,
    MRC_ALGORITHM_FIXED_SIZE_SHARDS,
    MRC_ALGORITHM_QUICKMRC,
    MRC_ALGORITHM_GOEL_QUICKMRC,
    MRC_ALGORITHM_EVICTING_MAP,
    MRC_ALGORITHM_AVERAGE_EVICTION_TIME,
    MRC_ALGORITHM_THEIR_AVERAGE_EVICTION_TIME,
};

// NOTE This corresponds to the same order as MRCAlgorithm so that we can
//      simply use the enumeration to print the correct string!
static char *algorithm_names[] = {
    "INVALID",
    "Olken",
    "Fixed-Rate-SHARDS",
    "Fixed-Size-SHARDS",
    "QuickMRC",
    "Goel-QuickMRC",
    "Evicting-Map",
    "Average-Eviction-Time",
    "Their-Average-Eviction-Time",
};

/// @brief  Print algorithms by name in format: "{Olken,Fixed-Rate-SHARDS,...}".
/// @note   Why is the function here? No good reason except that it uses
///         the 'algorithm_names' variable.
void
print_available_algorithms(FILE *stream);

/// @brief  The arguments for running an instance.
/// @details
///     The standard algorithm contains the following information:
///     - Algorithm
///     - Output MRC path
///     - Output histogram path [optional]
///     - Sampling rate (if applicable) [optional. Default = by algorithm]
///     - Number of histogram bins [optional. Default = 1 << 20]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = overflow]
///     - SHARDS adjustment [optional. Default = true for Fixed-Rate SHARDS]
///     The oracle contains:
///     - MRC path [both input/output]
///     - Histogram path [both input/output]
///     - Number of histogram bins [optional. Default = arbitrarily large]
///     - Size of histogram bins [optional. Default = 1]
///     - Histogram overflow strategy [optional. Default = overflow]
struct RunnerArguments {
    bool ok;

    enum MRCAlgorithm algorithm;
    char *mrc_path;
    char *hist_path;
    double sampling_rate;
    size_t num_bins;
    size_t bin_size;
    size_t max_size;
    enum HistogramOutOfBoundsMode out_of_bounds_mode;
    bool shards_adj;
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
