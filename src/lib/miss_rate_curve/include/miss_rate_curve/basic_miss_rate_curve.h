#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h> // for MIN()

#include "histogram/basic_histogram.h"
#include "histogram/fractional_histogram.h"

struct BasicMissRateCurve {
    double *miss_rate;
    uint64_t length;
};

bool
basic_miss_rate_curve__init_from_fractional_histogram(
    struct BasicMissRateCurve *me,
    struct FractionalHistogram *histogram);

bool
basic_miss_rate_curve__init_from_basic_histogram(
    struct BasicMissRateCurve *me,
    struct BasicHistogram *histogram);

/// NOTE    The arguments are in a terrible order. Sorry.
bool
basic_miss_rate_curve__init_from_parda_histogram(struct BasicMissRateCurve *me,
                                                 uint64_t histogram_length,
                                                 unsigned int *histogram,
                                                 uint64_t histogram_total,
                                                 uint64_t false_infinities);

bool
basic_miss_rate_curve__init_from_file(struct BasicMissRateCurve *me,
                                      char const *restrict const file_name,
                                      const uint64_t length);

bool
basic_miss_rate_curve__write_binary_to_file(
    struct BasicMissRateCurve *me,
    char const *restrict const file_name);

double
basic_miss_rate_curve__mean_squared_error(struct BasicMissRateCurve *lhs,
                                          struct BasicMissRateCurve *rhs);

void
basic_miss_rate_curve__print_as_json(struct BasicMissRateCurve *me);

void
basic_miss_rate_curve__destroy(struct BasicMissRateCurve *me);
