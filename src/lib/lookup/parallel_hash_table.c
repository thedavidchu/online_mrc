#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

#include "hash/splitmix64.h"
#include "lookup/lookup.h"
#include "lookup/parallel_hash_table.h"
#include "lookup/parallel_list.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

bool
ParallelHashTable__init(struct ParallelHashTable *me, size_t num_buckets)
{
    if (me == NULL || num_buckets == 0)
        return false;
    *me = (struct ParallelHashTable){
        .table = (struct ParallelList *)calloc(sizeof(*me->table), num_buckets),
        .length = num_buckets};
    return true;
}

bool
ParallelHashTable__insert(struct ParallelHashTable *me,
                          EntryType entry,
                          TimeStampType timestamp)
{
    if (me == NULL || me->table == NULL || me->length == 0)
        return false;
    Hash64BitType hash = splitmix64_hash(entry);
    return ParallelList__put_unique(&me->table[hash % me->length],
                                entry,
                                timestamp);
}

bool
ParallelHashTable__update(struct ParallelHashTable *me,
                          EntryType entry,
                          TimeStampType new_timestamp)
{
    if (me == NULL || me->table == NULL || me->length == 0)
        return false;
    Hash64BitType hash = splitmix64_hash(entry);
    return ParallelList__put_unique(&me->table[hash % me->length],
                                entry,
                                new_timestamp);
}

struct LookupReturn
ParallelHashTable__lookup(struct ParallelHashTable *me, EntryType entry)
{
    struct LookupReturn r = {.success = false};
    if (me == NULL || me->table == NULL || me->length == 0)
        return r;
    Hash64BitType hash = splitmix64_hash(entry);
    return ParallelList__lookup(&me->table[hash % me->length], entry);
}

void
ParallelHashTable__destroy(struct ParallelHashTable *me)
{
    if (me == NULL)
        return;
    if (me->table == NULL || me->length == 0) {
        *me = (struct ParallelHashTable){0};
        return;
    }
    for (size_t i = 0; i < me->length; ++i) {
        ParallelList__destroy(&me->table[i]);
    }
    free(me->table);
    *me = (struct ParallelHashTable){0};
}

void
ParallelHashTable__print(struct ParallelHashTable *me)
{
    if (me == NULL || me->table == NULL || me->length == 0)
        return;

    printf("[%zu]{\n", me->length);
    for (size_t i = 0; i < me->length; ++i) {
        ParallelList__print(&me->table[i]);
        printf(", ");
    }
    printf("}\n");
}
