/// @brief  A fixed-size priority queue implemented with a splay tree.

#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "hash/types.h"
#include "types/entry_type.h"

struct SubtreeMultimap {
    Hash64BitType key;
    EntryType value;
    uint64_t cardinality;
    struct SubtreeMultimap *left_subtree;
    struct SubtreeMultimap *right_subtree;
};

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
splay_priority_queue__init(struct SplayPriorityQueue *me, const uint64_t max_cardinality);

bool
splay_priority_queue__is_full(struct SplayPriorityQueue *me);

bool
splay_priority_queue__insert_if_room(struct SplayPriorityQueue *me,
                                     const Hash64BitType hash,
                                     const EntryType entry);

Hash64BitType
splay_priority_queue__get_max_hash(struct SplayPriorityQueue *me);

bool
splay_priority_queue__remove(struct SplayPriorityQueue *me,
                             Hash64BitType largest_hash,
                             EntryType *entry);

void
splay_priority_queue__destroy(struct SplayPriorityQueue *me);
