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
olken__init(struct OlkenReuseStack *me, const uint64_t max_num_unique_entries);

void
olken__access_item(struct OlkenReuseStack *me, EntryType entry);

void
olken__print_histogram_as_json(struct OlkenReuseStack *me);

void
olken__destroy(struct OlkenReuseStack *me);
