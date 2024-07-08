#include <assert.h>
#include <stdbool.h>
#include <stdio.h>

#include <glib.h>

#include "arrays/is_last.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

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
    // NOTE Using the 'g_direct_hash' function means that we need to pass our
    //      entries as pointers to the hash table. It also means we cannot
    //      destroy the values at the pointers, because the pointers are our
    //      actual values!
    // NOTE By passing 'NULL' instead of 'g_direct_equal', we save a function
    //      call, as per the documentation.
    me->hash_table = g_hash_table_new(g_direct_hash, NULL);
    if (me->hash_table == NULL)
        return false;
    return true;
}

size_t
HashTable__get_size(struct HashTable const *const me)
{
    return g_hash_table_size(me->hash_table);
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

struct LookupReturn
HashTable__remove(struct HashTable *const me, EntryType const key)
{
    if (me == NULL || me->hash_table == NULL)
        return (struct LookupReturn){.success = false, .timestamp = 0};

    gpointer stolen_value = NULL;
    gboolean r = g_hash_table_steal_extended(me->hash_table,
                                             (gconstpointer)key,
                                             NULL,
                                             &stolen_value);
    return (struct LookupReturn){.success = gboolean_to_bool(r),
                                 .timestamp = (TimeStampType)stolen_value};
}

static void
write_ghashtable_as_json(FILE *stream,
                         GHashTable *const hash_table,
                         bool const new_line)
{
    assert(stream != NULL && hash_table != NULL);

    guint size = g_hash_table_size(hash_table);
    fprintf(stream,
            "{\"type\": \"GHashTable\", \".size\": %u, \".data\": {",
            size);
    GHashTableIter iter = {0};
    g_hash_table_iter_init(&iter, hash_table);
    for (size_t i = 0; i < size; ++i) {
        gpointer key = NULL, value = NULL;
        gboolean r = g_hash_table_iter_next(&iter, &key, &value);
        if (!r) {
            LOGGER_WARN("unexpected early exit from hash table iter");
            break;
        }
        fprintf(stream,
                "\"%" PRIu64 "\": %" PRIu64,
                (uint64_t)key,
                (uint64_t)value);
        if (!is_last(i, size)) {
            fprintf(stream, ", ");
        }
    }
    fprintf(stream, "}}");

    if (new_line)
        fprintf(stream, "\n");
}

void
HashTable__write_as_json(FILE *stream, struct HashTable const *const me)
{
    if (stream == NULL) {
        LOGGER_WARN("stream == NULL");
        return;
    }
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    if (me->hash_table == NULL) {
        fprintf(stream, "{\"type\": \"HashTable\", \".hash_table\": null}\n");
        return;
    }
    fprintf(stream, "{\"type\": \"HashTable\", \".hash_table\": ");
    write_ghashtable_as_json(stream, me->hash_table, false);
    fprintf(stream, "}\n");
}

void
HashTable__destroy(struct HashTable *me)
{
    if (me == NULL || me->hash_table == NULL)
        return;

    g_hash_table_destroy(me->hash_table);
}
