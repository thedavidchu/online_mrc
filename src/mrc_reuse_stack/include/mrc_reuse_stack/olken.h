#pragma once

#include <stdint.h>

#include <glib.h>

#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"

typedef uint64_t EntryType;
typedef uint64_t TimeStampType;

const uint64_t MAX_HISTOGRAM_LENGTH = 1 << 20;

struct OlkenReuseStack {
    struct Tree *tree;
    GHashTable *hash_table;
    uint64_t *histogram;
    TimeStampType current_time_stamp;
    uint64_t infinite_distance;
};

struct OlkenReuseStack *
olken_reuse_stack_new();

void
olken_reuse_stack_access_item(struct OlkenReuseStack *me, EntryType entry);

void
olken_reuse_stack_print_sparse_histogram(struct OlkenReuseStack *me);

void
olken_reuse_stack_free(struct OlkenReuseStack *me);
