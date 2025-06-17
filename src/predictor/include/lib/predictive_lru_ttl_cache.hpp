#pragma once

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_predictive_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/remaining_lifetime.hpp"
#include "cpp_struct/hash_list.hpp"
#include "lib/eviction_cause.hpp"
#include "lib/lifetime_thresholds.hpp"
#include "lib/lru_ttl_cache.hpp"
#include "lib/lru_ttl_cache_statistics.hpp"
#include "lib/prediction_tracker.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <ostream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class PredictiveCache {
private:
    bool
    ok(bool const fatal = false) const;

    /// @brief  Insert an object into the cache.
    void
    insert(CacheAccess const &access);

    /// @brief  Process an access to an item in the cache.
    void
    update(CacheAccess const &access, CachePredictiveMetadata &metadata);

    /// @brief  Helper function to remove object from LRU queue.
    void
    remove_lru(uint64_t const victim_key,
               CachePredictiveMetadata const &m,
               CacheAccess const *const current_access,
               EvictionCause const cause);

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    remove(uint64_t const victim_key,
           EvictionCause const cause,
           CacheAccess const *const current_access);

    /// @brief  Remove expired objects from the TTL queue. This does not
    ///         remove expired objects that are not listed in the TTL
    ///         queue, however.
    void
    evict_expired_objects(uint64_t const current_time_ms);

    /// @brief  Remove the least recently used object(s) in the LRU queue.
    ///         This only touches objects in the LRU queue so if the
    ///         globally least recently used object isn't in the LRU
    ///         queue, it won't be evicted.
    /// @param  target_bytes - the number of bytes to try to evict.
    /// @return the number of bytes evicted.
    uint64_t
    evict_from_lru(uint64_t const target_bytes, CacheAccess const &access);

    /// @brief  Get the soonest expiring keys that would be enough to
    ///         make sufficient room in the cache.
    /// @param  target_bytes - the number of bytes to try to evict.
    /// @return the number of bytes evicted.
    uint64_t
    evict_smallest_ttl(uint64_t const target_bytes, CacheAccess const &access);

    bool
    ensure_enough_room(size_t const old_nbytes, CacheAccess const &access);

    /// @brief  Remove an expired object that the user is trying to access.
    ///         c.f. Lazy TTLs.
    void
    evict_expired_accessed_object(CacheAccess const &access);

    /// @brief  Remove an update object whose new size is too big for the cache.
    void
    evict_too_big_accessed_object(CacheAccess const &access);

    bool
    is_expired(CacheAccess const &access,
               CachePredictiveMetadata const &metadata);

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
                    std::map<std::string, std::string> kwargs = {});

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

    CachePredictiveMetadata const *
    get(uint64_t const key);

    std::string
    record_remaining_lifetime(CacheAccess const &access) const
    {
        RemainingLifetime rl{lru_cache_, map_, access.timestamp_ms, 100};
        return rl.json();
    }

    void
    print();

    PredictionTracker const &
    predictor() const;

    CacheStatistics const &
    statistics() const;

    /// @param  extras: extra things to print in the statistics. The
    ///                 values are taken literally without quoting.
    std::string
    json(std::map<std::string, std::string> extras) const;

    void
    print_json(std::ostream &ostrm = std::cout,
               std::map<std::string, std::string> extras = {}) const;

private:
    static constexpr bool DEBUG = false;

    // Maximum number of bytes in the cache.
    size_t const capacity_;

    // Number of bytes in the current cache.
    size_t size_ = 0;
    size_t lru_size_ = 0;
    size_t ttl_size_ = 0;
    // Statistics related to prediction.
    PredictionTracker pred_tracker;
    // Statistics related to cache performance.
    CacheStatistics statistics_;
    LRUTTLStatistics lru_ttl_statistics_;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CachePredictiveMetadata> map_;
    // Maps last access time to keys.
    HashList lru_cache_;

    // Maps expiration time to keys.
    std::multimap<double, uint64_t> ttl_cache_;

    LifeTimeThresholds lifetime_thresholds_;
    // This wouldn't exist in the real cache, for obvious reasons.
    // This is just to enable collecting accuracy statistics.
    LRU_TTL_Cache oracle_;

    // Extra metadata
    std::map<std::string, std::string> const kwargs_;
};
