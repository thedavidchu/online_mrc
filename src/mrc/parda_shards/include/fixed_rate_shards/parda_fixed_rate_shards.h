#pragma once

#include <stdbool.h>
#include <stdint.h>

// NOTE I detest including Parda's disgustingly messy header. It exposes all the
//      messy Parda internals that I absolutely do not want to reveal (e.g.
//      global variables, random types, etc.)
#include "hash/types.h"
#include "parda.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"

struct FixedRateShards {
    program_data_t program_data;
    Hash64BitType shards_scaling_factor;
    TimeStampType current_time_stamp;
};

bool
parda_fixed_rate_shards__init(struct FixedRateShards *me,
                              const Hash64BitType threshold);

void
parda_fixed_rate_shards__access_item(struct FixedRateShards *me,
                                     EntryType entry);

void
parda_fixed_rate_shards__destroy(struct FixedRateShards *me);
