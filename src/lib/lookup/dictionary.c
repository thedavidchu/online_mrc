#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <glib.h>

#include "arrays/is_last.h"
#include "logger/logger.h"
#include "lookup/dictionary.h"
#include "lookup/lookup.h"

bool
Dictionary__init(struct Dictionary *const me)
{
    if (me == NULL) {
        return false;
    }
    me->hash_table =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (me->hash_table == NULL) {
        return false;
    }
    return true;
}

size_t
Dictionary__get_size(struct Dictionary const *const me)
{
    if (me == NULL || me->hash_table == NULL) {
        return 0;
    }
    return g_hash_table_size(me->hash_table);
}

char const *
Dictionary__get(struct Dictionary const *const me, char const *const key)
{
    if (me == NULL || me->hash_table == NULL || key == NULL) {
        return NULL;
    }
    return g_hash_table_lookup(me->hash_table, key);
}

enum PutUniqueStatus
Dictionary__put(struct Dictionary *const me,
                char const *const key,
                char const *const value)
{
    if (me == NULL || me->hash_table == NULL || key == NULL || value == NULL) {
        return LOOKUP_PUTUNIQUE_ERROR;
    }
    // NOTE The `g_hash_table_replace` replaces the current key with the
    //      passed key as well. Basically, it is identical except for
    //      some memory management stuff.
    gboolean r =
        g_hash_table_insert(me->hash_table, g_strdup(key), g_strdup(value));
    return r ? LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE
             : LOOKUP_PUTUNIQUE_REPLACE_VALUE;
}

bool
Dictionary__remove(struct Dictionary *const me, char const *const key)
{
    if (me == NULL || me->hash_table == NULL || key == NULL) {
        return false;
    }
    // NOTE I am just making sure that the GLib boolean result is exactly
    //      the same as the standard C boolean.
    return g_hash_table_remove(me->hash_table, key) ? true : false;
}

bool
Dictionary__write(struct Dictionary const *const me,
                  FILE *const stream,
                  bool const newline)
{
    if (stream == NULL) {
        return false;
    }
    if (me == NULL || me->hash_table == NULL) {
        fprintf(stream, "{}%s", newline ? "\n" : "");
        return true;
    }
    fprintf(stream, "{");

    GHashTableIter iter = {0};
    size_t size = Dictionary__get_size(me);
    g_hash_table_iter_init(&iter, me->hash_table);
    for (size_t i = 0; i < size; ++i) {
        gpointer key = NULL, value = NULL;
        gboolean r = g_hash_table_iter_next(&iter, &key, &value);
        if (!r) {
            LOGGER_WARN("unexpected early exit from hash table iter");
            break;
        }
        fprintf(stream,
                "\"%s\": \"%s\"%s",
                (char *)key,
                (char *)value,
                is_last(i, size) ? "" : ", ");
    }
    fprintf(stream, "}%s", newline ? "\n" : "");
    return true;
}

void
Dictionary__destroy(struct Dictionary *const me)
{
    if (me == NULL || me->hash_table == NULL) {
        return;
    }
    g_hash_table_destroy(me->hash_table);
    *me = (struct Dictionary){0};
}
