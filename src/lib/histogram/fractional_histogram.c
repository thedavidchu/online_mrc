#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "histogram/fractional_histogram.h"
#include "math/doubles_are_equal.h"

bool
FractionalHistogram__init(struct FractionalHistogram *me,
                          const uint64_t num_bins,
                          const uint64_t bin_size)
{
    if (me == NULL || num_bins == 0) {
        return false;
    }
    // Assume it is either NULL or an uninitialized address!
    // NOTE A double with all zeroed bits is the equivalent of zero.
    me->histogram = calloc(num_bins, sizeof(*me->histogram));
    assert(me->histogram[0] == 0.0 &&
           "0x0000000000000000 should represent 0.0");
    if (me->histogram == NULL) {
        return false;
    }
    me->bin_size = bin_size;
    me->num_bins = num_bins;
    me->infinity = 0;
    me->false_infinity = 0.0;
    me->running_sum = 0;
    return true;
}

static uint64_t
get_first_bin(const uint64_t scaled_start, const uint64_t bin_size)
{
    return scaled_start / bin_size;
}

/// @note   This is my "proof" that we require the subtraction by one:
///
///         Let the `|-...-|` represent the range of the request, where the
///         first vertical bar is the start of the range and the second is one
///         past the last element of the range.
///         Then A and B are exclusively in Bin #0, while C stradles both
///         Bin #0 and #1.
///         A:              |-----|
///         B:                  |--|
///         C:                 |----|
///         Histogram:  |__________|__________|
///         Range:      0          10         30
///         Bin #:      0          1          2
static uint64_t
get_last_bin(const uint64_t scaled_exclusive_end, const uint64_t bin_size)
{
    return (scaled_exclusive_end - 1) / bin_size;
}

/// @brief  How much to fill a given bin.
static double
bin_portion(uint64_t bin_id,
            uint64_t bin_size,
            uint64_t scaled_start,
            uint64_t scaled_exclusive_end,
            const uint64_t scale,
            const uint64_t range)
{
    uint64_t first_bin = get_first_bin(scaled_start, bin_size);
    uint64_t last_bin = get_last_bin(scaled_exclusive_end, bin_size);

    if (bin_id < first_bin || bin_id > last_bin)
        return 0.0;

    uint64_t overlap = 0;
    if (bin_id == first_bin && bin_id == last_bin) {
        overlap = range;
    } else if (bin_id == first_bin) {
        overlap = (bin_id + 1) * bin_size - scaled_start;
    } else if (bin_id == last_bin) {
        overlap = scaled_exclusive_end - bin_id * bin_size;
    } else {
        overlap = bin_size;
    }
    return (double)scale * (double)overlap / (double)range;
}

/// @brief  Update the histogram over a fully-in-range section.
static void
insert_full_range(struct FractionalHistogram *me,
                  const uint64_t scaled_start,
                  const uint64_t scaled_exclusive_end,
                  const uint64_t scale,
                  const uint64_t range)
{
    assert(me != NULL && me->histogram != NULL && scale >= 1 && range >= 1 &&
           scaled_exclusive_end <= me->num_bins * me->bin_size);
    uint64_t first_bin = get_first_bin(scaled_start, me->bin_size);
    uint64_t last_bin = get_last_bin(scaled_exclusive_end, me->bin_size);
    assert(last_bin + 1 <= me->num_bins);
    for (uint64_t i = first_bin; i < last_bin + 1; ++i) {
        me->histogram[i] += bin_portion(i,
                                        me->bin_size,
                                        scaled_start,
                                        scaled_exclusive_end,
                                        scale,
                                        range);
    }
}

/// @brief  Update the histogram over a partially-in-range portion and then add
///         the remainder to the 'out-of-bounds' false infinity counter.
static void
insert_partial_range(struct FractionalHistogram *me,
                     const uint64_t scaled_start,
                     const uint64_t scaled_exclusive_end,
                     const uint64_t scale,
                     const uint64_t range)
{
    assert(me != NULL && me->histogram != NULL && scale >= 1 && range >= 1 &&
           scaled_exclusive_end >= me->num_bins * me->bin_size);
    uint64_t first_bin = get_first_bin(scaled_start, me->bin_size);
    uint64_t last_bin = get_last_bin(scaled_exclusive_end, me->bin_size);
    assert(last_bin + 1 >= me->num_bins);
    for (uint64_t i = first_bin; i < me->num_bins; ++i) {
        me->histogram[i] += bin_portion(i,
                                        me->bin_size,
                                        scaled_start,
                                        scaled_exclusive_end,
                                        scale,
                                        range);
    }
    // NOTE I worked this out on paper. This is correct as far as I can tell. An
    //      illustrative example is me->length = 1 and scaled_exclusive_end = 5.
    //      Now, we have {1, 2, 3, 4} numbers to account for and 5-1 = 4. QED!
    me->false_infinity +=
        (double)scale / (double)range *
        (double)(scaled_exclusive_end - me->num_bins * me->bin_size);
}

