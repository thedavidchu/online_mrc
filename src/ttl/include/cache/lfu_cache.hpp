
#pragma once

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <unordered_map>

#include "cache/base_cache.hpp"
#include "cache/lru_cache.hpp"
#include "cpp_lib/cache_statistics.hpp"

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
    }

    std::optional<std::uint64_t>
    evict_lfu()
    {
        for (auto &[frq, lru_cache] : eviction_queue_) {
            if (lru_cache.size() != 0) {
                auto maybe_victim_key = lru_cache.delete_lru();
                assert(maybe_victim_key);
                std::uint64_t victim_key = maybe_victim_key.value();
                std::size_t i = map_.erase(victim_key);
                assert(i == 1);
                return victim_key;
            }
        }
        return {};
    }

    int
    access_item(CacheAccess const &access)
    {
        int err = 0;
        if (capacity_ == 0) {
            statistics_.deprecated_miss();
            return 0;
        }
        // NOTE 'map_.size()' doesn't necessarily match
        //      'eviction_queue_.size()', because the latter is now a
        //      hierarchical data structure containing LRUCaches.
        if (map_.count(access.key)) {
            std::uint64_t prev_frq = map_[access.key];
            auto it_curr = eviction_queue_.find(prev_frq);
            assert(it_curr != eviction_queue_.end());
            LRUCache &lru_cache = it_curr->second;
            err = lru_cache.delete_item(access.key);
            assert(!err);
            // NOTE Delete the previous LRUCache if it is now empty.
            //      This is because an item with a very high frequency
            //      should not create a whole bunch of empty LRUCaches
            //      as we slowly increment it.
            if (lru_cache.size() == 0) {
                eviction_queue_.erase(prev_frq);
            }
            auto it_next = eviction_queue_.find(prev_frq + 1);
            if (it_next == eviction_queue_.end()) {
                eviction_queue_.emplace(prev_frq + 1, LRUCache(capacity_));
                it_next = eviction_queue_.find(prev_frq + 1);
            }
            assert(it_next != eviction_queue_.end());
            it_next->second.access_item(access);
            map_[access.key] += 1;
            statistics_.deprecated_hit();
        } else {
            assert(map_.size() <= capacity_);
            if (map_.size() >= capacity_) {
                this->evict_lfu();
                assert(map_.size() + 1 == capacity_);
            }
            assert(map_.size() + 1 <= capacity_);
            std::uint64_t frq = 0;
            auto [it, inserted] = map_.emplace(access.key, frq);
            assert(inserted && it->first == access.key && it->second == frq);
            if (eviction_queue_.count(frq) == 0) {
                eviction_queue_.emplace(frq, LRUCache(capacity_));
            }
            eviction_queue_.find(frq)->second.access_item(access);
            assert(map_.size() <= capacity_);
            statistics_.deprecated_miss();
        }
        assert(map_.count(access.key));
        ++logical_time_;
        return 0;
    }
};
