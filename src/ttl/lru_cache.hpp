
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "cache_statistics.hpp"

class LRUCache {
    std::unordered_map<std::uint64_t, std::uint64_t> map_;
    std::map<std::uint64_t, std::uint64_t> eviction_queue_;
    std::size_t const capacity_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "LRUCache";
    CacheStatistics statistics_;

    LRUCache(std::size_t capacity)
        : capacity_(capacity)
    {
    }

    int
    access_item(std::uint64_t const key)
    {
        assert(map_.size() == eviction_queue_.size());
        if (map_.count(key)) {
            std::uint64_t prev_access_time = map_[key];
            auto nh = eviction_queue_.extract(prev_access_time);
            nh.key() = logical_time_;
            eviction_queue_.insert(std::move(nh));
            map_[key] = logical_time_;
            statistics_.hit();
        } else {
            if (eviction_queue_.size() >= capacity_) {
                auto victim_it = eviction_queue_.begin();
                std::uint64_t victim_key = victim_it->second;
                eviction_queue_.erase(victim_it);
                size_t i = map_.erase(victim_key);
                assert(i == 1);
                assert(map_.size() + 1 == capacity_);
            }
            auto [it, inserted] = map_.emplace(key, logical_time_);
            assert(inserted && it->first == key && it->second == logical_time_);
            eviction_queue_.emplace(logical_time_, key);
            assert(map_.size() <= capacity_);
            statistics_.miss();
        }
        ++logical_time_;
        return 0;
    }
};
