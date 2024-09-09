#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <glib.h>

#include "arrays/is_last.h"
#include "logger/logger.h"
#include "lookup/dictionary.h"
#include "lookup/lookup.h"

static bool
initialize(struct Dictionary *const me)
{
    assert(me != NULL);
    me->hash_table =
        g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);
    if (me->hash_table == NULL) {
        return false;
    }
    return true;
}

bool
Dictionary__init(struct Dictionary *const me)
{
    if (me == NULL) {
        return false;
    }
    return initialize(me);
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
        fprintf(stream, "(null)%s", newline ? "\n" : "");
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

enum NextToken {
    LEFT_BRACE,
    KEY_OR_END,
    COLON,
    VALUE,
    COMMA_OR_END,
};

/// @return Returns the position of the final quotation or SIZE_MAX on error.
static size_t
parse_string(char const *const str, size_t const start_idx, size_t const length)
{
    assert(str != NULL);
    assert(start_idx < length);
    assert(str[start_idx] == '"');

    bool escape = false;
    for (size_t i = start_idx + 1; i < length; ++i) {
        char const c = str[i];
        if (escape) {
            // Parse other escaped characters
            if (c == '"' || c == '\\' || c == '/' || c == 'b' || c == 'f' ||
                c == 'n' || c == 'r' || c == 't') {
                escape = false;
            }
            // Otherwise, unrecognized escape! N.B. We do not conform to
            // JSON strings, which support '\uABCD'.
            else {
                LOGGER_WARN("unrecognized escape character '%c'", c);
                escape = false;
            }
        } else if (c == '\\') {
            escape = true;
        } else if (c == '"') {
            return i;
        }
    }
    LOGGER_ERROR("no end quote for string '%s'", &str[start_idx]);
    return SIZE_MAX;
}

static enum PutUniqueStatus
Dictionary__put_slice(struct Dictionary *const me,
                      char const *const key,
                      size_t const key_length,
                      char const *const value,
                      size_t const value_length)
{
    if (me == NULL || me->hash_table == NULL || key == NULL || value == NULL) {
        return LOOKUP_PUTUNIQUE_ERROR;
    }
    // NOTE The `g_hash_table_replace` replaces the current key with the
    //      passed key as well. Basically, it is identical except for
    //      some memory management stuff.
    gboolean r = g_hash_table_insert(me->hash_table,
                                     g_strndup(key, key_length),
                                     g_strndup(value, value_length));
    return r ? LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE
             : LOOKUP_PUTUNIQUE_REPLACE_VALUE;
}

char const *
Dictionary__read(struct Dictionary *const me, char const *const str)
{
    if (me == NULL || str == NULL) {
        return NULL;
    }
    if (!initialize(me)) {
        LOGGER_ERROR("failed to initialize dictionary");
        return NULL;
    }

    size_t const length = strlen(str);
    size_t key_start = 0;
    size_t key_end = 0;
    size_t value_start = 0;
    size_t value_end = 0;
    enum NextToken next = LEFT_BRACE;
    for (size_t i = 0; i < length; ++i) {
        char const c = str[i];
        if (isspace(c)) {
            continue;
        }
        switch (next) {
        case LEFT_BRACE:
            if (c != '{') {
                LOGGER_ERROR("expected left brace '{', got '%c'", c);
                goto err_cleanup;
            }
            next = KEY_OR_END;
            break;
        case KEY_OR_END:
            if (c == '}') {
                return &str[i + 1];
            }
            key_start = i;
            key_end = parse_string(str, i, length);
            if (key_end == SIZE_MAX) {
                LOGGER_ERROR("unable to parse key string");
                goto err_cleanup;
            }
            i = key_end;
            next = COLON;
            break;
        case COLON:
            if (c != ':') {
                LOGGER_ERROR("expected colon ':', got '%c'", c);
                goto err_cleanup;
            }
            next = VALUE;
            break;
        case VALUE:
            value_start = i;
            value_end = parse_string(str, i, length);
            if (value_end == SIZE_MAX) {
                LOGGER_ERROR("unable to parse value string");
                goto err_cleanup;
            }
            if (!Dictionary__put_slice(me,
                                       &str[key_start + 1],
                                       key_end - key_start - 1,
                                       &str[value_start + 1],
                                       value_end - value_start - 1)) {
                LOGGER_ERROR(
                    "failed to put key ('%.*s')/value('%.*s') into dictionary",
                    &str[key_start + 1],
                    key_end - key_start - 1,
                    &str[value_start + 1],
                    value_end - value_start - 1);
            }
            i = value_end;
            next = COMMA_OR_END;
            break;
        case COMMA_OR_END:
            if (c == ',') {
                next = KEY_OR_END;
            } else if (c == '}') {
                return &str[i + 1];
            } else {
                LOGGER_ERROR("expected comma ',' or right brace '}', got '%c'",
                             c);
                goto err_cleanup;
            }
            break;
        default:
            assert(0);
        }
    }
err_cleanup:
    Dictionary__destroy(me);
    return NULL;
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
