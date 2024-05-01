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
    double sampling_ratio;
    TimeStampType current_time_stamp;
};

bool
PardaFixedRateShards__init(struct PardaFixedRateShards *me,
                              const double sampling_ratio);

void
PardaFixedRateShards__access_item(struct PardaFixedRateShards *me,
                                     EntryType entry);

void
PardaFixedRateShards__destroy(struct PardaFixedRateShards *me);
