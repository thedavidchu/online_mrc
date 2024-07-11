#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "miss_rate_curve/miss_rate_curve.h"
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

bool
Olken__access_item(struct Olken *me, EntryType entry);

bool
Olken__remove_item(struct Olken *me, EntryType entry);

/// @brief  Ignore an entry.
/// @details    Sampling is not part of the core Olken algorithm,
///             however, this is extensively used by others so I intend
///             for this to be used when we ignore a sample. The reason
///             is for time-based analysis, where we may want the final
///             output and the oracle to line up in terms of time.
void
Olken__ignore_entry(struct Olken *me);

/// @return Return the stack distance of an existing item or uint64::MAX
///         upon an error.
uint64_t
Olken__update_stack(struct Olken *me, EntryType entry, TimeStampType timestamp);

bool
Olken__insert_stack(struct Olken *me, EntryType entry);

bool
Olken__post_process(struct Olken *me);

bool
Olken__to_mrc(struct Olken const *const me, struct MissRateCurve *const mrc);

void
Olken__print_histogram_as_json(struct Olken *me);

void
Olken__destroy(struct Olken *me);
