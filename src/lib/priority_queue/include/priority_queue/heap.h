/** @brief  This file implements a maximum-priority-heap, whereby the
 *          element with the maximum key (i.e. priority) is stored at
 *          the top of the heap.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "types/key_type.h"
#include "types/value_type.h"

struct HeapItem {
    KeyType key;
    ValueType value;
};

/// @brief  A binary heap with the maximum priority as the root.
struct Heap {
    struct HeapItem *data;
    size_t length;
    size_t capacity;
};

bool
Heap__validate(struct Heap const *const me);

bool
Heap__write_as_json(FILE *stream, struct Heap const *const me);

bool
Heap__init(struct Heap *me, size_t const max_size);

bool
Heap__is_full(struct Heap const *const me);

bool
Heap__insert_if_room(struct Heap *const me,
                     const KeyType hash,
                     const ValueType entry);

KeyType
Heap__get_max_key(struct Heap *me);

bool
Heap__remove(struct Heap *me, KeyType largest_key, ValueType *value_return);

void
Heap__destroy(struct Heap *me);
