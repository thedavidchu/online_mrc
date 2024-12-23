#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include "hash/hash.h"
#include "hash/types.h"
#include "logger/logger.h"
#include "math/ratio.h"
#include "priority_queue/heap.h"
#include "shards/fixed_size_shards_sampler.h"
#include "types/entry_type.h"

bool
FixedSizeShardsSampler__init(struct FixedSizeShardsSampler *const me,
                             const double starting_sampling_ratio,
                             const uint64_t max_size,
                             bool const adjustment)
{
    if (me == NULL || starting_sampling_ratio <= 0.0 ||
        1.0 < starting_sampling_ratio || max_size == 0) {
        LOGGER_WARN("bad input");
        return false;
    }
    if (!Heap__init_max_heap(&me->pq, max_size)) {
        LOGGER_WARN("failed to initialize priority queue");
        goto cleanup;
    }
    if (adjustment) {
        LOGGER_WARN("fixed-size SHARDS adjustment not supported yet");
    }
    *me = (struct FixedSizeShardsSampler){
        .sampling_ratio = starting_sampling_ratio,
        .threshold = ratio_uint64(starting_sampling_ratio),
        .scale = 1 / starting_sampling_ratio,
        .pq = me->pq,
        .adjustment = adjustment,
        .num_entries_seen = 0,
        .num_entries_processed = 0,
    };
    return true;
cleanup:
    Heap__destroy(&me->pq);
    return false;
}

void
FixedSizeShardsSampler__destroy(struct FixedSizeShardsSampler *const me)
{
    if (me == NULL) {
        return;
    }
    Heap__destroy(&me->pq);
    *me = (struct FixedSizeShardsSampler){0};
}

static void
set_sampling_rate(struct FixedSizeShardsSampler *me, Hash64BitType new_max_hash)
{
    assert(me != NULL);
    // NOTE Converting UINT64_MAX from uint64_t to double causes the
    //      value to be rounded from 18446744073709551615 to
    //      18446744073709551616. By explicitly casting the UINT64_MAX
    //      to a double, I get rid of this warning. In this case, I am
    //      fine with this roundup because it provides an answer that is
    //      "close enough" (unlike cases where I use ratio_uint64).
    me->sampling_ratio = (double)new_max_hash / UINT64_MAX;
    me->threshold = new_max_hash;
    me->scale = UINT64_MAX / new_max_hash;
}

static void
make_room(struct FixedSizeShardsSampler *me,
          void (*eviction_hook)(void *data, EntryType key),
          void *eviction_data)
{
    EntryType entry = 0;

    if (me == NULL) {
        return;
    }
    Hash64BitType max_hash = Heap__get_top_key(&me->pq);
    while (Heap__remove(&me->pq, max_hash, &entry)) {
        // This is where one would remove the entry/time-stamp from the
        // hash table and tree.
        if (eviction_hook != NULL) {
            eviction_hook(eviction_data, entry);
        }
    }
    // No more elements with the old max_hash. Now we can update the new
    //  sampling_ratio, threshold, and scale!
    Hash64BitType new_max_hash = Heap__get_top_key(&me->pq);
    set_sampling_rate(me, new_max_hash);
    return;
}

bool
FixedSizeShardsSampler__sample(struct FixedSizeShardsSampler *me,
                               EntryType entry)
{
    ++me->num_entries_seen;
    // Skip items above the threshold. Note that we accept items that are equal
    // to the threshold because the maximum hash is the threshold.
    if (Hash64Bit((uint64_t)entry) > me->threshold) {
        return false;
    }
    ++me->num_entries_processed;
    return true;
}

bool
FixedSizeShardsSampler__insert(struct FixedSizeShardsSampler *me,
                               EntryType entry,
                               void (*eviction_hook)(void *data, EntryType key),
                               void *eviction_data)
{
    if (Heap__is_full(&me->pq)) {
        make_room(me, eviction_hook, eviction_data);
    }
    if (!Heap__insert_if_room(&me->pq, Hash64Bit(entry), entry)) {
        return false;
    }
    return true;
}

uint64_t
FixedSizeShardsSampler__estimate_cardinality(struct FixedSizeShardsSampler *me)
{
    return me->pq.length * me->scale;
}
