#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "histogram/basic_histogram.h"

bool
BasicHistogram__init(struct BasicHistogram *me,
                     const uint64_t num_bins,
                     const uint64_t bin_size)
{
    if (me == NULL || num_bins == 0) {
        return false;
    }
    // Assume it is either NULL or an uninitialized address!
    me->histogram = (uint64_t *)calloc(num_bins, sizeof(uint64_t));
    if (me->histogram == NULL) {
        return false;
    }
    me->num_bins = num_bins;
    me->bin_size = bin_size;
    me->infinity = 0;
    me->false_infinity = 0;
    me->running_sum = 0;
    return true;
}

bool
BasicHistogram__insert_finite(struct BasicHistogram *me, const uint64_t index)
{
    if (me == NULL || me->histogram == NULL || me->bin_size == 0) {
        return false;
    }
    // NOTE I think it's clearer to have more code in the if-blocks than to
    //      spread it around. The optimizing compiler should remove it.
    if (index < me->num_bins * me->bin_size) {
        ++me->histogram[index / me->bin_size];
        ++me->running_sum;
    } else {
        ++me->false_infinity;
        ++me->running_sum;
    }
    return true;
}

bool
BasicHistogram__insert_scaled_finite(struct BasicHistogram *me,
                                     const uint64_t index,
                                     const uint64_t scale)
{
    const uint64_t scaled_index = scale * index;
    if (me == NULL || me->histogram == NULL || me->bin_size == 0) {
        return false;
    }
    // NOTE I think it's clearer to have more code in the if-blocks than to
    //      spread it around. The optimizing compiler should remove it.
    if (scaled_index < me->num_bins * me->bin_size) {
        me->histogram[scaled_index / me->bin_size] += scale;
        me->running_sum += scale;
    } else {
        me->false_infinity += scale;
        me->running_sum += scale;
    }
    return true;
}

bool
BasicHistogram__insert_infinite(struct BasicHistogram *me)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    ++me->infinity;
    ++me->running_sum;
    return true;
}

bool
BasicHistogram__insert_scaled_infinite(struct BasicHistogram *me,
                                       const uint64_t scale)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    me->infinity += scale;
    me->running_sum += scale;
    return true;
}

void
BasicHistogram__print_as_json(struct BasicHistogram *me)
{
    if (me == NULL) {
        printf("{\"type\": null}");
        return;
    }
    if (me->histogram == NULL) {
        printf("{\"type\": \"BasicHistogram\", \".histogram\": null}\n");
        return;
    }
    printf("{\"type\": \"BasicHistogram\", \".num_bins\": %" PRIu64
           ", \".running_sum\": %" PRIu64 ", \".histogram\": {",
           me->num_bins,
           me->running_sum);
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != 0) {
            printf("\"%" PRIu64 "\": %" PRIu64 ", ", i, me->histogram[i]);
        }
    }
    // NOTE I assume me->num_bins is much less than SIZE_MAX
    printf("\"%" PRIu64 "\": %" PRIu64 "}, \".false_infinity\": %" PRIu64
           ", \".infinity\": %" PRIu64 "}\n",
           me->num_bins,
           me->false_infinity,
           me->false_infinity,
           me->infinity);
}

bool
BasicHistogram__exactly_equal(struct BasicHistogram *me,
                              struct BasicHistogram *other)
{
    if (me == other) {
        return true;
    }
    if (me == NULL || other == NULL) {
        return false;
    }

    if (me->num_bins != other->num_bins ||
        me->bin_size != other->bin_size ||
        me->false_infinity != other->false_infinity ||
        me->infinity != other->infinity ||
        me->running_sum != other->running_sum) {
        return false;
    }
    // NOTE I do not account for overflow; I believe overflow is impossible
    //      since the size of the address space * sizeof(uint64_t) < 2^64.
    if (memcmp(me->histogram,
               other->histogram,
               sizeof(*me->histogram) * me->num_bins) != 0) {
        return false;
    }
    return true;
}

void
BasicHistogram__destroy(struct BasicHistogram *me)
{
    if (me == NULL) {
        return;
    }
    free(me->histogram);
    *me = (struct BasicHistogram){0};
    return;
}
