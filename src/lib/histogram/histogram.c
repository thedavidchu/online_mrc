#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "histogram/histogram.h"
#include "logger/logger.h"

bool
Histogram__init(struct Histogram *me,
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
Histogram__insert_finite(struct Histogram *me, const uint64_t index)
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
Histogram__insert_scaled_finite(struct Histogram *me,
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
Histogram__insert_infinite(struct Histogram *me)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    ++me->infinity;
    ++me->running_sum;
    return true;
}

bool
Histogram__insert_scaled_infinite(struct Histogram *me, const uint64_t scale)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    me->infinity += scale;
    me->running_sum += scale;
    return true;
}

void
Histogram__write_as_json(FILE *stream, struct Histogram *me)
{
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}");
        return;
    }
    if (me->histogram == NULL) {
        fprintf(stream, "{\"type\": \"Histogram\", \".histogram\": null}\n");
        return;
    }
    fprintf(stream,
            "{\"type\": \"Histogram\", \".num_bins\": %" PRIu64
            ", \".bin_size\": %" PRIu64 ", \".running_sum\": %" PRIu64
            ", \".histogram\": {",
            me->num_bins,
            me->bin_size,
            me->running_sum);
    bool first_value = true;
    for (uint64_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != 0) {
            if (first_value) {
                first_value = false;
            } else {
                fprintf(stream, ", ");
            }
            fprintf(stream,
                    "\"%" PRIu64 "\": %" PRIu64 "",
                    i * me->bin_size,
                    me->histogram[i]);
        }
    }
    // NOTE I assume me->num_bins is much less than SIZE_MAX
    fprintf(stream,
            "}, \".false_infinity\": %" PRIu64 ", \".infinity\": %" PRIu64
            "}\n",
            me->false_infinity,
            me->infinity);
}

void
Histogram__print_as_json(struct Histogram *me)
{
    Histogram__write_as_json(stdout, me);
}

bool
Histogram__exactly_equal(struct Histogram *me, struct Histogram *other)
{
    if (me == other) {
        LOGGER_DEBUG("Histograms are identical objects");
        return true;
    }
    if (me == NULL || other == NULL) {
        LOGGER_DEBUG("One Histograms is NULL");
        return false;
    }

    if (me->num_bins != other->num_bins || me->bin_size != other->bin_size ||
        me->false_infinity != other->false_infinity ||
        me->infinity != other->infinity ||
        me->running_sum != other->running_sum) {
        LOGGER_DEBUG("Histograms differ in metadata");
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

bool
Histogram__debug_difference(struct Histogram *me,
                            struct Histogram *other,
                            const size_t max_num_mismatch)
{
    if (me == NULL || me->histogram == NULL || me->bin_size == 0 ||
        me->num_bins == 0) {
        LOGGER_DEBUG("Invalid me object");
        return false;
    }
    if (other == NULL || other->histogram == NULL || other->bin_size == 0 ||
        other->num_bins == 0) {
        LOGGER_DEBUG("Invalid other object");
        return false;
    }

    if (me->bin_size != other->bin_size || me->num_bins != other->num_bins) {
        LOGGER_DEBUG("Metadata mismatch: .bin_size = {%" PRIu64 ", %" PRIu64
                     "}, .num_bins = {%" PRIu64 ", %" PRIu64 "}",
                     me->bin_size,
                     other->bin_size,
                     me->num_bins,
                     other->num_bins);
        return false;
    }

    size_t num_mismatch = 0;
    for (size_t i = 0; i < me->num_bins; ++i) {
        if (me->histogram[i] != other->histogram[i]) {
            LOGGER_DEBUG("Mismatch at %zu: %" PRIu64 " vs %" PRIu64,
                         i,
                         me->histogram[i],
                         other->histogram[i]);
            ++num_mismatch;
        }

        if (num_mismatch >= max_num_mismatch) {
            LOGGER_DEBUG("too many mismatches!");
            return false;
        }
    }
    return num_mismatch == 0;
}

bool
Histogram__adjust_first_buckets(struct Histogram *me, int64_t const adjustment)
{
    if (me == NULL || me->num_bins < 1 || me->bin_size < 1)
        return false;

    // NOTE SHARDS-Adj only adds to the first bucket; but what if the
    //      adjustment would make it negative? Well, in that case, I
    //      add it to the next buckets. I figure this is OKAY because
    //      histogram bin size is configurable and it's like using a
    //      larger bin.
    int64_t tmp_adj = adjustment;
    for (size_t i = 0; i < me->num_bins; ++i) {
        int64_t hist = me->histogram[i];
        if ((int64_t)me->histogram[i] + tmp_adj < 0) {
            me->histogram[i] = 0;
            tmp_adj += hist;
        } else {
            me->histogram[i] += tmp_adj;
            tmp_adj = 0;
            break;
        }
    }

    me->running_sum += adjustment - tmp_adj;

    // If the adjustment is larger than the number of elements, then
    // we have a problem!
    if (tmp_adj != 0) {
        // If I print warnings in many places, then I effectively get a
        // stack trace! Isn't that nice?
        LOGGER_WARN("the attempted adjustment (%" PRId64 ") "
                    "is larger than the adjustment we managed (%" PRId64 ")!",
                    adjustment,
                    adjustment - tmp_adj);
        return false;
    }

    return true;
}

void
Histogram__destroy(struct Histogram *me)
{
    if (me == NULL) {
        return;
    }
    free(me->histogram);
    *me = (struct Histogram){0};
    return;
}
