#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h> // for MIN()

#include "histogram/fractional_histogram.h"
#include "histogram/histogram.h"

struct MissRateCurve {
    double *miss_rate;
    uint64_t num_bins;
    uint64_t bin_size;
};

bool
MissRateCurve__init_from_fractional_histogram(
    struct MissRateCurve *me,
    struct FractionalHistogram *histogram);

bool
MissRateCurve__init_from_histogram(struct MissRateCurve *me,
                                   struct Histogram *histogram);

/// NOTE    The arguments are in a terrible order. Sorry.
bool
MissRateCurve__init_from_parda_histogram(struct MissRateCurve *me,
                                         uint64_t histogram_length,
                                         unsigned int *histogram,
                                         uint64_t histogram_total,
                                         uint64_t false_infinities);

bool
MissRateCurve__init_from_file(struct MissRateCurve *me,
                              char const *restrict const file_name,
                              const uint64_t num_bins,
                              const uint64_t bin_size);

bool
MissRateCurve__init_from_sparse_file(struct MissRateCurve *me,
                                     char const *restrict const file_name,
                                     const uint64_t num_bins,
                                     const uint64_t bin_size);

/// @note   This does not save the bin_size, which means you need to pass
///         this information some other way. Sorry, but it would make the
///         output binary more complicated if I were to pass it.
bool
MissRateCurve__write_binary_to_file(struct MissRateCurve const *const me,
                                    char const *restrict const file_name);

/// @brief  Save pairs of (index, value) so it's much sparser and thus
///         easier to plot.
bool
MissRateCurve__write_sparse_binary_to_file(
    struct MissRateCurve const *const me,
    char const *restrict const file_name);

/// @return non-negative mean squared error, or a negative number (e.g. -1.0) on
///         error.
double
MissRateCurve__mean_squared_error(struct MissRateCurve *lhs,
                                  struct MissRateCurve *rhs);

/// @return non-negative mean absolute error, or a negative number (e.g. -1.0)
///         on error.
double
MissRateCurve__mean_absolute_error(struct MissRateCurve *lhs,
                                   struct MissRateCurve *rhs);

void
MissRateCurve__print_as_json(struct MissRateCurve *me);

bool
MissRateCurve__validate(struct MissRateCurve *me);

void
MissRateCurve__destroy(struct MissRateCurve *me);
