#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <glib.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "lookup/sampled_hash_table.h"
#include "math/ratio.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct SampledHashTableNode {
    KeyType key;
    Hash64BitType hash;
    ValueType value;
};

bool
SampledHashTable__init(struct SampledHashTable *me,
                       const size_t length,
                       const double init_sampling_ratio)
{
    const uint64_t threshold = ratio_uint64(init_sampling_ratio);
    if (me == NULL || length == 0 || init_sampling_ratio <= 0.0 ||
        init_sampling_ratio > 1.0)
        return false;

    struct SampledHashTableNode *data = malloc(length * sizeof(*me->data));
    if (data == NULL)
        return false;
    // HACK Set the threshold to some low number to begin (otherwise, we
    //      end up with teething performance issues)
    for (size_t i = 0; i < length; ++i) {
        data[i].hash = threshold;
    }

    *me = (struct SampledHashTable){
        .data = data,
        .length = length,
        .threshold = threshold,
    };
    return true;
}

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, KeyType key)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = splitmix64_hash(key);
    struct SampledHashTableNode *incumbent = &me->data[hash % me->length];

    if (hash < incumbent->hash) {
        return (struct SampledLookupReturn){.status = SAMPLED_NOTFOUND};
    } else if (hash == incumbent->hash && key == incumbent->key) {
        return (struct SampledLookupReturn){.status = SAMPLED_FOUND,
                                            .hash = incumbent->hash,
                                            .timestamp = incumbent->value};
    } else {
        return (struct SampledLookupReturn){.status = SAMPLED_NOTTRACKED};
    }
}

enum SampledStatus
SampledHashTable__put_unique(struct SampledHashTable *me,
                             KeyType key,
                             ValueType value)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return SAMPLED_NOTFOUND;

    Hash64BitType hash = splitmix64_hash(key);
    struct SampledHashTableNode *incumbent = &me->data[hash % me->length];

    if (hash < incumbent->hash) {
        *incumbent = (struct SampledHashTableNode){.key = key,
                                                   .hash = hash,
                                                   .value = value};
        return SAMPLED_REPLACED;
    } else if (hash == incumbent->hash && key == incumbent->key) {
        incumbent->value = value;
        return SAMPLED_UPDATED;
    } else {
        return SAMPLED_NOTTRACKED;
    }
}

void
SampledHashTable__refresh_threshold(struct SampledHashTable *me)
{
    Hash64BitType max_hash = 0;
    for (size_t i = 0; i < me->length; ++i) {
        Hash64BitType hash = me->data[i].hash;
        if (hash > max_hash)
            max_hash = hash;
    }
    me->threshold = max_hash;
}

static void
print_SampledHashTableNode(struct SampledHashTableNode *me,
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
SampledHashTable__print_as_json(struct SampledHashTable *me)
{
    if (me == NULL) {
        printf("{\"type\": null}");
        return;
    }
    if (me->data == NULL) {
        printf("{\"type\": \"SampledHashTable\", \".data\": null}");
        return;
    }

    printf("{\"type\": \"SampledHashTable\", \".length\": %" PRIu64
           ", \".data\": [",
           me->length);
    for (size_t i = 0; i < me->length; ++i) {
        print_SampledHashTableNode(&me->data[i], UINT64_MAX * 1e-3);
        if (i < me->length - 1) {
            printf(", ");
        }
    }
    printf("]}");
}

void
SampledHashTable__destroy(struct SampledHashTable *me)
{
    if (me == NULL)
        return;
    free(me->data);
    *me = (struct SampledHashTable){0};
}
