#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "parda_shards/parda_fixed_rate_shards.h"
#include "hash/splitmix64.h"
#include "parda.h"
#include "types/entry_type.h"

bool
PardaFixedRateShards__init(struct PardaFixedRateShards *me,
                              const double sampling_ratio)
{
    if (me == NULL || sampling_ratio <= 0.0 || 1.0 < sampling_ratio)
        return false;
    // TODO: initialize program data
    me->program_data = parda_init();
    me->sampling_ratio = sampling_ratio;
    me->current_time_stamp = 0;
    return true;
}

void
PardaFixedRateShards__access_item(struct PardaFixedRateShards *me,
                                     EntryType entry)
{
    char entry_str[21] = {0};
    if (me == NULL) {
        return;
    }
    if (splitmix64_hash(entry) > UINT64_MAX * me->sampling_ratio) {
        return;
    }
    // I need to convert it to a string because Parda will strdup(...) it.
    // This probably leads to terrible performance. Welp, just further proof
    // that Parda is terribly implemented.
    sprintf(entry_str, "%" PRIu64, entry);
    process_one_access(entry_str,
                       &me->program_data,
                       me->current_time_stamp,
                       me->sampling_ratio);
    ++me->current_time_stamp;
}

/// NOTE
/// TODO Parda does not destroy the key-value pairs in the hash table.
void
PardaFixedRateShards__destroy(struct PardaFixedRateShards *me)
{
    parda_free(&me->program_data);
    me->program_data.gh = NULL;
    me->program_data.ga = NULL;
    me->program_data.root = NULL;
    me->program_data.histogram = NULL;
}
