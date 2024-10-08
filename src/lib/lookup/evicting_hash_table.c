#include <assert.h>
#include <float.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "array/print_array.h"
#include "hash/hash.h"
#include "hash/types.h"
#include "logger/logger.h"
#include "lookup/evicting_hash_table.h"
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

    ValueType *data = malloc(length * sizeof(*me->values));
    if (data == NULL) {
        LOGGER_ERROR("failed to initialize with length %zu", length);
        return false;
    }
    Hash64BitType *hashes = malloc(length * sizeof(*hashes));
    if (hashes == NULL) {
        LOGGER_ERROR("failed to initialize hashes with length %zu", length);
        return false;
    }
    // NOTE I'm not sure what the best way of representing all 1's. I
    //      don't want to rely on two's complement. I think it's safest
    //      to assume that 0 is represented by all zeros.
    memset(hashes, ~0, length * sizeof(*hashes));

    *me = (struct EvictingHashTable){
        .values = data,
        .hashes = hashes,
        .length = length,
        .init_sampling_ratio = init_sampling_ratio,
        // HACK Set the threshold to some low number to begin
        //      (otherwise, we end up with teething performance issues).
        .global_threshold = ratio_uint64(init_sampling_ratio),
        .num_inserted = 0,
        // NOTE This is the sum of reciprocals, which is why we are
        //      multiplying by the init_sampling_ratio rather than
        //      dividing.
        .running_denominator = length * init_sampling_ratio,
        .hll_alpha_m = hll_alpha_m(length),
        .track_global_threshold = true,
    };
    return true;
}

struct SampledLookupReturn
EvictingHashTable__lookup(struct EvictingHashTable *me, KeyType key)
{
    if (!me || !me->hashes || !me->values || me->length == 0)
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = Hash64Bit(key);
    if (hash > me->global_threshold)
        return (struct SampledLookupReturn){.status = SAMPLED_IGNORED};

    ValueType incumbent = me->values[hash % me->length];
    Hash64BitType const old_hash = me->hashes[hash % me->length];
    if (hash == old_hash)
        return (struct SampledLookupReturn){.status = SAMPLED_FOUND,
                                            .hash = old_hash,
                                            .timestamp = incumbent};
    if (hash < old_hash)
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};
    if (old_hash == UINT64_MAX)
        return (struct SampledLookupReturn){.status = SAMPLED_HITHERTOEMPTY};
    return (struct SampledLookupReturn){.status = SAMPLED_IGNORED};
}

struct SampledPutReturn
EvictingHashTable__put(struct EvictingHashTable *me,
                       KeyType key,
                       ValueType value)
{
    if (!me || !me->hashes || !me->values || me->length == 0)
        return (struct SampledPutReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = Hash64Bit(key);
    ValueType *incumbent = &me->values[hash % me->length];
    Hash64BitType *const hash_ptr = &me->hashes[hash % me->length];
    Hash64BitType const old_hash = *hash_ptr;

    ++me->num_inserted;
    if (me->num_inserted == me->length) {
        EvictingHashTable__refresh_threshold(me);
    }
    // HACK Note that the hash value of UINT64_MAX is reserved to mark
    //      the bucket as "invalid" (i.e. no valid element has been inserted).
    if (old_hash == UINT64_MAX) {
        TimeStampType old_timestamp = *incumbent;
        *incumbent = value;
        *hash_ptr = hash;
        return (struct SampledPutReturn){.status = SAMPLED_INSERTED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    if (hash < old_hash) {
        TimeStampType old_timestamp = *incumbent;
        *incumbent = value;
        *hash_ptr = hash;
        return (struct SampledPutReturn){.status = SAMPLED_REPLACED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    // NOTE If the key comparison is expensive, then one could first
    //      compare the hashes. However, in this case, they are not expensive.
    if (hash == old_hash) {
        TimeStampType old_timestamp = *incumbent;
        *incumbent = value;
        return (struct SampledPutReturn){.status = SAMPLED_UPDATED,
                                         .new_hash = hash,
                                         .old_timestamp = old_timestamp};
    }
    return (struct SampledPutReturn){.status = SAMPLED_IGNORED};
}

void
EvictingHashTable__refresh_threshold(struct EvictingHashTable *me)
{
    if (!me || !me->hashes || !me->values || me->length == 0)
        return;
    Hash64BitType max_hash = 0;
    for (size_t i = 0; i < me->length; ++i) {
        Hash64BitType hash = me->hashes[i];
        if (hash > max_hash)
            max_hash = hash;
    }
    me->global_threshold = max_hash;
}

void
EvictingHashTable__print_as_json(struct EvictingHashTable *me)
{
    if (me == NULL) {
        printf("{\"type\": null}");
        return;
    }
    printf("{\"type\": \"EvictingHashTable\", \".length\": %zu, ", me->length);
    printf("\".hashes\": ");
    if (me->hashes == NULL) {
        // NOTE The 'print_array' function prints "(null)" when passed a
        //      null-pointer, whereas I only want a "null".
        printf("null");
    } else {
        print_array(stdout,
                    me->hashes,
                    me->length,
                    sizeof(me->hashes),
                    false,
                    _print_uint64);
    }
    printf(", \".values\": ");
    if (me->values == NULL) {
        printf("null");
    } else {
        print_array(stdout,
                    me->hashes,
                    me->length,
                    sizeof(me->hashes),
                    false,
                    _print_uint64);
    }
    printf("}\n");
}

void
EvictingHashTable__destroy(struct EvictingHashTable *me)
{
    if (me == NULL)
        return;
    free(me->values);
    free(me->hashes);
    *me = (struct EvictingHashTable){0};
}
