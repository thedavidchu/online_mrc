#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "histogram/basic_histogram.h"

bool
basic_histogram_init(struct BasicHistogram *me, const uint64_t length)
{
    if (me == NULL || length == 0) {
        return false;
    }
    // Assume it is either NULL or an uninitialized address!
    me->histogram = (uint64_t *)calloc(length, sizeof(uint64_t));
    if (me->histogram == NULL) {
        return false;
    }
    me->length = length;
    me->infinity = 0;
    me->false_infinity = 0;
    me->running_sum = 0;
    return true;
}

bool
basic_histogram_insert_finite(struct BasicHistogram *me, const uint64_t index)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    // NOTE I think it's clearer to have more code in the if-blocks than to
    //      spread it around. The optimizing compiler should remove it.
    if (index < me->length) {
        ++me->histogram[index];
        ++me->running_sum;
    } else {
        ++me->false_infinity;
        ++me->running_sum;
    }
    return true;
}

bool
basic_histogram_insert_infinite(struct BasicHistogram *me)
{
    if (me == NULL || me->histogram == NULL) {
        return false;
    }
    ++me->infinity;
    ++me->running_sum;
    return true;
}

void
basic_histogram_print(struct BasicHistogram *me)
{
    if (me == NULL || me->histogram == NULL) {
        printf("{}\n");
        return;
    }
    printf("{");
    for (uint64_t i = 0; i < me->length; ++i) {
        printf("\"" PRIu64 "\": " PRIu64 ", ", i, me->histogram[i]);
    }
    // NOTE I assume me->length is much less than SIZE_MAX
    printf("\"" PRIu64 "\": " PRIu64 ", ", me->length, me->false_infinity);
    printf("\"inf\": " PRIu64 "", me->infinity);
    printf("}\n");
}

void
basic_histogram_destroy(struct BasicHistogram *me)
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
