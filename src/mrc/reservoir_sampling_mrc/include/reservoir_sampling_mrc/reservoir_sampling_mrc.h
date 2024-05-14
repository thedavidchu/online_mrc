#pragma once

#include <stdint.h>

#include <glib.h>

#include "histogram/histogram.h"
#include "lookup/hash_table.h"
#include "olken/olken.h"
#include "sampling/reservoir_sampling.h"
#include "tree/types.h"
#include "types/entry_type.h"
#include "types/time_stamp_type.h"
#include "unused/mark_unused.h"

struct ReservoirSamplingMRC {
    struct ReservoirSamplingAlgorithmR reservoir;
    struct Olken olken;
};

bool
ReservoirSamplingMRC__init(struct ReservoirSamplingMRC *me,
                           const uint64_t max_num_unique_entries,
                           const uint64_t histogram_bin_size);

void
ReservoirSamplingMRC__access_item(struct ReservoirSamplingMRC *me,
                                  EntryType entry);

static inline void
ReservoirSamplingMRC__post_process(struct ReservoirSamplingMRC *me)
{
    UNUSED(me);
}

void
ReservoirSamplingMRC__print_histogram_as_json(struct ReservoirSamplingMRC *me);

void
ReservoirSamplingMRC__destroy(struct ReservoirSamplingMRC *me);
