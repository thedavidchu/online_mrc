/** @brief  An associative string-string lookup where this data structure
 *          owns the strings.
 *  @note   This copies strings by value, so any strings passed in remain
 *          owned by the caller. Any strings returned by this library are
 *          merely borrowed.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

#include <glib.h>

#include "lookup/lookup.h"

struct Dictionary {
    GHashTable *hash_table;
};

bool
Dictionary__init(struct Dictionary *const me);

size_t
Dictionary__get_size(struct Dictionary const *const me);

char const *
Dictionary__get(struct Dictionary const *const me, char const *const key);

enum PutUniqueStatus
Dictionary__put(struct Dictionary *const me,
                char const *const key,
                char const *const value);

bool
Dictionary__remove(struct Dictionary *const me, char const *const key);

bool
Dictionary__write(struct Dictionary const *const me,
                  FILE *const stream,
                  bool const newline);

/// @brief  Parse a dictionary from a string.
///         c.f. string-only JSON objects, https://www.json.org/json-en.html
/// @param  me: an initialized dictionary.
/// @example    Valid: '{"abc": "123", "def": "345",}'
/// @note   I do not support non-strings or escape sequences with '\uABCD', etc.
/// @note   For duplicated keys, the current implementation takes the
///         value of the second key. However, I do not guarantee this.
char const *
Dictionary__read(struct Dictionary *const me, char const *const str);

void
Dictionary__destroy(struct Dictionary *const me);
