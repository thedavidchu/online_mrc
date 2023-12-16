#pragma once

#include <stdint.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct OlkenReuseStack {
    struct Tree tree;
    GHashTable *hash_table;
    struct BasicHistogram histogram;
    TimeStampType current_time_stamp;
};

bool
olken___init(struct OlkenReuseStack *me, const uint64_t max_num_unique_entries);

void
olken___access_item(struct OlkenReuseStack *me, EntryType entry);

void
olken___print_sparse_histogram(struct OlkenReuseStack *me);

void
olken___destroy(struct OlkenReuseStack *me);
