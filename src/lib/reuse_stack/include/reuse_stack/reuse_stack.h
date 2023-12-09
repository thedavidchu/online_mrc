#pragma once

#include <stddef.h>

#include <glib.h>

#include "tree/naive_tree.h"
#include "tree/sleator_tree.h"

typedef size_t EntryType;
typedef size_t TimeStampType;

const size_t MAX_HISTOGRAM_LENGTH = 1 << 20;

struct ReuseStack {
    struct Tree *tree;
    GHashTable *hash_table;
    size_t *histogram;
    TimeStampType current_time_stamp;
    size_t infinite_distance;
};

struct ReuseStack *
reuse_stack_new();

void
reuse_stack_access_item(struct ReuseStack *me, EntryType entry);

void
reuse_stack_print_sparse_histogram(struct ReuseStack *me);

void
reuse_stack_free(struct ReuseStack *me);
