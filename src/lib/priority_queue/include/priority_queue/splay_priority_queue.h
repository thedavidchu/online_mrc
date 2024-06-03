/// @brief  A fixed-size priority queue implemented with a splay tree.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hash/types.h"
#include "types/entry_type.h"

struct SubtreeMultimap;

struct SplayPriorityQueue {
    struct SubtreeMultimap *root;
    uint64_t cardinality;

    // This is the free list. I was going to make it into a separate structure,
    // but meh. Too much work.
    struct SubtreeMultimap *all_subtrees;
    struct SubtreeMultimap **free_subtrees;
    uint64_t num_free_subtrees;

    // The maximum cardinality of the root (and also incidentally the sum of the
    // cardinality and the num_free_subtrees)
    uint64_t max_cardinality;
};

bool
SplayPriorityQueue__init(struct SplayPriorityQueue *me,
                         const uint64_t max_cardinality);

bool
SplayPriorityQueue__is_full(struct SplayPriorityQueue *me);

bool
SplayPriorityQueue__insert_if_room(struct SplayPriorityQueue *me,
                                   const Hash64BitType hash,
                                   const EntryType entry);

Hash64BitType
SplayPriorityQueue__get_max_hash(struct SplayPriorityQueue *me);

bool
SplayPriorityQueue__remove(struct SplayPriorityQueue *me,
                           Hash64BitType largest_hash,
                           EntryType *entry);

void
SplayPriorityQueue__destroy(struct SplayPriorityQueue *me);
