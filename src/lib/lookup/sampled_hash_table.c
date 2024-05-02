#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

#include "hash/splitmix64.h"
#include "hash/types.h"
#include "lookup/sampled_hash_table.h"
#include "types/key_type.h"
#include "types/value_type.h"

struct SampledHashTableNode {
    KeyType key;
    Hash64BitType hash;
    ValueType value;
};

bool
SampledHashTable__init(struct SampledHashTable *me, const size_t length)
{
    if (me == NULL)
        return false;

    me->data = calloc(length, sizeof(*me->data));
    me->length = length;
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

static void
print_SampledHashTableNode(struct SampledHashTableNode *me)
{
    printf("[%" PRIu64 ", %" PRIu64", %" PRIu64 "]", me->key, me->hash, me->value);
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

    printf("{\"type\": \"SampledHashTable\", \".length\": %" PRIu64 ", \".data\": [", me->length);
    for (size_t i = 0; i < me->length; ++i) {
        print_SampledHashTableNode(&me->data[i]);
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
