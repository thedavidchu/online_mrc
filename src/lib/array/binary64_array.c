#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>

#include <glib.h>

#include "array/binary64_array.h"
#include "file/file.h"

bool
Binary64Array__init(struct Binary64Array *const me)
{
    if (me == NULL) {
        return false;
    }
    size_t const element_size = sizeof(double);
    *me =
        (struct Binary64Array){.array = g_array_new(true, true, element_size)};
    if (me->array == NULL) {
        goto cleanup;
    }
    return true;
cleanup:
    Binary64Array__destroy(me);
    return false;
}

void
Binary64Array__destroy(struct Binary64Array *const me)
{
    if (me == NULL) {
        return;
    }
    // NOTE This is not thread-safe. Use 'g_array_unref()' for thread safety.
    g_array_free(me->array, true);
    *me = (struct Binary64Array){0};
}

bool
Binary64Array__append(struct Binary64Array *const me,
                      void const *const item_ptr)
{
    if (me == NULL) {
        return false;
    }
    g_array_append_vals(me->array, &item_ptr, 1);
    return true;
}

bool
Binary64Array__save(struct Binary64Array const *const me,
                    char const *const path)
{
    if (me == NULL) {
        return false;
    }
    return write_buffer(path,
                        me->array->data,
                        me->array->len,
                        g_array_get_element_size(me->array));
}
