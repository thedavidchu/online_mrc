#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>

#include "interval/interval_olken.h"
#include "logger/logger.h"
#include "lookup/hash_table.h"
#include "lookup/lookup.h"
#include "tree/basic_tree.h"
#include "tree/sleator_tree.h"
#include "types/entry_type.h"

bool
IntervalOlken__init(struct IntervalOlken *me, size_t const length)
{
    bool r = false;
    if (me == NULL) {
        LOGGER_ERROR("invalid me");
        return false;
    }

    r = HashTable__init(&me->reuse_lookup);
    if (!r) {
        LOGGER_ERROR("bad reuse hash table");
        goto cleanup;
    }
    r = tree__init(&me->lru_stack);
    if (!r) {
        LOGGER_ERROR("bad tree");
        goto cleanup;
    }
    me->stats = calloc(length, sizeof(*me->stats));
    if (me->stats == NULL) {
        LOGGER_ERROR("bad calloc");
        goto cleanup;
    }
    me->length = length;
    me->current_timestamp = 0;

    return true;
cleanup:
    HashTable__destroy(&me->reuse_lookup);
    tree__destroy(&me->lru_stack);
    free(me->stats);
    return false;
}

void
IntervalOlken__destroy(struct IntervalOlken *me)
{
    HashTable__destroy(&me->reuse_lookup);
    tree__destroy(&me->lru_stack);
    free(me->stats);
    *me = (struct IntervalOlken){0};
}

bool
IntervalOlken__access_item(struct IntervalOlken *me, EntryType const entry)
{
    bool s = false;
    enum PutUniqueStatus q = LOOKUP_PUTUNIQUE_ERROR;
    size_t reuse_dist = 0, reuse_time = 0;

    if (me == NULL) {
        return false;
    }

    struct LookupReturn r = HashTable__lookup(&me->reuse_lookup, entry);
    if (r.success) {
        reuse_dist = tree__reverse_rank(&me->lru_stack, r.timestamp);
        s = tree__sleator_remove(&me->lru_stack, r.timestamp);
        assert(s && "remove should not fail");
        s = tree__sleator_insert(&me->lru_stack, me->current_timestamp);
        assert(s && "insert should not fail");
        reuse_time = me->current_timestamp - r.timestamp - 1;
        q = HashTable__put_unique(&me->reuse_lookup,
                                  entry,
                                  me->current_timestamp);
        assert(q == LOOKUP_PUTUNIQUE_REPLACE_VALUE &&
               "update should replace value");
    } else {
        reuse_dist = SIZE_MAX;
        reuse_time = SIZE_MAX;
    }

    // Record the "histogram" statistics
    me->stats[me->current_timestamp].reuse_distance = reuse_dist;
    me->stats[me->current_timestamp].reuse_time = reuse_time;

    // Update current state
    ++me->current_timestamp;
    HashTable__put_unique(&me->reuse_lookup, entry, me->current_timestamp);

    return true;
}
