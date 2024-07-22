#include <immintrin.h>
#include <x86intrin.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <assert.h>
#include "../histogram.h"
#include "qmrc.h"

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

/* Logically, a bucket contains and epoch and a count.
 *
 * struct bucket {
 *	int epoch;     // epoch at which this bucket is created
 *	count_t count; // nr of keys in bucket
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
	count_t *counts;
	struct histogram hist; /* stores histogram of stack distance */

	count_t nr_buckets; /* number of epochs/counts buckets */
	count_t epoch_limit; /* threshold when an epoch is created */
	count_t total_keys; /* current total number of unique keys */
	count_t max_keys; /* max unique keys that currently fit in histogram */

	int adjust_epoch_limit; /* adjust epoch_limit, if not specified */

	int nr_merge;
	int nr_zero;
#ifdef STATS
	count_t *lookup;
	count_t *delete;
	count_t *merge;
#endif /* STATS */
};

/* remove idx element from buckets by
 * shifting all previous elements to the right by one. */
static void
qmrc_remove_epoch(int *buckets, int idx)
{
	memmove(buckets + 1, buckets, (size_t)idx * sizeof(int));
}

static void
qmrc_remove_bucket(count_t *buckets, int idx)
{
	memmove(buckets + 1, buckets, (size_t)idx * sizeof(count_t));
}

static void
qmrc_update_max_keys(struct qmrc *qmrc)
{
	/* double max_keys */
	qmrc->max_keys <<= 1;

	/* adjust the histogram buckets */
	histogram_double_bucket_size(&qmrc->hist);

	/* double epoch_limit */
	if (qmrc->adjust_epoch_limit) {
		qmrc->epoch_limit <<= 1;
	}
}

/* creates a new epoch by making space at qmrc->buckets[0]. */
static void
qmrc_merge(struct qmrc *qmrc)
{
	int merge_idx = 0; /* index of bucket to merge */
	count_t min_sum = INT_MAX;

	/* compute the sum of the counts of consecutive buckets and merge two
	 * buckets with the smallest sum. this strategy aims to keep similar
	 * counts in the different buckets, which should minimize error.
	 *
	 * we can use other merging strategies, e.g., we could remove a bucket
	 * with the smallest lookup count, i.e., qmrc->lookup[idx]. however,
	 * make sure that min_sum is the sum of the counts[] of two
	 * consecutive buckets.
	 */
	for (int idx = 1; idx < qmrc->nr_buckets; idx++) {
		count_t sum = qmrc->counts[idx-1] + qmrc->counts[idx];
		if (min_sum > sum) {
			min_sum = sum;
			merge_idx = idx;
		}
	}

#ifdef ASSERT
	assert(merge_idx > 0);
	assert(qmrc->counts[merge_idx-1] >= 0);
	assert(qmrc->counts[merge_idx] >= 0);
	assert(min_sum >= 0);
#endif /* ASSERT */

	/* merge merge_idx - 1 bucket into merge_idx bucket */
	qmrc->counts[merge_idx] = min_sum;
	qmrc->nr_merge++;

	/* remove bucket at merge_idx-1 by shifting
	 * all previous buckets to the right */
	merge_idx--;
	if (merge_idx > 0) {
		qmrc_remove_epoch(qmrc->epochs, merge_idx);
		qmrc_remove_bucket(qmrc->counts, merge_idx);

#ifdef ASSERT
		for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
			assert(qmrc->counts[idx] >= 0);
		}
#endif /* ASSERT */

	} else {
		/* track whether memcpy is needed. if this value is high at the
		 * end of the experiment compared to the total number of merges,
		 * then we may be creating too many epochs, i.e., calling merge
		 * too many times. however, the overhead of merge is low, so
		 * this may not be an issue. */
		qmrc->nr_zero++;
	}

	/* initialize the first bucket with a new epoch */
	qmrc->counts[0] = 0;
	qmrc->epochs[0] += 1;

#ifdef STATS
	qmrc->merge[merge_idx]++;
#endif /* STATS */
}

