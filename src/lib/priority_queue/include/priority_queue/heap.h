#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/types.h"
#include "types/entry_type.h"

struct HeapItem {
    Hash64BitType priority;
    EntryType entry;
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
Heap__init(struct Heap *me, size_t const max_size);

bool
Heap__is_full(struct Heap const *const me);

bool
Heap__insert_if_room(struct Heap *const me,
                     const Hash64BitType hash,
                     const EntryType entry);

Hash64BitType
Heap__get_max_hash(struct Heap *me);

bool
Heap__remove(struct Heap *me, Hash64BitType largest_hash, EntryType *entry);

void
Heap__destroy(struct Heap *me);
