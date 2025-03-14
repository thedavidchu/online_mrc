#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <optional>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"
#include "list/list.hpp"
#include "logger/logger.h"

#include "lib/eviction_cause.hpp"
#include "lib/lifetime_cache.hpp"
#include "lib/lru_ttl_cache.hpp"
#include "lib/prediction_tracker.hpp"
#include "lib/util.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class PredictiveCache {
private:
    /// @brief  Insert an object into the cache.
    void
    insert(CacheAccess const &access)
    {
        statistics_.insert(access.size_bytes);
        uint64_t const ttl_ms = access.ttl_ms.value_or(UINT64_MAX);
        map_.emplace(access.key, CacheMetadata{access});
        auto r = lifetime_cache_.thresholds();
        if (ttl_ms >= r.first) {
            pred_tracker.record_store_lru();
            lru_cache_.access(access.key);
        }
        if (ttl_ms <= r.second) {
            pred_tracker.record_store_ttl();
            ttl_cache_.emplace(access.expiration_time_ms().value_or(UINT64_MAX),
                               access.key);
        }
        size_ += access.size_bytes;
    }

    /// @brief  Process an access to an item in the cache.
    void
    update(CacheAccess const &access, CacheMetadata &metadata)
    {
        statistics_.update(metadata.size_, access.size_bytes);
        metadata.visit(access.timestamp_ms, {});
        if (!repredict_on_reaccess_) {
            return;
        }
        // We want the new TTL after the access (above).
        uint64_t const ttl_ms = metadata.ttl_ms();
        auto r = lifetime_cache_.thresholds();
        if (ttl_ms >= r.first) {
            pred_tracker.record_store_lru();
            lru_cache_.access(access.key);
        }
        if (ttl_ms <= r.second) {
            // Even if we don't re-insert into the TTL queue, we still
            // want to mark it as stored.
            pred_tracker.record_store_ttl();
            // Only insert the TTL if it hasn't been inserted already.
            if (find_multimap_kv(ttl_cache_,
                                 metadata.expiration_time_ms_,
                                 access.key) == ttl_cache_.end()) {
                ttl_cache_.emplace(metadata.expiration_time_ms_, access.key);
            }
        }
    }

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    evict(uint64_t const victim_key, EvictionCause const cause)
    {
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        uint64_t exp_tm = m.expiration_time_ms_;

        // Update metadata tracking
        switch (cause) {
        case EvictionCause::LRU:
            statistics_.evict(m.size_);
            if (statistics_.current_time_ms_ <= exp_tm) {
                // Not yet expired.
                pred_tracker.update_correctly_evicted(sz_bytes);
            } else {
                pred_tracker.update_wrongly_evicted(sz_bytes);
            }
            break;
        case EvictionCause::TTL:
            statistics_.expire(m.size_);
            if (oracle_.get(victim_key)) {
                pred_tracker.update_correctly_expired(sz_bytes);
            } else {
                pred_tracker.update_wrongly_expired(sz_bytes);
            }
            break;
        default:
            assert(0 && "impossible");
        }

        // Evict from map.
        map_.erase(victim_key);
        size_ -= sz_bytes;
        // Evict from LRU queue.
        lru_cache_.remove(victim_key);
        // Evict from TTL queue.
        remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
    }

    void
    evict_expired_objects(uint64_t const current_time_ms)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_cache_) {
            if (exp_tm >= current_time_ms) {
                break;
            }
            victims.push_back(key);
        }
        // One cannot erase elements from a multimap while also iterating!
        for (auto victim : victims) {
            evict(victim, EvictionCause::TTL);
        }
    }

    bool
    ensure_enough_room(size_t const old_nbytes, size_t const new_nbytes)
    {
        // We already have enough room if we're not increasing the data.
        if (old_nbytes >= new_nbytes) {
            assert(size_ <= capacity_);
            return true;
        }
        size_t const nbytes = new_nbytes - old_nbytes;
        // We can't possibly fit the new object into the cache!
        // A side-effect is that in this case, we don't flush our cache
        // for no reason.
        if (nbytes > capacity_) {
            if (DEBUG) {
                LOGGER_WARN("not enough capacity (%zu) for object (%zu)",
                            capacity_,
                            nbytes);
            }
            return false;
        }
        // Check that the required bytes to free is greater than zero.
        if (nbytes <= capacity_ - size_) {
            return true;
        }
        uint64_t required_bytes = nbytes - (capacity_ - size_);
        uint64_t evicted_bytes = 0;
        std::vector<uint64_t> victims;
        // Otherwise, make room in the cache for the new object.
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            if (evicted_bytes >= required_bytes) {
                break;
            }
            auto &m = map_.at(n->key);
            evicted_bytes += m.size_;
            victims.push_back(n->key);
        }
        // One cannot evict elements from the map one is iterating over.
        for (auto v : victims) {
            evict(v, EvictionCause::LRU);
        }
        return true;
    }

    void
    evict_accessed_object(CacheAccess const &access)
    {
        // TODO We should evict the old object, but I'm too lazy...
        //      I'd have to change the causes of eviction.
        (void)access;
        return;
    }

    void
    hit(CacheAccess const &access)
    {
        auto &metadata = map_.at(access.key);
        if (!ensure_enough_room(metadata.size_, access.size_bytes)) {
            statistics_.skip(access.size_bytes);
            evict_accessed_object(access);
            if (DEBUG) {
                LOGGER_WARN("too big updated object");
            }
            return;
        }
        update(access, metadata);
    }

    bool
    miss(CacheAccess const &access)
    {
        if (!ensure_enough_room(0, access.size_bytes)) {
            if (DEBUG) {
                LOGGER_WARN("not enough room to insert!");
            }
            statistics_.skip(access.size_bytes);
            return false;
        }
        insert(access);
        return true;
    }