/*
 * log_hist_buckets is the log of the number of histogram buckets.
 *
 * log_qmrc_buckets is the log of the number of qmrc buckets. 
 *
 * log_epoch_limit is the log of the threshold at which the current bucket is
 * considered full and a new epoch is created. If it is 0, we adapt it.
 */
struct qmrc *
qmrc_init(int log_hist_buckets, int log_qmrc_buckets, int log_epoch_limit)
{
	struct qmrc *qmrc;
	size_t alloc_size;
	
	qmrc = calloc(1, sizeof(struct qmrc));
	assert(qmrc);

	qmrc->nr_buckets = (count_t)1 << log_qmrc_buckets;

	/* align buckets to cache size */
	alloc_size = qmrc->nr_buckets * sizeof(int);
	qmrc->epochs = aligned_alloc(CACHELINE_SIZE, alloc_size);
	assert((intptr_t)qmrc->epochs % CACHELINE_SIZE == 0);
	memset(qmrc->epochs, 0, alloc_size);

	alloc_size = qmrc->nr_buckets * sizeof(count_t);
	qmrc->counts = aligned_alloc(CACHELINE_SIZE, alloc_size);
	assert((intptr_t)qmrc->counts % CACHELINE_SIZE == 0);
	memset(qmrc->counts, 0, alloc_size);

	/* set initial max_keys to the number of histogram buckets */
	qmrc->max_keys = (count_t)1 << log_hist_buckets;
	histogram_init(&qmrc->hist, log_hist_buckets, 0);

	/* Choice of epoch limit is somewhat critical for performance and
	 * accuracy. A smaller epoch limit will increase the number of epochs
	 * being created, which will increase accuracy but lower performance.*/
	if (log_epoch_limit == 0) {
		/* increase epoch_limit as we increase max_keys */
		qmrc->adjust_epoch_limit = 1;
		/* expected number of keys in a qmrc bucket is
		 * (max_keys / qmrc_buckets). */
		log_epoch_limit = log_hist_buckets - log_qmrc_buckets;
	}
	assert(log_epoch_limit >= 0);
	qmrc->epoch_limit = (count_t)1 << log_epoch_limit;

#ifdef STATS
	qmrc->lookup = calloc(qmrc->nr_buckets, sizeof(count_t));
	assert(qmrc->lookup);
	qmrc->delete = calloc(qmrc->nr_buckets, sizeof(count_t));
	assert(qmrc->delete);
	qmrc->merge = calloc(qmrc->nr_buckets, sizeof(count_t));
	assert(qmrc->merge);
#endif /* STATS */

	return qmrc;
}

#ifndef QMRC_NO_AVX2

/* helper or hsum_8x32 */
static uint32_t
hsum_epi32_avx(__m128i x)
{
	/* 3-op non-destructive AVX lets us save a byte without using movdqa */
	__m128i hi64  = _mm_unpackhi_epi64(x, x);
	__m128i sum64 = _mm_add_epi32(hi64, x);
	/* swap the low two elements */
	__m128i hi32  = _mm_shuffle_epi32(sum64, _MM_SHUFFLE(2, 3, 0, 1));
	__m128i sum32 = _mm_add_epi32(sum64, hi32);
	return (uint32_t)_mm_cvtsi128_si32(sum32);
}

/* returns sum of 8 ints in v, only needs AVX2 */
static uint32_t
hsum_8x32(__m256i v)
{
	__m128i sum128 = _mm_add_epi32(_mm256_castsi256_si128(v),
				       _mm256_extracti128_si256(v, 1));
	return hsum_epi32_avx(sum128);
}

#endif /* QMRC_NO_AVX2 */

/*
 * key idea of qmrc
 * 
 * epochs: 4  3  2  1  0
 * counts: 30 10 07 03 20
 *
 * say we perform qmrc_lookup(1)
 * this decrements the count for epoch 1, and then via qmrc_insert,
 * increments the count for epoch 4
 * epochs: 4  3  2  1  0
 * counts: 31 10 07 02 20
 * 
 * however, say the epoch_limit is 30, then qmrc_insert will create
 * a new epoch (5) by merging epoch 2 with epoch 1
 * epochs: 5  4  3  1  0
 * counts: 01 30 10 09 20
 *  
 * then qmrc_lookup will return the new current epoch (5)
 * 5 = qmrc_lookup(1)
 */

