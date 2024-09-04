#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/fractional_histogram.h"
#include "histogram/histogram.h"

struct MissRateCurve {
    double *miss_rate;
    size_t num_bins;
    size_t bin_size;
};

/// @brief  Allocate an empty MRC. This is not a valid MRC since it does
///         not start at 1.0.
/// @note   The MRC has 2 more bins than the histogram and this function
///         expects the number of MRC bins desired.
bool
MissRateCurve__alloc_empty(struct MissRateCurve *const me,
                           uint64_t const num_mrc_bins,
                           uint64_t const bin_size);

bool
MissRateCurve__init_from_fractional_histogram(
    struct MissRateCurve *me,
    struct FractionalHistogram *histogram);

bool
MissRateCurve__init_from_histogram(struct MissRateCurve *me,
                                   struct Histogram const *const histogram);

/// NOTE    The arguments are in a terrible order. Sorry.
bool
MissRateCurve__init_from_parda_histogram(struct MissRateCurve *me,
                                         uint64_t histogram_length,
                                         unsigned int *histogram,
                                         uint64_t histogram_total,
                                         uint64_t false_infinities);

/// @brief  Save a sparse MRC curve to a file.
bool
MissRateCurve__save(struct MissRateCurve const *const me,
                    char const *restrict const file_name);

/// @brief  Load a sparse MRC curve from a file.
bool
MissRateCurve__load(struct MissRateCurve *const me,
                    char const *restrict const file_name);

/// @note   This is useful when trying to average many MRCs without
///         needing to load all of the histograms at once.
/// @note   I am not entirely content with the semantics of this function.
bool
MissRateCurve__scaled_iadd(struct MissRateCurve *const me,
                           struct MissRateCurve const *const other,
                           double const scale);

/// @brief  Ensure that all values are within some 'epsilon' between two
///         miss rate curves.
bool
MissRateCurve__all_close(struct MissRateCurve const *const lhs,
                         struct MissRateCurve const *const rhs,
                         double const epsilon);

/// @return non-negative mean squared error, or INFINITY on error.
double
MissRateCurve__mean_absolute_error(struct MissRateCurve const *const lhs,
                                   struct MissRateCurve const *const rhs);

/// @return non-negative mean squared error, or INFINITY on error.
double
MissRateCurve__mean_squared_error(struct MissRateCurve const *const lhs,
                                  struct MissRateCurve const *const rhs);

void
MissRateCurve__write_as_json(FILE *stream,
                             struct MissRateCurve const *const me);

void
MissRateCurve__print_as_json(struct MissRateCurve *me);

bool
MissRateCurve__validate(struct MissRateCurve *me);

void
MissRateCurve__destroy(struct MissRateCurve *me);
