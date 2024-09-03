/** @brief  A hash table where collisions result in eviction.
 *
 *  A file containing an experimental hash table with an eviction policy
 *  for collisions, whereby the element with the larger hash value is
 *  discarded and the element with the smaller hash value is kept.
 *
 * This is a form of Reservoir Sampling, where the randomization is the
 * hash function. One may also observe some similarity between this
 * technique and Waldspurger et al.'s SHARDS.
 *
 * The beauty of this data structure is three-fold:
 * 1. It is a constant size with no data allocation,
 * 2. It is trivially parallelizable, and
 * 3. It trivially yields a HyperLogLog counter!
 *
 * @note    Changing the hash function breaks my beautiful test cases.
 */
#pragma once

#include <assert.h>
#include <float.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "hash/hash.h"
#include "hash/types.h"
#include "logger/logger.h"
#include "math/count_leading_zeros.h"
#include "types/key_type.h"
#include "types/time_stamp_type.h"
#include "types/value_type.h"
#include "unused/mark_unused.h"

struct EvictingHashTableNode {
    KeyType key;
    ValueType value;
};

struct EvictingHashTable {
    struct EvictingHashTableNode *data;
    Hash64BitType *hashes;
    size_t length;
    double init_sampling_ratio;
    Hash64BitType global_threshold;

    size_t num_inserted;
    double running_denominator;
    double hll_alpha_m;
};

enum SampledStatus {
    SAMPLED_HITHERTOEMPTY,
    SAMPLED_NOTFOUND,
    SAMPLED_IGNORED,
    SAMPLED_FOUND,
    SAMPLED_INSERTED,
    SAMPLED_REPLACED,
    SAMPLED_UPDATED,
};

struct SampledLookupReturn {
    enum SampledStatus status;
    Hash64BitType hash;
    TimeStampType timestamp;
};

struct SampledPutReturn {
    enum SampledStatus status;
    Hash64BitType new_hash;
    TimeStampType old_timestamp;
};

struct SampledTryPutReturn {
    enum SampledStatus status;
    Hash64BitType new_hash;
    KeyType old_key;
    Hash64BitType old_hash;
    ValueType old_value;
};

bool
EvictingHashTable__init(struct EvictingHashTable *me,
                        const size_t length,
                        const double init_sampling_ratio);

struct SampledLookupReturn
EvictingHashTable__lookup(struct EvictingHashTable *me, KeyType key);

struct SampledPutReturn
EvictingHashTable__put(struct EvictingHashTable *me,
                       KeyType key,
                       ValueType value);

/// @note   If we know the globally maximum threshold, then we can
///         immediately discard any element that is greater than this.
/// @note   This is an optimization to try to match SHARDS's performance.
///         Without this, we slightly underperform SHARDS. I don't know
///         how the Splay Tree priority queue is so fast...
void
EvictingHashTable__refresh_threshold(struct EvictingHashTable *me);

static inline struct SampledTryPutReturn
EHT__insert_new_element(struct EvictingHashTable *me,
                        KeyType key,
                        ValueType value,
                        struct EvictingHashTableNode *incumbent,
                        Hash64BitType *hash_ptr,
                        Hash64BitType hash)
{
    *incumbent = (struct EvictingHashTableNode){.key = key, .value = value};
    *hash_ptr = hash;
    ++me->num_inserted;
    if (me->num_inserted == me->length) {
        EvictingHashTable__refresh_threshold(me);
    }
    me->running_denominator += exp2(-clz(hash) - 1) - me->init_sampling_ratio;
    return (struct SampledTryPutReturn){.status = SAMPLED_INSERTED,
                                        .new_hash = hash};
}

static inline struct SampledTryPutReturn
EHT__replace_incumbent_element(struct EvictingHashTable *me,
                               KeyType key,
                               ValueType value,
                               struct EvictingHashTableNode *incumbent,
                               Hash64BitType *hash_ptr,
                               Hash64BitType hash)
{
    Hash64BitType const old_hash = *hash_ptr;
    struct SampledTryPutReturn r = (struct SampledTryPutReturn){
        .status = SAMPLED_REPLACED,
        .new_hash = hash,
        .old_key = incumbent->key,
        .old_hash = old_hash,
        .old_value = incumbent->value,
    };
    // NOTE Update the incumbent before we do the scan for the maximum
    //      threshold because we want do not want to "find" that the
    //      maximum hasn't changed.
    *incumbent = (struct EvictingHashTableNode){.key = key, .value = value};
    *hash_ptr = hash;
    if (old_hash == me->global_threshold) {
        EvictingHashTable__refresh_threshold(me);
    }
    me->running_denominator += exp2(-clz(hash) - 1) - exp2(-clz(old_hash) - 1);
    return r;
}