/* TODO: add optimization when epoch == epochs[0]. this should benefit skewed
 * workloads. */
/* TODO: add support for object sizes. should be relatively simple */
int
qmrc_lookup(struct qmrc *qmrc, int epoch)
{
	count_t sd = 0;
	int idx = -1;

#ifndef QMRC_NO_AVX2 /* avx2 provides a little benefit. */

/* use avx2 when index of desired epoch is at least AVX2_THRESHOLD. */
#define AVX2_THRESHOLD 16

	if (qmrc->epochs[idx + AVX2_THRESHOLD] > epoch) {
		/* Initialize sum vector to {0, 0, 0, 0, 0, 0, 0, 0} */
		__m256i sum_vec = _mm256_setzero_si256();
		idx += 8;		
		do {
			/* load 32 bytes into tmp */
			__m256i tmp = _mm256_load_si256
				((__m256i *)(qmrc->counts + idx - 7));
			sum_vec = _mm256_add_epi32(sum_vec, tmp);
			idx += 8;
		} while (qmrc->epochs[idx] > epoch);
		sd = hsum_8x32(sum_vec);
		idx -= 8;
	}
#endif /* QMRC_NO_AVX2 */

	do {
		idx++;
		sd += qmrc->counts[idx];
	} while (qmrc->epochs[idx] > epoch);

#ifdef ASSERT
	assert(idx < qmrc->nr_buckets);
	assert(qmrc->counts[idx] > 0);
#endif /* ASSERT */

#ifdef STATS
	qmrc->lookup[idx]++;
#endif /* STATS */

	/* decrement bucket count for epoch */
	qmrc->counts[idx]--;

	sd -= 1; /* ensures that histogram array does not overflow */

#ifdef ASSERT
	assert(sd >= 0);
	assert(sd < qmrc->total_keys);
#endif /* ASSERT */

#ifdef QMRC_INTERPOLATE
	/* interpolate the stack distance based on epochs */
	/* TODO: haven't benchmarked this option */
	if (idx > 0 && epoch > qmrc->epochs[idx]) {
		float ratio = (float)(epoch - qmrc->epochs[idx]) /
			(float)(qmrc->epochs[idx - 1] - qmrc->epochs[idx]);
		float sub = ratio * (float)qmrc->counts[idx];
		sd = sd - (count_t)sub;
	}
#endif /* QMRC_INTERPOLATE */

	/* create a histogram of stack distances */
	histogram_insert(&qmrc->hist, sd);

	/* this code should be same as the bottom of qmrc_insert() */
	if (unlikely(qmrc->counts[0] >= qmrc->epoch_limit))
		qmrc_merge(qmrc);

	/* increment count for current epoch */
	qmrc->counts[0]++;

	/* return current epoch */	
	return qmrc->epochs[0];
}

int
qmrc_insert(struct qmrc *qmrc)
{
#ifdef ASSERT
	assert(qmrc->total_keys <= qmrc->max_keys);
#endif /* ASSERT */
	qmrc->total_keys++;

	if (unlikely(qmrc->total_keys > qmrc->max_keys))
		qmrc_update_max_keys(qmrc);

	if (unlikely(qmrc->counts[0] >= qmrc->epoch_limit))
		qmrc_merge(qmrc);

	/* increment count for current epoch */
	qmrc->counts[0]++;

	/* return current epoch */	
	return qmrc->epochs[0];
}

/* QMRC_BINARY provides little or no benefits */
#ifdef QMRC_BINARY

static int
qmrc_lowerbound(struct qmrc *qmrc, int epoch)
{
	int l = 0;
	int r = qmrc->nr_buckets - 1;
	while (l < r) {
		int t = (l + r)/2;
		int e = qmrc->epochs[t];
		if (e <= epoch) {
			r = t;
		} else {
			l = t + 1;
		}
	}
	return l;
}

