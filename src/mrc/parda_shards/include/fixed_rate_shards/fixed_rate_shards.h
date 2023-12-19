#pragma once

#include <stdbool.h>

struct FixedRateShards {
    program_data_t program_data;
};

bool
fixed_rate_shards__init(FixedRateShards *me, const uint64_t threshold, );