bool
FractionalHistogram__insert_scaled_finite(struct FractionalHistogram *me,
                                          const uint64_t start,
                                          const uint64_t range,
                                          const uint64_t scale)
{
    const uint64_t scaled_start = scale * start;
    const uint64_t scaled_exclusive_end = scaled_start + scale * range;
    if (me == NULL || me->histogram == NULL || range < 1 || scale < 1) {
        return false;
    }

    if (scaled_exclusive_end <= me->num_bins * me->bin_size) {
        insert_full_range(me, scaled_start, scaled_exclusive_end, scale, range);
    } else if (scaled_start < me->num_bins * me->bin_size) {
        insert_partial_range(me,
                             scaled_start,
                             scaled_exclusive_end,
                             scale,
                             range);
    } else {
        me->false_infinity += (double)scale;
    }
    me->running_sum += scale;
    return true;
}

bool
FractionalHistogram__insert_scaled_infinite(struct FractionalHistogram *me,
                                            const uint64_t scale)
{
    if (me == NULL || me->histogram == NULL || scale < 1) {
        return false;
    }
    me->infinity += scale;
    me->running_sum += scale;
    return true;
}

/// @brief  Print the histogram sparsely.
void
FractionalHistogram__print_as_json(struct FractionalHistogram *me)
{
    if (me == NULL) {
        printf("{\"type\": null}");
        return;
    }
    if (me->histogram == NULL) {
        printf("{\"type\": \"FractionalHistogram\", \".histogram\": null}\n");
        return;
    }
    printf("{\"type\": \"FractionalHistogram\", \".length\": %" PRIu64
           ", \".running_sum\": %" PRIu64 ", \".bin_size\": %" PRIu64
           ", \".histogram\": {",
           me->num_bins,
           me->running_sum,
           me->bin_size);
    bool first_value = true;
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != 0.0) {
            if (first_value) {
                first_value = false;
            } else {
                printf(", ");
            }
            printf("\"%" PRIu64 "\": %lf", i * me->bin_size, me->histogram[i]);
        }
    }
    // NOTE I assume me->length is much less than SIZE_MAX
    printf("}, \".false_infinity\": %lf, \".infinity\": %" PRIu64 "}\n",
           me->false_infinity,
           me->infinity);
}

bool
FractionalHistogram__exactly_equal(struct FractionalHistogram *me,
                                   struct FractionalHistogram *other)
{
    if (me == other) {
        return true;
    }
    if (me == NULL || other == NULL) {
        return false;
    }

    if (me->num_bins != other->num_bins || me->bin_size != other->bin_size ||
        !doubles_are_equal(me->false_infinity, other->false_infinity) ||
        me->infinity != other->infinity ||
        me->running_sum != other->running_sum) {
        return false;
    }
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        // We use this custom function to tolerate slight error in the histogram
        // due to imprecise floating-point arithmetic.
        if (!doubles_are_equal(me->histogram[i], other->histogram[i])) {
            return false;
        }
    }
    return true;
}

static bool
debug(struct FractionalHistogram *me, bool print)
{
    assert(me != NULL);

    double expected_sum = me->running_sum;
    double sum = 0.0;

    for (size_t i = 0; i < me->num_bins; ++i) {
        sum += me->histogram[i];
    }

    sum += me->false_infinity;
    sum += me->infinity;

    if (print)
        fprintf(stderr,
                "Expected sum: %g. Got sum: %g. Difference: %g\n",
                expected_sum,
                sum,
                expected_sum - sum);
    // I choose 1e-6 because this will print to the same value by default
    return doubles_are_close(expected_sum, sum, 1e-6);
}

bool
FractionalHistogram__validate(struct FractionalHistogram *me)
{
    if (me == NULL)
        return true;

    return debug(me, false);
}

void
FractionalHistogram__destroy(struct FractionalHistogram *me)
{
    if (me == NULL) {
        return;
    }
    free(me->histogram);
    *me = (struct FractionalHistogram){0};
    return;
}