static inline struct SampledTryPutReturn
EHT__update_incumbent_element(struct EvictingHashTable *me,
                              KeyType key,
                              ValueType value,
                              struct EvictingHashTableNode *incumbent,
                              Hash64BitType *hash_ptr,
                              Hash64BitType hash)
{
    UNUSED(me);
    UNUSED(hash_ptr);
    struct SampledTryPutReturn r = (struct SampledTryPutReturn){
        .status = SAMPLED_UPDATED,
        .new_hash = hash,
        .old_key = key,
        .old_hash = hash,
        .old_value = incumbent->value,
    };
    incumbent->value = value;
    return r;
}

/// @brief  Try to put a value into the hash table.
/// @return A structure of the new hash value and the evicted data (if
///         applicable).
/// @note   This combines the lookup and put traditionally used by the
///         MRC algorithm. I haven't thought too hard about whether all
///         other MRC algorithms could similarly combine the lookup and
///         put.
/// @note   Defining this as a `static inline` function improves performance
///         dramatically. In fact, without it, performance is much worse
///         than the separate lookup and put. I'm not exactly sure why, but
///         this has a much more complex return type. The performance is
///         better this way than enabling link-time optimizations too.
static inline struct SampledTryPutReturn
EvictingHashTable__try_put(struct EvictingHashTable *me,
                           KeyType key,
                           ValueType value)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return (struct SampledTryPutReturn){.status = SAMPLED_NOTFOUND};

    Hash64BitType hash = Hash64Bit(key);
    if (hash > me->global_threshold)
        return (struct SampledTryPutReturn){.status = SAMPLED_IGNORED};

    struct EvictingHashTableNode *incumbent = &me->data[hash % me->length];
    Hash64BitType *const hash_ptr = &me->hashes[hash % me->length];
    Hash64BitType const old_hash = *hash_ptr;
    if (old_hash == UINT64_MAX) {
        return EHT__insert_new_element(me,
                                       key,
                                       value,
                                       incumbent,
                                       hash_ptr,
                                       hash);
    }
    if (hash < old_hash) {
        return EHT__replace_incumbent_element(me,
                                              key,
                                              value,
                                              incumbent,
                                              hash_ptr,
                                              hash);
    }
    // NOTE If the key comparison is expensive, then one could first
    //      compare the hashes. However, in this case, they are not expensive.
    if (key == incumbent->key) {
        return EHT__update_incumbent_element(me,
                                             key,
                                             value,
                                             incumbent,
                                             hash_ptr,
                                             hash);
    }
    return (struct SampledTryPutReturn){.status = SAMPLED_IGNORED};
}

/// @param  m: uint64_t const
///             Number of HLL counters.
/// @param  V: uint64_t const
///             Number of registers equal to zero. We cannot get an
///             accurate estimate of the linear count if this V is zero.
/// @note   This function is general-purpose enough that I don't feel
///         the need to prefix it with 'EvictingHashTable__' or 'EHT__'.
static double
linear_counting(uint64_t const m, uint64_t const V)
{
    assert(m >= 1 && V >= 1);
    return m * log((double)m / V);
}

static inline double
EHT__estimate_num_unique(struct EvictingHashTable const *const me)
{
    if (me == NULL || me->data == NULL || me->length == 0)
        return 0.0;
    double const raw_estimate =
        me->hll_alpha_m * me->length * me->length / me->running_denominator;
    LOGGER_VERBOSE(
        "\\alpha: %f, length: %zu, denominator: %f, raw estimate: %f",
        me->hll_alpha_m,
        me->length,
        me->running_denominator,
        raw_estimate);
    uint64_t const num_empty = me->length - me->num_inserted;
    // NOTE We need to account for the initial SHARDS sampling when
    //      deciding between linear counting and hyperloglog.
    if (raw_estimate * me->init_sampling_ratio < 2.5 * me->length &&
        num_empty != 0) {
        return linear_counting(me->length, num_empty) / me->init_sampling_ratio;
    } else {
        // NOTE We don't bother with the large number approximation
        //      since I'm using 64 bit hashes.
        return raw_estimate;
    }
}

static inline double
EvictingHashTable__estimate_scale_factor(
    struct EvictingHashTable const *const me)
{
    return EHT__estimate_num_unique(me) / me->num_inserted;
}

void
EvictingHashTable__print_as_json(struct EvictingHashTable *me);

void
EvictingHashTable__destroy(struct EvictingHashTable *me);
