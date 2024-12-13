
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include "cache_statistics.hpp"

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
    access_item(std::uint64_t const key)
    {
        if (capacity_ == 0) {
            statistics_.miss();
            return 0;
        }

        if (map_.count(key)) {
            statistics_.hit();
            return 0;
        }

        auto [a, b] = map_.insert(key);
        assert(b);

        if (eviction_idx_ >= capacity_) {
            uint64_t victim_key = eviction_queue_[eviction_idx_ % capacity_];
            size_t i = map_.erase(victim_key);
            assert(i == 1);
        }

        eviction_queue_[eviction_idx_ % capacity_] = key;
        ++eviction_idx_;
        assert(map_.size() <= capacity_);
        statistics_.miss();
        return 0;
    }
};
