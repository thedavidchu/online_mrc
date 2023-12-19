/// NOTE    This is an implementation of Mimir based off of the paper
///         [here](https://dl.acm.org/doi/10.1145/2670979.2671007). I do not
///         implement the ghost cache yet.
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/fractional_histogram.h"
#include "mimir/buckets.h"
#include "types/entry_type.h"

// NOTE Stacker ages the younger half of entries (rounded up to the nearest
//      bucket) by a single bucket. Rounder combines the last two buckets and
//      changes the newest/oldest bucket pointers.
enum MimirAgingPolicy {
    MIMIR_STACKER = 0,
    MIMIR_ROUNDER = 1,
};

struct Mimir {
    GHashTable *hash_table;
    struct MimirBuckets buckets;
    struct FractionalHistogram histogram;
    enum MimirAgingPolicy aging_policy;
};

bool
mimir__init(struct Mimir *me,
            const uint64_t num_buckets,
            const uint64_t max_num_unique_entries,
            const enum MimirAgingPolicy aging_policy);

void
mimir__access_item(struct Mimir *me, EntryType entry);

void
mimir__print_hash_table(struct Mimir *me);

void
mimir__print_sparse_histogram(struct Mimir *me);

bool
mimir__validate(struct Mimir *me);

void
mimir__destroy(struct Mimir *me);
