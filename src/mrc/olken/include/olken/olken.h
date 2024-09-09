#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "lookup/boost_hash_table.h"
#include "lookup/hash_table.h"
#include "lookup/k_hash_table.h"
#include "lookup/lookup.h"
#include "miss_rate_curve/miss_rate_curve.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct Olken {
    struct Tree tree;
    struct KHashTable hash_table;
    struct Histogram histogram;
    TimeStampType current_time_stamp;
#ifdef PROFILE_STATISTICS
    uint64_t ticks_ht;
    uint64_t ticks_lru;
    uint64_t cnt_ht;
    uint64_t cnt_lru;
#endif
};

bool
Olken__init(struct Olken *const me,
            size_t const histogram_num_bins,
            size_t const histogram_bin_size);

/// @brief  Initialize the Olken data structure, but with more parameters!
/// @note   The API of this function is less stable than the Olken__init().
bool
Olken__init_full(struct Olken *const me,
                 size_t const histogram_num_bins,
                 size_t const histogram_bin_size,
                 enum HistogramOutOfBoundsMode const out_of_bounds_mode);

bool
Olken__access_item(struct Olken *const me, EntryType const entry);

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
Olken__destroy(struct Olken *const me);

/// @brief  Provide a neat interface to access the histogram.
/// @details    Yes, I know that getter/setter functions are passé, but
///             I need a neat interface to get the histograms of every
///             MRC algorithm.
bool
Olken__get_histogram(struct Olken const *const me,
                     struct Histogram const **const histogram);

/// @brief  Get the cardinality of the current working set size.
inline size_t
Olken__get_cardinality(struct Olken const *const me)
{
    return KHashTable__get_size(&me->hash_table);
}

/// @brief  Lookup a value in Olken.
/// @note   This is simply to allow changing the implementation of the
///         hash table without breaking dependencies.
inline struct LookupReturn
Olken__lookup(struct Olken const *const me, EntryType const key)
{
    return KHashTable__lookup(&me->hash_table, key);
}

/// @brief  Lookup a value in Olken.
/// @note   This is simply to allow changing the implementation of the
///         hash table without breaking dependencies.
inline enum PutUniqueStatus
Olken__put(struct Olken *const me,
           EntryType const key,
           TimeStampType const value)
{
    return KHashTable__put(&me->hash_table, key, value);
}
