#include <assert.h>
#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "histogram/fractional_histogram.h"

bool
fractional_histogram__init(struct FractionalHistogram *me, const uint64_t length)
{
    if (me == NULL || length == 0) {
        return false;
    }
    // Assume it is either NULL or an uninitialized address!
    // NOTE A double with all zeroed bits is the equivalent of zero.
    me->histogram = calloc(length, sizeof(*me->histogram));
    assert(me->histogram[0] == 0.0 && "0x0000000000000000 should represent 0.0");
    if (me->histogram == NULL) {
        return false;
    }
    me->length = length;
    me->infinity = 0;
    me->false_infinity = 0.0;
    me->running_sum = 0;
    return true;
}

/// @brief  Update the histogram over a fully-in-range section.
static bool
insert_full_range(struct FractionalHistogram *me,
                  const uint64_t scaled_start,
                  const uint64_t scaled_exclusive_end,
                  const uint64_t scale,
                  const uint64_t range)
{
    assert(me != NULL && me->histogram != NULL && scale >= 1 && range >= 1 &&
           scaled_exclusive_end >= me->length);
    double delta = (double)scale / (double)range;
    for (uint64_t i = scaled_start; i < scaled_exclusive_end; ++i) {
        me->histogram[i] += delta;
    }
}

/// @brief  Update the histogram over a partially-in-range portion and then add
///         the remainder to the 'out-of-bounds' false infinity counter.
static bool
insert_partial_range(struct FractionalHistogram *me,
                     const uint64_t scaled_start,
                     const uint64_t scaled_exclusive_end,
                     const uint64_t scale,
                     const uint64_t range)
{
    assert(me != NULL && me->histogram != NULL && scale >= 1 && range >= 1 &&
           scaled_exclusive_end >= me->length);
    double delta = (double)scale / (double)range;
    for (uint64_t i = scaled_start; i < me->length; ++i) {
        me->histogram[i] += delta;
    }
    // NOTE I worked this out on paper. This is correct as far as I can tell. An
    //      illustrative example is me->length = 1 and scaled_exclusive_end = 5.
    //      Now, we have {1, 2, 3, 4} numbers to account for and 5-1 = 4. QED!
    me->false_infinity += delta * (scaled_exclusive_end - me->length);
}

bool
fractional_histogram__insert_scaled_finite(struct FractionalHistogram *me,
                                           const uint64_t start,
                                           const uint64_t range,
                                           const uint64_t scale)
{
    const uint64_t scaled_start = scale * start;
    const uint64_t scaled_exclusive_end = scaled_start + scale * range;
    if (me == NULL || me->histogram == NULL || range < 1 || scale < 1) {
        return false;
    }

    if (scaled_exclusive_end <= me->length) {
        insert_full_range(me, scaled_start, scaled_exclusive_end, scale, range);
    } else if (scaled_start < me->length) {
        insert_partial_range(me, scaled_start, scaled_exclusive_end, scale, range);
    } else {
        me->false_infinity += scale;
    }
    me->running_sum += scale;
    return true;
}

bool
fractional_histogram__insert_scaled_infinite(struct FractionalHistogram *me, const uint64_t scale)
{
    if (me == NULL || me->histogram == NULL || scale < 1) {
        return false;
    }
    me->infinity += scale;
    me->running_sum += scale;
    return true;
}

void
fractional_histogram__print_sparse(struct FractionalHistogram *me)
{
    if (me == NULL || me->histogram == NULL) {
        printf("{}\n");
        return;
    }
    printf("{");
    for (uint64_t i = 0; i < me->length; ++i) {
        if (me->histogram[i] != 0) {
            printf("\"%" PRIu64 "\": %lf, ", i, me->histogram[i]);
        }
    }
    // NOTE I assume me->length is much less than SIZE_MAX
    printf("\"%" PRIu64 "\": %" PRIu64 ", ", me->length, me->false_infinity);
    printf("\"inf\": %" PRIu64 "", me->infinity);
    printf("}\n");
}

void
fractional_histogram__destroy(struct FractionalHistogram *me)
{
    if (me == NULL) {
        return;
    }
    free(me->histogram);
    me->length = 0;
    me->false_infinity = 0;
    me->infinity = 0;
    me->running_sum = 0;
    return;
}