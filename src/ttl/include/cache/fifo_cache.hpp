
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_set>
#include <vector>

#include "cache/base_cache.hpp"
#include "cpp_cache/cache_statistics.hpp"

class FIFOCache {
    std::unordered_set<std::uint64_t> map_;
    std::vector<std::uint64_t> eviction_queue_;
    std::size_t const capacity_;
    std::uint64_t eviction_idx_ = 0;

public:
    static constexpr char name[] = "FIFOCache";
    CacheStatistics statistics_;

    FIFOCache(std::size_t capacity)
        : eviction_queue_(capacity),
          capacity_(capacity)
    {
    }

    int
    access_item(CacheAccess const &access)
    {
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }

        if (map_.count(access.key)) {
            statistics_.deprecated_hit();
            return 0;
        }

        auto [a, b] = map_.insert(access.key);
        assert(b);

        if (eviction_idx_ >= capacity_) {
            uint64_t victim_key = eviction_queue_[eviction_idx_ % capacity_];
            size_t i = map_.erase(victim_key);
            assert(i == 1);
        }

        eviction_queue_[eviction_idx_ % capacity_] = access.key;
        ++eviction_idx_;
        assert(map_.size() <= capacity_);
        statistics_.deprecated_miss();
        return 0;
    }
};
