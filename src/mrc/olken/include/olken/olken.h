#pragma once

#include <stdint.h>

#include <glib.h>

#include "histogram/basic_histogram.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "lookup/parallel_hash_table.h"

struct Olken {
    struct Tree tree;
    struct ParallelHashTable hash_table;
    struct BasicHistogram histogram;
    TimeStampType current_time_stamp;
};

bool
Olken__init(struct Olken *me, const uint64_t max_num_unique_entries);

void
Olken__access_item(struct Olken *me, EntryType entry);

void
Olken__print_histogram_as_json(struct Olken *me);

void
Olken__destroy(struct Olken *me);
