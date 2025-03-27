#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"
#include "list/list.hpp"

#include "lib/eviction_cause.hpp"
#include "lib/lifetime_cache.hpp"
#include "lib/lru_ttl_cache.hpp"
#include "lib/prediction_tracker.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class PredictiveCache {
private:
    /// @brief  Insert an object into the cache.
    void
    insert(CacheAccess const &access);

    /// @brief  Process an access to an item in the cache.
    void
    update(CacheAccess const &access, CacheMetadata &metadata);

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    evict(uint64_t const victim_key, EvictionCause const cause);

    void
    evict_expired_objects(uint64_t const current_time_ms);

    /// @param  target_bytes - the number of bytes to try to evict.
    /// @return the number of bytes evicted.
    uint64_t
    evict_from_lru(uint64_t const target_bytes);

    /// @brief  Get the soonest expiring keys that would be enough to
    ///         make sufficient room in the cache.
    /// @param  target_bytes - the number of bytes to try to evict.
    /// @return the number of bytes evicted.
    bool
    evict_smallest_ttl(uint64_t const target_bytes);

    bool
    ensure_enough_room(size_t const old_nbytes, size_t const new_nbytes);

    void
    evict_accessed_object(CacheAccess const &access);

    bool
    is_expired(CacheAccess const &access, CacheMetadata const &metadata);

    void
    hit(CacheAccess const &access);

    bool
    miss(CacheAccess const &access);

public:
    /// @note   The {ttl,lru}_only parameters are deprecated. This wasn't
    ///         a particularly good idea nor was it particularly useful.
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    /// @param  {lower,upper}_ratio: double [0.0, 1.0] - The
    ///         ratio thresholds for prediction.
    /// @param  record_reaccess: bool
    ///         If true, then record the lifetime as the time between
    ///         either LRU evictions or reaccess.
    ///         Otherwise, record the lifetime as the time between
    ///         insertion and LRU eviction.
    /// @param  repredict_reaccess: bool
    ///         If true, then re-predict the queues on each reaccess.
    ///         Otherwise, predict the queue only on insertion.
    PredictiveCache(size_t const capacity,
                    double const lower_ratio,
                    double const upper_ratio,
                    LifeTimeCacheMode const cache_mode);

    void
    start_simulation();

    void
    end_simulation();

    int
    access(CacheAccess const &access);

    size_t
    size() const;

    size_t
    capacity() const;

    LifeTimeCacheMode
    lifetime_cache_mode() const;

    CacheMetadata const *
    get(uint64_t const key);

    void
    print();

    PredictionTracker const &
    predictor() const;

    CacheStatistics const &
    statistics() const;

    void
    print_statistics() const;

private:
    static constexpr bool DEBUG = false;

    // Maximum number of bytes in the cache.
    size_t const capacity_;
    LifeTimeCacheMode const lifetime_cache_mode_;

    // Number of bytes in the current cache.
    size_t size_ = 0;
    // Statistics related to prediction.
    PredictionTracker pred_tracker;
    // Statistics related to cache performance.
    CacheStatistics statistics_;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;

    // Maps expiration time to keys.
    std::multimap<uint64_t, uint64_t> ttl_cache_;

    // Real (or SHARDS-ified) LRU cache to monitor the lifetime of elements.
    LifeTimeCache lifetime_cache_;
    // This wouldn't exist in the real cache, for obvious reasons.
    // This is just to enable collecting accuracy statistics.
    LRUTTLCache oracle_;
};
