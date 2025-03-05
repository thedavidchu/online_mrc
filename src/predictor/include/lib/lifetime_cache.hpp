#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <sys/types.h>
#include <utility>

#include "cache_metadata/cache_metadata.hpp"
#include "lib/lifetime_thresholds.hpp"
#include "list/list.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class LifeTimeCache {
public:
    LifeTimeCache(size_t const capacity, double const uncertainty)
        : capacity_(capacity),
          thresholds_(uncertainty)
    {
    }

    bool
    contains(uint64_t const key)
    {
        // This is just to make sure I really do return 'true' or 'false'.
        return map_.count(key) ? true : false;
    }

    void
    access(CacheAccess const &access)
    {
        if (!first_time_ms_.has_value()) {
            first_time_ms_ = access.timestamp_ms;
        }
        current_time_ms_ = access.timestamp_ms;

        if (map_.count(access.key)) {
            map_.at(access.key).visit(access.timestamp_ms, {});
        } else {
            if (access.size_bytes > capacity_) {
                // Skip objects that are too large for the cache!
                return;
            }
            while (size_ + access.size_bytes > capacity_) {
                auto x = lru_cache_.remove_head();
                auto &node = map_.at(x->key);
                size_ -= node.size_;
                thresholds_.register_cache_eviction(current_time_ms_ -
                                                        node.insertion_time_ms_,
                                                    node.size_);
                // Erase everything
                map_.erase(x->key);
                std::free(x);
            }
            map_.emplace(access.key, CacheMetadata{access});
        }
    }

    /// @brief  Get the time thresholds in milliseconds.
    std::pair<uint64_t, uint64_t>
    thresholds()
    {
        auto r = thresholds_.thresholds();
        assert(r.first <= r.second);
        return r;
    }

    /// @brief  Get the number of times the thresholds have been refreshed.
    size_t
    refreshes() const
    {
        return thresholds_.refreshes();
    }

private:
    // Maximum number of bytes in the cache.
    size_t const capacity_;
    // Number of bytes in the current cache.
    size_t size_ = 0;

    std::optional<uint64_t> first_time_ms_ = std::nullopt;
    uint64_t current_time_ms_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;

    LifeTimeThresholds thresholds_;
};
