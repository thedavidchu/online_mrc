
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cpp_cache/cache_statistics.hpp"

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

    std::size_t
    size()
    {
        return map_.size();
    }

    std::optional<std::uint64_t>
    delete_lru()
    {
        if (eviction_queue_.size() == 0) {
            return {};
        }
        auto victim_it = eviction_queue_.begin();
        if (victim_it == eviction_queue_.end()) {
            return {};
        }
        std::uint64_t victim_key = victim_it->second;
        eviction_queue_.erase(victim_it);
        std::size_t i = map_.erase(victim_key);
        assert(i == 1);
        return victim_key;
    }

    int
    access_item(CacheAccess const &access)
    {
        assert(map_.size() == eviction_queue_.size());
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }
        if (map_.count(access.key)) {
            std::uint64_t prev_access_time = map_[access.key];
            auto nh = eviction_queue_.extract(prev_access_time);
            nh.key() = logical_time_;
            eviction_queue_.insert(std::move(nh));
            map_[access.key] = logical_time_;
            statistics_.deprecated_hit();
        } else {
            if (eviction_queue_.size() >= capacity_) {
                this->delete_lru();
                assert(map_.size() + 1 == capacity_);
            }
            assert(map_.size() + 1 <= capacity_);
            auto [it, inserted] = map_.emplace(access.key, logical_time_);
            assert(inserted && it->first == access.key &&
                   it->second == logical_time_);
            eviction_queue_.emplace(logical_time_, access.key);
            assert(map_.size() <= capacity_);
            statistics_.deprecated_miss();
        }
        ++logical_time_;
        return 0;
    }

    int
    delete_item(std::uint64_t const key)
    {
        assert(map_.size() == eviction_queue_.size());
        if (map_.count(key)) {
            std::uint64_t prev_access_time = map_[key];
            std::size_t i = eviction_queue_.erase(prev_access_time);
            assert(i == 1);
            i = map_.erase(key);
            assert(i == 1);
            return 0;
        }
        // Item not found, so return error!
        return -1;
    }
};
