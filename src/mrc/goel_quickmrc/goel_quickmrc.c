#include <stdbool.h>
#include <stdint.h>

#include "goel_quickmrc/goel_quickmrc.h"
#include "quickmrc/cache.h"
#include "types/entry_type.h"
#include "unused/mark_unused.h"

bool
GoelQuickMRC__init(struct GoelQuickMRC *me,
                   const int log_max_keys,
                   const int log_hist_buckets,
                   const int log_qmrc_buckets,
                   const int log_epoch_limit,
                   const double shards_sampling_ratio)
{
    me->cache = cache_init(log_max_keys,
                           log_hist_buckets,
                           log_qmrc_buckets,
                           log_epoch_limit);
    if (!me->cache)
        return false;
    UNUSED(shards_sampling_ratio);
    return true;
}

bool
GoelQuickMRC__access_item(struct GoelQuickMRC *me, EntryType entry)
{
    cache_insert(me->cache, entry);
    return true;
}

void
GoelQuickMRC__post_process(struct GoelQuickMRC *me)
{
    UNUSED(me);
}

void
GoelQuickMRC__print_histogram_as_json(struct GoelQuickMRC *me)
{
    UNUSED(me);
}

void
GoelQuickMRC__destroy(struct GoelQuickMRC *me)
{
    cache_destroy(me->cache);
    *me = (struct GoelQuickMRC){0};
}
