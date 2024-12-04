#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "cache_statistics.hpp"
#include "logger/logger.h"

class ClockCache {
    std::unordered_map<std::uint64_t, bool> map_;
    std::vector<std::uint64_t> eviction_queue_;
    std::size_t const capacity_;
    std::uint64_t eviction_idx_ = 0;

public:
    static constexpr char name[] = "ClockCache";
    CacheStatistics statistics_;

    ClockCache(std::size_t capacity)
        : eviction_queue_(capacity),
          capacity_(capacity)
    {
    }

    int
    access_item(std::uint64_t const key)
    {
        assert(map_.size() <= capacity_);
        assert(eviction_queue_.size() == capacity_);

        if (map_.count(key)) {
            map_[key] = true;
            statistics_.hit();
            assert(map_.size() <= capacity_);
            return 0;
        }

        if (eviction_idx_ >= capacity_) {
            assert(map_.size() == capacity_);
            while (true) {
                uint64_t victim_key =
                    eviction_queue_[eviction_idx_ % capacity_];
                if (!map_[victim_key]) {
                    size_t i = map_.erase(victim_key);
                    assert(i == 1);
                    eviction_queue_[eviction_idx_ % capacity_] = key;

                    // We should be sure that we can correctly add
                    // another element to the cache, since we should
                    // have evicted one.
                    assert(map_.size() + 1 == capacity_);
                    break;
                } else {
                    ++eviction_idx_;
                    map_[victim_key] = false;
                }
            }
        }

        assert(map_.size() < capacity_);
        map_[key] = false;
        eviction_queue_[eviction_idx_ % capacity_] = key;
        ++eviction_idx_;
        assert(map_.size() <= capacity_);
        statistics_.miss();
        return 0;
    }
};
