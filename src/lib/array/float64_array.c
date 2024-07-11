#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <glib.h>

#include "array/float64_array.h"
#include "file/file.h"

bool
Float64Array__init(struct Float64Array *const me)
{
    if (me == NULL) {
        return false;
    }
    size_t const element_size = sizeof(double);
    *me = (struct Float64Array){.array = g_array_new(true, true, element_size)};
    if (me->array == NULL) {
        goto cleanup;
    }
    return true;
cleanup:
    Float64Array__destroy(me);
    return false;
}

void
Float64Array__destroy(struct Float64Array *const me)
{
    if (me == NULL) {
        return;
    }
    // NOTE This is not thread-safe. Use 'g_array_unref()' for thread safety.
    g_array_free(me->array, true);
    *me = (struct Float64Array){0};
}

bool
Float64Array__append(struct Float64Array *const me, double const item)
{
    if (me == NULL) {
        return false;
    }
    g_array_append_vals(me->array, &item, 1);
    return true;
}

bool
Float64Array__save(struct Float64Array const *const me, char const *const path)
{
    if (me == NULL) {
        return false;
    }
    return write_buffer(path,
                        me->array->data,
                        me->array->len,
                        g_array_get_element_size(me->array));
}
