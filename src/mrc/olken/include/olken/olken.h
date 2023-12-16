#pragma once

#include <stdint.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct OlkenReuseStack {
    struct Tree tree;
    GHashTable *hash_table;
    struct BasicHistogram histogram;
    TimeStampType current_time_stamp;
};

bool
olken_reuse_stack_init(struct OlkenReuseStack *me, const uint64_t max_num_unique_entries);

void
olken_reuse_stack_access_item(struct OlkenReuseStack *me, EntryType entry);

void
olken_reuse_stack_print_sparse_histogram(struct OlkenReuseStack *me);

void
olken_reuse_stack_destroy(struct OlkenReuseStack *me);
