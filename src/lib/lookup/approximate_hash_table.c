#include <stdbool.h>
#include <stdlib.h>

#include "lookup/approximate_hash_table.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"

struct SampledHashTableNode {
    EntryType entry;
    TimeStampType timestamp;
}

bool
SampledHashTable__init(struct SampledHashTable *me, const uint64_t length)
{
    if (me == NULL)
        return false;

    me->data = calloc(length, sizeof(*me->data));
    me->length = length;
    return true;
}

struct SampledLookupReturn
SampledHashTable__lookup(struct SampledHashTable *me, EntryType key)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return false;

    Hash64BitType hash = splitmix64_hash(key);
    struct SampledHashTableNode *incumbent = &me->data[hash % me->length];
    
    if (hash < me->hash) {
        // TODO Replace it
    } else if (hash == me->hash && key == me->entry) {
        // TODO Update timestamp
    } else {
        // TODO Nothing!
    }
}

enum SampledStatus
SampledHashTable__put_unique(struct SampledHashTable *me, EntryType key, TimeStampType value);

void
SampledHashTable__destroy(struct SampledHashTable *me);
