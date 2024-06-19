#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "sampler/phase_sampler.h"
#include "types/entry_type.h"

struct AverageEvictionTime {
    struct HashTable hash_table;
    struct Histogram histogram;
    uint64_t current_time_stamp;

    // Phase sampling
    bool use_phase_sampling;
    uint64_t phase_sampling_epoch;
    struct PhaseSampler phase_sampler;
};

/// @param  phase_sampling_epoch : uint64_t const
///         This is the length of each phase sampling epoch. To not use
///         phase sampling, set this to 0. Admittedly, this is a
///         confusing convention. I know you didn't sign up for an
///         essay, but I think Rust's enum types show intent much
///         better. An optimization on top of this would be if one could
///         specify what part of the range is not used so that the enum
///         type could be made to fit entirely within a single integral
///         value. Wow, that's the longest comment I've written about
///         probably the least significant thing. QED.
bool
AverageEvictionTime__init(struct AverageEvictionTime *const me,
                          uint64_t const histogram_num_bins,
                          uint64_t const histogram_bin_size,
                          uint64_t const phase_sampling_epoch);

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
AverageEvictionTime__to_mrc(struct AverageEvictionTime const *const me,
                            struct MissRateCurve *const mrc);

/// @brief  Follow the pseudocode to convert the MRC to the MRC.
///         Source:
///         https://dl-acm-org.myaccess.library.utoronto.ca/doi/10.1145/3185751
/// @note   We eagerly convert to doubles rather than using integers, so
///         there may be some decreased accuracy.
bool
AverageEvictionTime__their_to_mrc(struct AverageEvictionTime const *const me,
                                  struct MissRateCurve *const mrc);

void
AverageEvictionTime__destroy(struct AverageEvictionTime *me);
