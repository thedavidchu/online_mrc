#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "logger/logger.h"
#include "lookup/sampled_hash_table.h"
#include "math/ratio.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"

/// Source: https://en.wikipedia.org/wiki/HyperLogLog#Practical_considerations
static double
hll_alpha_m(size_t m)
{
    if (m == 16) {
        return 0.673;
    } else if (m == 32) {
        return 0.697;
    } else if (m == 64) {
        return 0.709;
    } else if (m >= 128) {
        return 0.7213 / (1 + 1.079 / m);
    } else {
        LOGGER_WARN(
            "unsupported HyperLogLog size of %zu, not using fudge factor!",
            m);
        return 1.0;
    }
}

bool
EvictingHashTable__init(struct EvictingHashTable *me,
                        const size_t length,
                        const double init_sampling_ratio)
{
    if (me == NULL || length == 0 || init_sampling_ratio <= 0.0 ||
        init_sampling_ratio > 1.0)
        return false;

    struct EvictingHashTableNode *data = malloc(length * sizeof(*me->data));
    if (data == NULL)
        return false;
    // NOTE There is no way to allow us to sample values with a hash of
    //      UINT64_MAX because we reserve this as the "invalid" hash value.
    for (size_t i = 0; i < length; ++i) {
        data[i].hash = UINT64_MAX;
    }

    *me = (struct EvictingHashTable){
        .data = data,
        .length = length,
        // HACK Set the threshold to some low number to begin
        //      (otherwise, we end up with teething performance issues).
        .global_threshold = ratio_uint64(init_sampling_ratio),
        .num_inserted = 0,
        .running_denominator = 0,
        .hll_alpha_m = hll_alpha_m(length),
    };
    return true;
}

struct SampledLookupReturn
EvictingHashTable__lookup(struct EvictingHashTable *me, KeyType key)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = splitmix64_hash(key);
    if (hash > me->global_threshold)
        return (struct SampledLookupReturn){.status = SAMPLED_IGNORED};

    struct EvictingHashTableNode *incumbent = &me->data[hash % me->length];
    if (incumbent->hash == UINT64_MAX)
        return (struct SampledLookupReturn){.status = SAMPLED_HITHERTOEMPTY};
    if (hash < incumbent->hash)
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};
    // NOTE If the key comparison is expensive, then one could first
    //      compare the hashes. However, in this case, they are not expensive.
    if (key == incumbent->key)
        return (struct SampledLookupReturn){.status = SAMPLED_FOUND,
                                            .hash = incumbent->hash,
                                            .timestamp = incumbent->value};
    return (struct SampledLookupReturn){.status = SAMPLED_IGNORED};
}

struct SampledPutReturn
EvictingHashTable__put_unique(struct EvictingHashTable *me,
                              KeyType key,
                              ValueType value)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledPutReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = splitmix64_hash(key);
    struct EvictingHashTableNode *incumbent = &me->data[hash % me->length];

    // HACK Note that the hash value of UINT64_MAX is reserved to mark
    //      the bucket as "invalid" (i.e. no valid element has been inserted).
    if (incumbent->hash == UINT64_MAX) {
        TimeStampType old_timestamp = incumbent->value;
        *incumbent = (struct EvictingHashTableNode){.key = key,
                                                    .hash = hash,
                                                    .value = value};
        return (struct SampledPutReturn){.status = SAMPLED_INSERTED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    if (hash < incumbent->hash) {
        TimeStampType old_timestamp = incumbent->value;
        *incumbent = (struct EvictingHashTableNode){.key = key,
                                                    .hash = hash,
                                                    .value = value};
        return (struct SampledPutReturn){.status = SAMPLED_REPLACED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    // NOTE If the key comparison is expensive, then one could first
    //      compare the hashes. However, in this case, they are not expensive.
    if (key == incumbent->key) {
        TimeStampType old_timestamp = incumbent->value;
        incumbent->value = value;
        return (struct SampledPutReturn){.status = SAMPLED_UPDATED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    return (struct SampledPutReturn){.status = SAMPLED_IGNORED};
}

void
EvictingHashTable__refresh_threshold(struct EvictingHashTable *me)
{
    Hash64BitType max_hash = 0;
    for (size_t i = 0; i < me->length; ++i) {
        Hash64BitType hash = me->data[i].hash;
        if (hash > max_hash)
            max_hash = hash;
    }
    me->global_threshold = max_hash;
}

static void
print_EvictingHashTableNode(struct EvictingHashTableNode *me,
                            uint64_t invalid_hash)
{
    if (me->hash == invalid_hash) {
        printf("null");
    } else {
        printf("[%" PRIu64 ", %" PRIu64 ", %" PRIu64 "]",
               me->key,
               me->hash,
               me->value);
    }
}

void
EvictingHashTable__print_as_json(struct EvictingHashTable *me)
{
    if (me == NULL) {
        printf("{\"type\": null}");
        return;
    }
    if (me->data == NULL) {
        printf("{\"type\": \"EvictingHashTable\", \".data\": null}");
        return;
    }

    printf("{\"type\": \"EvictingHashTable\", \".length\": %" PRIu64
           ", \".data\": [",
           me->length);
    for (size_t i = 0; i < me->length; ++i) {
        print_EvictingHashTableNode(&me->data[i], UINT64_MAX);
        if (i < me->length - 1) {
            printf(", ");
        }
    }
    printf("]}\n");
}

double
EvictingHashTable__estimate_num_unique(struct EvictingHashTable *me)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return 0.0;
    double avg_nlz = me->num_inserted / (me->running_denominator + DBL_EPSILON);
    // NOTE I add this sketchy subtraction by 1 to compensate for my sketchy
    //      addition by 1 for each element. On a simple trace, this matches the
    //      actual number of elements better, so I am content. However, I do not
    //      understand what I'm doing.
    return me->hll_alpha_m * me->num_inserted * exp2(avg_nlz - 1);
}

void
EvictingHashTable__destroy(struct EvictingHashTable *me)
{
    if (me == NULL)
        return;
    free(me->data);
    *me = (struct EvictingHashTable){0};
}
