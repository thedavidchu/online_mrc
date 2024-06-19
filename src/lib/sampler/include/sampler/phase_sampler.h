#pragma once

#include <stdbool.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "miss_rate_curve/miss_rate_curve.h"

struct PhaseSampler {
    GPtrArray *saved_histograms;
};

bool
PhaseSampler__init(struct PhaseSampler *const me);

void
PhaseSampler__destroy(struct PhaseSampler *const me);

bool
should_i_create_a_new_histogram(struct Histogram const *const old_hist,
                                struct Histogram const *const new_hist,
                                double const threshold);

bool
PhaseSampler__change_histogram(struct PhaseSampler *const me,
                               struct Histogram const *const old_hist);

/// @note   The number of histogram bins is two less than the number of
///         MRC bins.
bool
PhaseSampler__create_mrc(struct PhaseSampler const *const me,
                         struct MissRateCurve *const mrc,
                         uint64_t const num_hist_bins,
                         uint64_t const bin_size);
