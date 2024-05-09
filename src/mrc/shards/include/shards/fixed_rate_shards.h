#pragma once

#include <bits/stdint-uintn.h>
#include <stdbool.h>
#include <stdint.h>

#include "olken/olken.h"
#include "types/entry_type.h"

struct FixedRateShards {
    struct Olken olken;
    // I cannot const qualify this because I need to set it and it is
    // stack memory, so the discussion in [1] doesn't apply.
    // It is times like these that I wish I were using Rust, or another
    // such language that doesn't require bending over backwards to
    // initialize this variable after it has been malloced. See my
    // 'init' function if you're curious what I mean!
    // [1] StackOverflow answer:
    // https://stackoverflow.com/questions/9691404/how-to-initialize-const-in-a-struct-in-c-with-malloc
    double sampling_ratio;
    uint64_t threshold;
    uint64_t scale;

    // SHARDS Adjustment Parameters
    uint64_t num_entries_seen;
    uint64_t num_entries_processed;
};

/// @brief  Initialize the structures needed for fixed-size SHARDS.
/// @param  me:
/// @param  max_num_unique_entries:
/// @param  sampling_ratio: this is the ratio that SHARDS samples
bool
FixedRateShards__init(struct FixedRateShards *me,
                      const uint64_t max_num_unique_entries,
                      const double sampling_ratio,
                      const uint64_t histogram_bin_size);

void
FixedRateShards__access_item(struct FixedRateShards *me, EntryType entry);

void
FixedRateShards__post_process(struct FixedRateShards *me);

void
FixedRateShards__print_histogram_as_json(struct FixedRateShards *me);

void
FixedRateShards__destroy(struct FixedRateShards *me);
