#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "types/entry_type.h"

struct AverageEvictionTime {
    struct HashTable hash_table;
    struct Histogram histogram;
    uint64_t current_time_stamp;
};

bool
AverageEvictionTime__init(struct AverageEvictionTime *me,
                          const uint64_t histogram_num_bins,
                          const uint64_t histogram_bin_size);

bool
AverageEvictionTime__access_item(struct AverageEvictionTime *me,
                                 EntryType entry);

bool
AverageEvictionTime__post_process(struct AverageEvictionTime *me);

/// @brief  Convert a reuse time histogram to a miss rate curve.
/// @param  hist: struct Histogram *
///             This is the reuse time histogram.
/// @details    We get the MRC with the following:
///         Let the following definitions hold:
///         - MR(c) : Miss Rate for cache size c
///         - AET(c) : Average Eviction Time for a cache size c
///         - RT(t) : Reuse time at time t
///         - P(t) : Probability that the reuse time is greater than time t
///         - N : Total number of reuse times within the entire reuse histogram
///         We are given:
///             [1] MR(c) = P(AET(c))
///             [2] P(t) = SUM{i=t+1..INF} RT(i) / N
///             [3] c = SUM{t=0..=AET(c)} P(t)  ...in the discrete case
///         Therefore, find AET(c) by finding summing up P(0) + P(1) + ...
///         until you reach c. Do this for every c.
bool
AverageEvictionTime__to_mrc(struct MissRateCurve *mrc, struct Histogram *hist);

/// @brief  Follow the pseudocode to convert the MRC to the MRC.
///         Source:
///         https://dl-acm-org.myaccess.library.utoronto.ca/doi/10.1145/3185751
/// @note   We eagerly convert to doubles rather than using integers, so
///         there may be some decreased accuracy.
bool
AverageEvictionTime__their_to_mrc(struct MissRateCurve *mrc,
                                  struct Histogram *hist);

void
AverageEvictionTime__destroy(struct AverageEvictionTime *me);
