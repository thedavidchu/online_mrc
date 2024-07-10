#pragma once

#include <stdbool.h>
#include <stddef.h>

#include <glib.h>

struct Float64Array {
    GArray *array;
};

bool
Float64Array__init(struct Float64Array *const me);

void
Float64Array__destroy(struct Float64Array *const me);

bool
Float64Array__append(struct Float64Array *const me, double const item);

bool
Float64Array__save(struct Float64Array const *const me, char const *const path);
