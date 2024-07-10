#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

struct Array {
    GArray *array;
};

bool
Array__init(struct Array *const me, size_t const init_capacity);

void
Array__destroy(struct Array *const me);

bool
Array__append(struct Array *const me, void *item);

bool
Array__save(struct Array const *const me, char const *const path);
