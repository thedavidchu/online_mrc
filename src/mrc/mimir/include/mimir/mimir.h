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

/// @param num_buckets: uint64_t   - number of MIMIR stack distance buckets
/// @param bin_size: uint64_t   - size of the histogram bin
bool
Mimir__init(struct Mimir *me,
            const uint64_t num_buckets,
            const uint64_t bin_size,
            const uint64_t max_num_unique_entries,
            const enum MimirAgingPolicy aging_policy);

void
Mimir__access_item(struct Mimir *me, EntryType entry);

void
Mimir__print_hash_table(struct Mimir *me);

void
Mimir__print_histogram_as_json(struct Mimir *me);

bool
Mimir__validate(struct Mimir *me);

void
Mimir__destroy(struct Mimir *me);
