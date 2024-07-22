/** Taken from Ashvin Goel's QuickMRC implementation. */
#pragma once

#include <stdbool.h>
#include <stddef.h>

/* Logically, a bucket contains and epoch and a count.
 *
 * struct bucket {
 *	int epoch;     // epoch at which this bucket is created
 *	size_t count; // nr of keys in bucket
 * };
 *
 * below, we allocate epochs and counts arrays separately, which may provide
 * better locality */
struct qmrc {
    /* stores epoch at which a bucket is created.
     * current (most recent) epoch is stored in epochs[0]. */
    /* qmrc_delete code assumes that epochs is an int array */
    int *epochs;
    /* nr of keys last accessed in an epoch E, where E lies in the range
     * epochs[N-1] > E >= epochs[N] are stored in bucket N */
    size_t *counts;

    size_t nr_buckets;  /* number of epochs/counts buckets */
    size_t epoch_limit; /* threshold when an epoch is created */
    size_t total_keys;  /* current total number of unique keys */
    size_t max_keys;    /* max unique keys that currently fit in histogram */

    bool adjust_epoch_limit; /* adjust epoch_limit, if not specified */

    int nr_merge;
    int nr_zero;
#ifdef STATS
    size_t *lookup;
    size_t *delete;
    size_t *merge;
#endif /* STATS */
};

bool
qmrc_init(struct qmrc *qmrc,
          int log_hist_buckets,
          int log_qmrc_buckets,
          int log_epoch_limit);
int
qmrc_lookup(struct qmrc *qmrc, int epoch);
int
qmrc_insert(struct qmrc *qmrc);
void
qmrc_delete(struct qmrc *qmrc, int epoch);
void
qmrc_destroy(struct qmrc *qmrc);
