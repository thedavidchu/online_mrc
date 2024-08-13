/** Use the khash.h file to create a hash table with keys and values of
 *  type uint64_t. */

#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "khash.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

KHASH_MAP_INIT_INT64(64, uint64_t)

/// @brief  This implemenents a hash table with key and values of type
///         uint64_t. It uses the klib library backend.
struct KHashTable {
    khash_t(64) * hash_table;
};

bool
KHashTable__init(struct KHashTable *me)
{
    if (me == NULL)
        return false;
    me->hash_table = kh_init(64);
    if (me->hash_table == NULL)
        return false;
    return true;
}

size_t
KHashTable__get_size(struct KHashTable const *const me)
{
    return kh_size(me->hash_table);
}

struct LookupReturn
KHashTable__lookup(struct KHashTable *me, EntryType key)
{
    if (me == NULL || me->hash_table == NULL)
        return (struct LookupReturn){.success = false, .timestamp = 0};
    khiter_t k = kh_get(64, me->hash_table, key);
    bool found = (k != kh_end(me->hash_table));
    uint64_t value = kh_value(me->hash_table, k);
    return (struct LookupReturn){.success = found, .timestamp = value};
}

/// @return Returns whether we inserted, replaced, or errored.
enum PutUniqueStatus
KHashTable__put_unique(struct KHashTable *me,
                       EntryType key,
                       TimeStampType value)
{

    if (me == NULL || me->hash_table == NULL)
        return LOOKUP_PUTUNIQUE_ERROR;

    int ret = 0;
    khint_t k = kh_put(64, me->hash_table, key, &ret);
    if (ret == -1)
        return LOOKUP_PUTUNIQUE_ERROR;
    kh_value(me->hash_table, k) = value;

    // NOTE The value of 'ret' is 0 if the key exists; it is 1 or 2 if
    //      the bucket was empty or deleted (respectively). This is why
    //      I test for a value of zero, since the other two numbers both
    //      imply an insertion.
    return ret == 0 ? LOOKUP_PUTUNIQUE_REPLACE_VALUE
                    : LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE;
}

struct LookupReturn
KHashTable__remove(struct KHashTable *const me, EntryType const key)
{
    if (me == NULL || me->hash_table == NULL)
        return (struct LookupReturn){.success = false, .timestamp = 0};

    khiter_t k;
    k = kh_get(64, me->hash_table, key);
    bool const found = (k != kh_end(me->hash_table));
    uint64_t const stolen_value = kh_value(me->hash_table, k);
    kh_del(64, me->hash_table, k);
    // NOTE We assume that a found item implies a successful deletion.
    //      This obviously means that this is not thread safe.
    return (struct LookupReturn){.success = found,
                                 .timestamp = (TimeStampType)stolen_value};
}

bool
KHashTable__write(struct KHashTable const *const me,
                  FILE *const stream,
                  bool const newline)
{
    if (me == NULL || me->hash_table == NULL)
        return false;
    fprintf(stream, "{");
    for (khiter_t k = kh_begin(me->hash_table); k != kh_end(me->hash_table);
         ++k) {
        // NOTE We could remove the trailing comma if we keep track of
        //      how many items we are printing versus how many we have
        //      already printed. When we see the last item, then we do
        //      not print the trailing comma. Alternatively, we could
        //      print the first item (if it exists) without the trailing
        //      comma and then print all other items with a leading
        //      comma. I don't feel like doing these things though.
        if (kh_exist(me->hash_table, k)) {
            fprintf(stream,
                    "%" PRIu64 ": %" PRIu64 ", ",
                    kh_key(me->hash_table, k),
                    kh_value(me->hash_table, k));
        }
    }
    fprintf(stream, "}%s", newline ? "\n" : "");
    return true;
}

void
KHashTable__destroy(struct KHashTable *me)
{
    if (me == NULL || me->hash_table == NULL)
        return;

    kh_destroy(64, me->hash_table);
}
