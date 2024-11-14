/** @brief  This file implements a {min,max}-priority-heap, whereby the
 *          element with the {min,max} key (i.e. priority) is stored at
 *          the top of the heap.
 *  @todo   Change the names to make this min/max-heap agnostic.
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
    bool (*cmp)(KeyType const lhs, KeyType const rhs);
    // The key that causes an item to be moved to the bottom of the heap.
    size_t bottom_key;
};

bool
Heap__validate(struct Heap const *const me);

bool
Heap__write_as_json(FILE *stream, struct Heap const *const me);

bool
Heap__init_max_heap(struct Heap *me, size_t const max_size);

bool
Heap__init_min_heap(struct Heap *me, size_t const max_size);

bool
Heap__is_full(struct Heap const *const me);

bool
Heap__insert_if_room(struct Heap *const me,
                     const KeyType hash,
                     const ValueType entry);

/// @brief  Insert key/value with resize if necessary.
bool
Heap__insert(struct Heap *const me, const KeyType key, const ValueType value);

/// @brief  Get the key at the 'top' of the queue (i.e. in position 0).
/// @return Returns key at position 0; otherwise 0 if error.
KeyType
Heap__get_top_key(struct Heap *me);

/// @brief  Remove the top key from the heap if it matches the 'rm_key'.
/// @todo   Consider changing the semantics of this to delete the key
///         even without an exact match to the 'rm_key'. For example, in
///         a max-heap, this function would delete a key if it is
///         greater-than-or-equal-to 'rm_key'.
bool
Heap__remove(struct Heap *me, KeyType rm_key, ValueType *value_return);

void
Heap__destroy(struct Heap *me);
