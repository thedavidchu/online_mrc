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

struct Binary64Array {
    GArray *array;
};

bool
Binary64Array__init(struct Binary64Array *const me);

void
Binary64Array__destroy(struct Binary64Array *const me);

bool
Binary64Array__append(struct Binary64Array *const me,
                      void const *const item_ptr);

bool
Binary64Array__save(struct Binary64Array const *const me,
                    char const *const path);
