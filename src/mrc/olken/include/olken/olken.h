#pragma once

#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

struct Olken {
    struct Tree tree;
    struct HashTable hash_table;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
};

bool
Olken__init(struct Olken *me,
            const uint64_t histogram_num_bins,
            const uint64_t histogram_bin_size);

void
Olken__access_item(struct Olken *me, EntryType entry);

static inline void
Olken__post_process(struct Olken *me)
{
    UNUSED(me);
}

void
Olken__print_histogram_as_json(struct Olken *me);

void
Olken__destroy(struct Olken *me);
