#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <optional>
#include <sys/types.h>
#include <utility>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "lib/lifetime_thresholds.hpp"
#include "list/list.hpp"
#include "logger/logger.h"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

enum class LifeTimeCacheMode {
    // This is the time since the last access to the eviction.
    EvictionTime,
    // This is the time from the insertion to the eviction.
    LifeTime,
};

static inline LifeTimeCacheMode
LifeTimeCacheMode__parse(char const *const str)
{
    if (strcmp(str, "EvictionTime") == 0) {
        return LifeTimeCacheMode::EvictionTime;
    }
    if (strcmp(str, "LifeTime") == 0) {
        return LifeTimeCacheMode::LifeTime;
    }
    LOGGER_FATAL("unrecognized lifetime cache mode: %s", str);
    exit(1);
}

static inline char const *
LifeTimeCacheMode__str(LifeTimeCacheMode const mode)
{
    switch (mode) {
    case LifeTimeCacheMode::EvictionTime:
        return "EvictionTime";
    case LifeTimeCacheMode::LifeTime:
        return "LifeTime";
    default:
        assert(0 && "impossible");
    }
}

class LifeTimeCache {
    void
    ensure_enough_room(CacheAccess const &access)
    {
        while (size_ + access.size_bytes > capacity_) {
            auto x = lru_cache_.remove_head();
            auto &node = map_.at(x->key);
            size_ -= node.size_;
            switch (mode_) {
            case LifeTimeCacheMode::EvictionTime:
                thresholds_.register_cache_eviction(
                    current_time_ms_ - node.last_access_time_ms_,
                    node.size_);
                break;
            case LifeTimeCacheMode::LifeTime:
                thresholds_.register_cache_eviction(current_time_ms_ -
                                                        node.insertion_time_ms_,
                                                    node.size_);
                break;
            default:
                break;
            }
            // Erase everything
            map_.erase(x->key);
            std::free(x);
        }
    }

public:
    /// @param lower_ratio - acceptable level of not expiring objects.
    /// @param upper_ratio - acceptable level of not evicting objects.
    LifeTimeCache(size_t const capacity,
                  double const lower_ratio,
                  double const upper_ratio,
                  LifeTimeCacheMode const mode)
        : capacity_(capacity),
          thresholds_(lower_ratio, upper_ratio),
          mode_(mode)
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
            auto &node = map_.at(access.key);
            node.visit(access.timestamp_ms, {});
        } else {
            if (access.size_bytes > capacity_) {
                // Skip objects that are too large for the cache!
                return;
            }
            ensure_enough_room(access);
            map_.emplace(access.key, CacheMetadata{access});
            lru_cache_.access(access.key);
            size_ += access.size_bytes;
        }
    }

    /// @brief  Get the time thresholds in milliseconds.
    std::pair<uint64_t, uint64_t>
    thresholds() const
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

    size_t
    evictions() const
    {
        return thresholds_.evictions();
    }

    size_t
    since_refresh() const
    {
        return thresholds_.since_refresh();
    }

    double
    lower_ratio() const
    {
        return thresholds_.lower_ratio();
    }

    double
    upper_ratio() const
    {
        return thresholds_.upper_ratio();
    }

    LifeTimeCacheMode
    mode() const
    {
        return mode_;
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

    LifeTimeCacheMode const mode_;
};
