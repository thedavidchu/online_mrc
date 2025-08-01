#pragma once

#include "accurate/accurate.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/cache_statistics.hpp"
#include "cpp_lib/util.hpp"
#include "lib/eviction_cause.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <map>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

class TTL_Cache : public Accurate {
private:
    void
    insert(CacheAccess const &access) override final
    {
        statistics_.insert(access.size_bytes());
        map_.emplace(access.key, CacheMetadata{access});
        ttl_queue_.emplace(access.expiration_time_ms(), access.key);
        size_bytes_ += access.size_bytes();
    }

    void
    update(CacheAccess const &access, CacheMetadata &metadata) override final
    {
        size_bytes_ += access.size_bytes() - metadata.size_;
        statistics_.update(metadata.size_, access.size_bytes());
        metadata.visit_without_ttl_refresh(access);
    }

    void
    remove(uint64_t const victim_key,
           EvictionCause const cause,
           CacheAccess const &access) override final
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
    remove_expired(CacheAccess const &access) override final
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
        expiration_work_ += victims.size();
    }

public:
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    TTL_Cache(uint64_t const capacity_bytes)
        : Accurate{capacity_bytes}
    {
    }

private:
    // Maps expiration time to keys.
    std::multimap<double, uint64_t> ttl_queue_;
};