public:
    /// @note   The {ttl,lru}_only parameters are deprecated. This wasn't
    ///         a particularly good idea nor was it particularly useful.
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    /// @param  uncertainty: double [0.0, 0.5] - The uncertainty we
    ///         allow in the prediction? (this is poorly phrased)
    /// @param  record_reaccess: bool
    ///         If true, then record the lifetime as the time between
    ///         either LRU evictions or reaccess.
    ///         Otherwise, record the lifetime as the time between
    ///         insertion and LRU eviction.
    /// @param  repredict_reaccess: bool
    ///         If true, then re-predict the queues on each reaccess.
    ///         Otherwise, predict the queue only on insertion.
    PredictiveCache(size_t const capacity,
                    double const uncertainty,
                    bool const record_reaccess,
                    bool const repredict_reaccess)
        : capacity_(capacity),
          record_reaccess_(record_reaccess),
          repredict_on_reaccess_(repredict_reaccess),
          lifetime_cache_(capacity, uncertainty, record_reaccess),
          oracle_(capacity)
    {
    }

    void
    start_simulation()
    {
        statistics_.start_simulation();
        oracle_.start_simulation();
    }

    void
    end_simulation()
    {
        statistics_.end_simulation();
        oracle_.end_simulation();
    }

    int
    access(CacheAccess const &access)
    {
        statistics_.time(access.timestamp_ms);
        assert(size_ == statistics_.size_);
        evict_expired_objects(access.timestamp_ms);
        lifetime_cache_.access(access);
        oracle_.access(access);
        if (map_.count(access.key)) {
            hit(access);
        } else {
            if (!miss(access)) {
                if (DEBUG) {
                    LOGGER_WARN("cannot handle miss");
                }
                return -1;
            }
        }
        return 0;
    }

    size_t
    size() const
    {
        return size_;
    }

    size_t
    capacity() const
    {
        return capacity_;
    }

    bool
    record_reaccess() const
    {
        return record_reaccess_;
    }

    bool
    repredict_on_reaccess() const
    {
        return repredict_on_reaccess_;
    }

    CacheMetadata const *
    get(uint64_t const key)
    {
        if (map_.count(key)) {
            return &map_.at(key);
        }
        return nullptr;
    }

    void
    print()
    {
        std::cout << "> PredictiveCache(sz: " << size_ << ", cap: " << capacity_
                  << ")\n";
        std::cout << "> \tLRU: ";
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            std::cout << n->key << ", ";
        }
        std::cout << "\n";
        std::cout << "> \tTTL: ";
        for (auto [tm, key] : ttl_cache_) {
            std::cout << key << "@" << tm << ", ";
        }
        std::cout << "\n";
    }

    PredictionTracker const &
    predictor() const
    {
        return pred_tracker;
    }

    CacheStatistics const &
    statistics() const
    {
        return statistics_;
    }

    void
    print_statistics() const
    {
        auto r = lifetime_cache_.thresholds();
        std::cout << "> {\"Capacity [B]\": " << format_memory_size(capacity_)
                  << ", \"Uncertainty\": " << lifetime_cache_.uncertainty()
                  << ", \"Record Reaccess\": " << record_reaccess_
                  << ", \"Repredict on Reaccess\": " << repredict_on_reaccess_
                  << ", \"CacheStatistics\": " << statistics_.json()
                  << ", \"PredictionTracker\": " << pred_tracker.json()
                  << ", \"Numer of Threshold Refreshes\": "
                  << format_engineering(lifetime_cache_.refreshes())
                  << ", \"Since Refresh\": "
                  << format_engineering(lifetime_cache_.since_refresh())
                  << ", \"Evictions\": "
                  << format_engineering(lifetime_cache_.evictions())
                  << ", \"Lower Threshold\": " << format_time(r.first)
                  << ", \"Upper Threshold\": " << format_time(r.second) << "}"
                  << std::endl;
    }

private:
    static constexpr bool DEBUG = false;

    // Maximum number of bytes in the cache.
    size_t const capacity_;
    bool const record_reaccess_;
    bool const repredict_on_reaccess_;

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

public:
    // Real (or SHARDS-ified) LRU cache to monitor the lifetime of elements.
    LifeTimeCache lifetime_cache_;
    // This wouldn't exist in the real cache, for obvious reasons.
    // This is just to enable collecting accuracy statistics.
    LRUTTLCache oracle_;
};
