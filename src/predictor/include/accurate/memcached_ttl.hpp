#pragma once

#include "accurate/accurate.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/format_measurement.hpp"
#include "cpp_lib/temporal_sampler.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class CacheLibTTL : public Accurate {
private:
    void
    insert(CacheAccess const &access)
    {
        statistics_.insert(access.size_bytes());
        map_.emplace(access.key, CacheMetadata{access});
        ttl_queue_.emplace(access.expiration_time_ms(), access.key);
        size_bytes_ += access.size_bytes();
    }

    void
    update(CacheAccess const &access, CacheMetadata &metadata)
    {
        size_bytes_ += access.size_bytes() - metadata.size_;
        statistics_.update(metadata.size_, access.size_bytes());
        metadata.visit_without_ttl_refresh(access);
    }

    void
    remove(uint64_t const victim_key,
           EvictionCause const cause,
           CacheAccess const &access)
    {
        assert(map_.contains(victim_key));
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        double exp_tm = m.expiration_time_ms_;

        // Update metadata tracking
        switch (cause) {
        case EvictionCause::ProactiveTTL:
            statistics_.ttl_expire(m.size_);
            break;
        case EvictionCause::AccessExpired:
            statistics_.lazy_expire(m.size_, m.ttl_ms(access.timestamp_ms));
            break;
        default:
            assert(0 && "impossible");
        }

        size_bytes_ -= sz_bytes;
        map_.erase(victim_key);
        remove_multimap_kv(ttl_queue_, exp_tm, victim_key);
    }

    void
    remove_expired_accessed_object(CacheAccess const &access)
    {
        if (map_.contains(access.key) &&
            map_.at(access.key).ttl_ms(access.timestamp_ms) < 0.0) {
            remove(access.key, EvictionCause::AccessExpired, access);
        }
    }
    void
    evict_expired_objects(CacheAccess const &access)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_queue_) {
            if (exp_tm >= access.timestamp_ms) {
                break;
            }
            victims.push_back(key);
        }
        // One cannot erase elements from a multimap while also iterating!
        for (auto victim : victims) {
            remove(victim, EvictionCause::ProactiveTTL, access);
        }
    }

public:
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    CacheLibTTL(size_t const capacity)
        : capacity_bytes_(capacity),
          sampler_{3600 * 1000, false, false}
    {
    }

    void
    start_simulation()
    {
        statistics_.start_simulation();
    }

    void
    end_simulation()
    {
        statistics_.end_simulation();
    }

    void
    access(CacheAccess const &access)
    {
        assert(size_bytes_ == statistics_.size_);
        statistics_.time(access.timestamp_ms);
        assert(size_bytes_ == statistics_.size_);
        remove_expired_accessed_object(access);
        if (sampler_.should_sample(access.timestamp_ms)) {
            evict_expired_objects(access);
        }
        if (map_.contains(access.key)) {
            update(access, map_.at(access.key));
        } else {
            insert(access);
        }
    }

    std::string
    json(std::unordered_map<std::string, std::string> extras = {}) const
    {
        std::stringstream ss;
        ss << "{\"Capacity [B]\": " << format_memory_size(capacity_bytes_)
           << ", \"Max Size [B]\": "
           << format_memory_size(statistics_.max_size_)
           << ", \"Max Resident Objects\": "
           << format_engineering(statistics_.max_resident_objs_)
           << ", \"Uptime [ms]\": " << format_time(statistics_.uptime_ms())
           << ", \"Number of Insertions\": "
           << format_engineering(statistics_.insert_ops_)
           << ", \"Number of Updates\": "
           << format_engineering(statistics_.update_ops_)
           << ", \"Miss Ratio\": " << statistics_.miss_ratio()
           << ", \"Statistics\": " << statistics_.json()
           << ", \"Extras\": " << map2str(extras) << "}";
        return ss.str();
    }

private:
    // Maximum number of bytes in the cache.
    size_t const capacity_bytes_;
    // Number of bytes in the current cache.
    size_t size_bytes_ = 0;
    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps expiration time to keys.
    std::multimap<double, uint64_t> ttl_queue_;
    // Statistics related to cache performance.
    CacheStatistics statistics_;
    TemporalSampler sampler_;
};
