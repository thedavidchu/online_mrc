
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <unordered_map>

#include "cache_statistics.hpp"
#include "lru_cache.hpp"

class LFUCache {
    std::unordered_map<std::uint64_t, std::uint64_t> map_;
    std::map<std::uint64_t, LRUCache> eviction_queue_;
    std::size_t const capacity_;
    std::uint64_t logical_time_ = 0;

public:
    static constexpr char name[] = "LFUCache";
    CacheStatistics statistics_;

    LFUCache(std::size_t capacity)
        : capacity_(capacity)
    {
        assert(capacity_ != 0);
    }

    int
    evict_lfu()
    {
        for (auto victim_it = eviction_queue_.begin();
             victim_it != eviction_queue_.end();
             ++victim_it) {
            LRUCache victim_cache = victim_it->second;
            if (victim_cache.delete_lru() == 0) {
                break;
            }
        }
        return 0;
    }

    int
    access_item(std::uint64_t const key)
    {
        int err = 0;
        // NOTE 'map_.size()' doesn't necessarily match
        //      'eviction_queue_.size()', because the latter is now a
        //      hierarchical data structure containing LRUCaches.
        if (map_.count(key)) {
            std::uint64_t prev_frq = map_[key];
            err = eviction_queue_[prev_frq].delete_item(key);
            assert(!err);
            // NOTE Delete the previous LRUCache if it is now empty.
            //      This is because an item with a very high frequency
            //      should not create a whole bunch of empty LRUCaches
            //      as we slowly increment it.
            if (eviction_queue_[prev_frq].size() == 0) {
                eviction_queue_.erase(prev_frq);
            }
            if (!eviction_queue_.count(prev_frq + 1)) {
                eviction_queue_.emplace(prev_frq + 1, LRUCache(capacity_));
            }
            eviction_queue_[prev_frq + 1].access_item(key);
            map_[key] += 1;
            statistics_.hit();
        } else {
            if (map_.size() >= capacity_) {
                this->evict_lfu();
            }
            auto [it, inserted] = map_.emplace(key, logical_time_);
            assert(inserted && it->first == key && it->second == logical_time_);
            if (!eviction_queue_.count(0)) {
                eviction_queue_.emplace(0, LRUCache(capacity_));
            }
            eviction_queue_.at(0).access_item(key);
            assert(map_.size() <= capacity_);
            statistics_.miss();
        }
        ++logical_time_;
        return 0;
    }
};
