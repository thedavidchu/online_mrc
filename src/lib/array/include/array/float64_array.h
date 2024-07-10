/** It is times like these that I wish I were using C++ for its Standard
 *  Template Library (STL). Remind me again, why did I choose to use
 *  pure C? And not just any C, but the archaic C99.
 *
 *  Oh, right. It's because I wanted the experience of writing C code
 *  and because I would probably have made many rookie mistakes in C++
 *  that
 *
 *  Also, PARDA's codebase was written in C, so I just stuck with it.
 */
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
