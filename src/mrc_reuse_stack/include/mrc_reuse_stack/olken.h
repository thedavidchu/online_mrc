#pragma once

#include <stddef.h>

#include <glib.h>

#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"

typedef size_t EntryType;
typedef size_t TimeStampType;

const size_t MAX_HISTOGRAM_LENGTH = 1 << 20;

struct OlkenReuseStack {
    struct Tree *tree;
    GHashTable *hash_table;
    size_t *histogram;
    TimeStampType current_time_stamp;
    size_t infinite_distance;
};

struct OlkenReuseStack *
olken_reuse_stack_new();

void
olken_reuse_stack_access_item(struct OlkenReuseStack *me, EntryType entry);

void
olken_reuse_stack_print_sparse_histogram(struct OlkenReuseStack *me);

void
olken_reuse_stack_free(struct OlkenReuseStack *me);
