#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include "parda_shards/parda_fixed_rate_shards.h"
#include "hash/splitmix64.h"
#include "parda.h"
#include "types/entry_type.h"

bool
parda_fixed_rate_shards__init(struct PardaFixedRateShards *me,
                              const uint64_t shards_scaling_factor)
{
    if (me == NULL) {
        return false;
    }
    // TODO: initialize program data
    me->program_data = parda_init();
    me->shards_scaling_factor = shards_scaling_factor;
    me->current_time_stamp = 0;
    return true;
}

void
parda_fixed_rate_shards__access_item(struct PardaFixedRateShards *me,
                                     EntryType entry)
{
    char entry_str[21] = {0};
    if (me == NULL) {
        return;
    }
    if (splitmix64_hash(entry) > UINT64_MAX / me->shards_scaling_factor) {
        return;
    }
    // I need to convert it to a string because Parda will strdup(...) it.
    // This probably leads to terrible performance. Welp, just further proof
    // that Parda is terribly implemented.
    sprintf(entry_str, "%" PRIu64, entry);
    process_one_access(entry_str,
                       &me->program_data,
                       me->current_time_stamp,
                       me->shards_scaling_factor);
    ++me->current_time_stamp;
}

/// NOTE
/// TODO Parda does not destroy the key-value pairs in the hash table.
void
parda_fixed_rate_shards__destroy(struct PardaFixedRateShards *me)
{
    parda_free(&me->program_data);
    me->program_data.gh = NULL;
    me->program_data.ga = NULL;
    me->program_data.root = NULL;
    me->program_data.histogram = NULL;
}