#endif /* QMRC_BINARY */

#define COUNTS_PER_CACHELINE (int)(CACHELINE_SIZE/sizeof(int))

void
qmrc_delete(struct qmrc *qmrc, int epoch)
{
#ifndef QMRC_BINARY
	int idx = -1;

	/* search a cacheline at a time */
	while (qmrc->epochs[idx + COUNTS_PER_CACHELINE] > epoch) {
		idx += COUNTS_PER_CACHELINE;
	}

	/* search an element at a time */
	do {
		idx++;
	} while (qmrc->epochs[idx] > epoch);

#else /* QMRC_BINARY */
	int idx = qmrc_lowerbound(qmrc, epoch);
#endif /* QMRC_BINARY */
	
	/* decrement bucket count for epoch */
	qmrc->counts[idx]--;
	qmrc->total_keys--;

#ifdef STATS
	qmrc->delete[idx]++;
#endif /* STATS */
	
}

void
qmrc_output(struct qmrc *qmrc)
{
	/* total_hits is the total # of lookup (not insert) operations */
	printf("qmrc_buckets = %d, sd_buckets = %d, epoch_limit = %ld, "
	       "total_hits = %ld, nr_merge = %d, nr_zero = %d\n",
	       qmrc->nr_buckets, histogram_length(&qmrc->hist),
	       (size_t)qmrc->epoch_limit, histogram_total_hits(&qmrc->hist),
	       qmrc->nr_merge, qmrc->nr_zero);

	size_t max_count = 0;
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		if (max_count < qmrc->counts[idx]) {
			max_count = qmrc->counts[idx];
		}
	}
	printf("max_keys = %ld, total_keys = %ld, average_keys/bucket = %ld, "
	       "max_keys/bucket = %ld, max_error = %.2lf%%\n",
	       (size_t)qmrc->max_keys, (size_t)qmrc->total_keys,
	       (size_t)qmrc->total_keys / qmrc->nr_buckets,
	       max_count, (100 * (double)max_count) / (double)qmrc->total_keys);

	printf("\n");
	histogram_print_miss_rate(&qmrc->hist);
		
#ifdef STATS
	printf("\n");
	printf("\n1. bucket_nr: miss rate");
	histogram_print_formatted_miss_rate(&qmrc->hist);
	
	printf("\n2. bucket_nr: hits per bucket");
	histogram_print_formatted(&qmrc->hist);

	printf("\n3. bucket_nr: epoch_nr, bucket_count");	
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		if ((idx % 8) == 0) {
			printf("\n|");
		}
		printf("%4d: %5d %7ld|", idx, qmrc->epochs[idx],
		       (size_t)qmrc->counts[idx]);
	}
	printf("\n");

	printf("\n4. bucket_nr: lookup");
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		if ((idx % 8) == 0) {
			printf("\n|");
		}
		printf("%4d: %9ld| ", idx, (size_t)qmrc->lookup[idx]);
	}
	printf("\n");

	printf("\n5. bucket_nr: delete");
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		if ((idx % 8) == 0) {
			printf("\n|");
		}
		printf("%4d: %9ld| ", idx, (size_t)qmrc->delete[idx]);
	}
	printf("\n");

	printf("\n6. bucket_nr: merge");
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		if ((idx % 8) == 0) {
			printf("\n|");
		}
		printf("%4d: %9ld| ", idx, (size_t)qmrc->merge[idx]);
	}
	printf("\n");
#endif /* STATS */

}

void
qmrc_destroy(struct qmrc *qmrc)
{
#ifdef ASSERT
	/* assuming that the cache is empty when this function is invoked,
	 * the counts should all be 0 */
	for (int idx = 0; idx < qmrc->nr_buckets; idx++) {
		assert(qmrc->counts[idx] == 0);
	}
#endif /* ASSERT */
	free(qmrc->counts);
	free(qmrc->epochs);
	histogram_destroy(&qmrc->hist);
	free(qmrc);
}
