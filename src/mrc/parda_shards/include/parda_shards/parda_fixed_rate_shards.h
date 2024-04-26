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

struct PardaFixedRateShards {
    program_data_t program_data;
    Hash64BitType shards_scaling_factor;
    TimeStampType current_time_stamp;
};

bool
parda_fixed_rate_shards__init(struct PardaFixedRateShards *me,
                              const uint64_t shards_scaling_factor);

void
parda_fixed_rate_shards__access_item(struct PardaFixedRateShards *me,
                                     EntryType entry);

void
parda_fixed_rate_shards__destroy(struct PardaFixedRateShards *me);
