#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <glib.h>

#include "arrays/arrays.h"
#include "file/file.h"

bool
Array__init(struct Array *const me, size_t const element_size)
{
    if (me == NULL || element_size == 0) {
        return false;
    }
    *me = (struct Array){.array = g_array_new(true, true, element_size)};
    if (me->array == NULL) {
        goto cleanup;
    }
    return true;
cleanup:
    Array__destroy(me);
    return false;
}

void
Array__destroy(struct Array *const me)
{
    if (me == NULL) {
        return;
    }
    // NOTE This is not thread-safe. Use 'g_array_unref()' for thread safety.
    g_array_free(me->array, true);
    *me = (struct Array){0};
}

bool
Array__append(struct Array *const me, void *item)
{
    if (me == NULL) {
        return false;
    }
    g_array_append_vals(me->array, item, 1);
    return true;
}

bool
Array__save(struct Array const *const me, char const *const path)
{
    if (me == NULL) {
        return false;
    }
    return write_buffer(path,
                        me->array->data,
                        me->array->len,
                        g_array_get_element_size(me->array));
}
