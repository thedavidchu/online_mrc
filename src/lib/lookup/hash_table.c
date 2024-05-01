#include <stdbool.h>

#include <glib.h>

#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"

static gboolean
entry_compare(gconstpointer a, gconstpointer b)
{
    return (a == b) ? TRUE : FALSE;
}

static inline bool
gboolean_to_bool(const gboolean b)
{
    // NOTE I am not comfortable assuming that gboolean == bool, so I do
    //      an explicit conversion.
    return b ? true : false;
}

bool
HashTable__init(struct HashTable *me)
{
    if (me == NULL)
        return false;
    // NOTE Using the g_direct_hash function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    me->hash_table = g_hash_table_new(g_direct_hash, entry_compare);
    if (me->hash_table == NULL)
        return false;
    return true;
}

struct LookupReturn
HashTable__lookup(struct HashTable *me, EntryType key)
{
    if (me == NULL || me->hash_table == NULL)
        return (struct LookupReturn){.success = false, .timestamp = 0};

    gboolean found = FALSE;
    uint64_t value = 0;
    found = g_hash_table_lookup_extended(me->hash_table,
                                         (gconstpointer)key,
                                         NULL,
                                         (gpointer *)&value);
    return (struct LookupReturn){.success = gboolean_to_bool(found),
                                 .timestamp = value};
}

/// @return Returns whether we inserted, replaced, or errored.
enum PutUniqueStatus
HashTable__put_unique(struct HashTable *me, EntryType key, TimeStampType value)
{

    if (me == NULL || me->hash_table == NULL)
        return LOOKUP_PUTUNIQUE_ERROR;

    // NOTE The `g_hash_table_replace` replaces the current key with the passed
    //      key as well. Basically, it is identical except for some memory
    //      management stuff.
    gboolean r =
        g_hash_table_insert(me->hash_table, (gpointer)key, (gpointer)value);
    return r ? LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE
             : LOOKUP_PUTUNIQUE_REPLACE_VALUE;
}

void
HashTable__destroy(struct HashTable *me)
{
    if (me == NULL || me->hash_table == NULL)
        return;

    g_hash_table_destroy(me->hash_table);
}
