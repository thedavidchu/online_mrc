
#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_metadata.hpp"
#include "cpp_cache/cache_statistics.hpp"
#include "list/list.hpp"
#include "logger/logger.h"

#include "cpp_cache/format_measurement.hpp"
#include "lib/eviction_cause.hpp"
#include "lib/util.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <optional>
#include <stdlib.h>
#include <sys/types.h>
#include <unordered_map>
#include <vector>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class LRUTTLCache {
private:
    void
    insert(CacheAccess const &access)
    {
        statistics_.insert(access.size_bytes);
        map_.emplace(access.key, CacheMetadata{access});
        lru_cache_.access(access.key);
        ttl_cache_.emplace(access.expiration_time_ms().value_or(UINT64_MAX),
                           access.key);
        size_ += access.size_bytes;
    }

    void
    update(CacheAccess const &access, CacheMetadata &metadata)
    {
        statistics_.update(metadata.size_, access.size_bytes);
        metadata.visit(access.timestamp_ms, {});
        lru_cache_.access(access.key);
    }

    /// @brief  Evict an object in the cache (either due to the eviction
    ///         policy or TTL expiration).
    void
    evict(uint64_t const victim_key, EvictionCause const cause)
    {
        CacheMetadata &m = map_.at(victim_key);
        uint64_t sz_bytes = m.size_;
        uint64_t exp_tm = m.expiration_time_ms_;

        // Update metadata tracking
        switch (cause) {
        case EvictionCause::LRU:
            statistics_.evict(m.size_);
            break;
        case EvictionCause::TTL:
            statistics_.expire(m.size_);
            break;
        default:
            assert(0 && "impossible");
        }

        // Evict from map.
        map_.erase(victim_key);
        size_ -= sz_bytes;
        // Evict from LRU queue.
        lru_cache_.remove(victim_key);
        // Evict from TTL queue.
        remove_multimap_kv(ttl_cache_, exp_tm, victim_key);
    }

    void
    evict_expired_objects(uint64_t const current_time_ms)
    {
        std::vector<uint64_t> victims;
        for (auto [exp_tm, key] : ttl_cache_) {
            if (exp_tm >= current_time_ms) {
                break;
            }
            victims.push_back(key);
        }
        // One cannot erase elements from a multimap while also iterating!
        for (auto victim : victims) {
            evict(victim, EvictionCause::TTL);
        }
    }

    bool
    ensure_enough_room(size_t const old_nbytes, size_t const new_nbytes)
    {
        // We already have enough room if we're not increasing the data.
        if (old_nbytes >= new_nbytes) {
            assert(size_ <= capacity_);
            return true;
        }
        size_t const nbytes = new_nbytes - old_nbytes;
        // We can't possibly fit the new object into the cache!
        // A side-effect is that in this case, we don't flush our cache
        // for no reason.
        if (nbytes > capacity_) {
            if (DEBUG) {
                LOGGER_WARN("cannot possibly evict enough bytes: need to evict "
                            "%zu, but can only evict %zu",
                            nbytes,
                            capacity_);
            }
            return false;
        }
        // Check that the required bytes to free is greater than zero.
        if (nbytes <= capacity_ - size_) {
            return true;
        }
        uint64_t required_bytes = nbytes - (capacity_ - size_);
        uint64_t evicted_bytes = 0;
        std::vector<uint64_t> victims;
        // Otherwise, make room in the cache for the new object.
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            if (evicted_bytes >= required_bytes) {
                break;
            }
            auto &m = map_.at(n->key);
            evicted_bytes += m.size_;
            victims.push_back(n->key);
        }
        // One cannot evict elements from the map one is iterating over.
        for (auto v : victims) {
            evict(v, EvictionCause::LRU);
        }
        return true;
    }

    void
    evict_accessed_object(CacheAccess const &access)
    {
        // TODO We should evict the old object, but I'm too lazy...
        //      I'd have to change the causes of eviction.
        (void)access;
        return;
    }

    void
    hit(CacheAccess const &access)
    {
        auto &metadata = map_.at(access.key);
        if (!ensure_enough_room(metadata.size_, access.size_bytes)) {
            statistics_.skip(access.size_bytes);
            evict_accessed_object(access);
            if (DEBUG) {
                LOGGER_WARN("too big updated object");
            }
            return;
        }
        update(access, metadata);
    }

    int
    miss(CacheAccess const &access)
    {
        if (!ensure_enough_room(0, access.size_bytes)) {
            if (DEBUG) {
                LOGGER_WARN("not enough room to insert!");
            }
            statistics_.skip(access.size_bytes);
            return -1;
        }
        insert(access);
        return 0;
    }

public:
    /// @param  capacity: size_t - The capacity of the cache in bytes.
    LRUTTLCache(size_t const capacity)
        : capacity_(capacity)
    {
    }

    int
    access(CacheAccess const &access)
    {
        statistics_.time(access.timestamp_ms);
        assert(size_ == statistics_.size_);
        evict_expired_objects(access.timestamp_ms);
        if (map_.count(access.key)) {
            hit(access);
        } else {
            int err = miss(access);
            if (err) {
                if (DEBUG) {
                    LOGGER_WARN("cannot handle miss");
                }
                return -1;
            }
        }
        return 0;
    }

    size_t
    size() const
    {
        return size_;
    }

    size_t
    capacity() const
    {
        return capacity_;
    }

    CacheMetadata const *
    get(uint64_t const key)
    {
        if (map_.count(key)) {
            return &map_.at(key);
        }
        return nullptr;
    }

    CacheStatistics const &
    statistics() const
    {
        return statistics_;
    }

    void
    print() const
    {
        std::cout << "> LRUTTLCache(sz: " << size_ << ", cap: " << capacity_
                  << ")\n";
        std::cout << "> \tLRU: ";
        for (auto n = lru_cache_.begin(); n != lru_cache_.end(); n = n->r) {
            std::cout << n->key << ", ";
        }
        std::cout << "\n";
        std::cout << "> \tTTL: ";
        for (auto [tm, key] : ttl_cache_) {
            std::cout << key << "@" << tm << ", ";
        }
        std::cout << "\n";
    }

    void
    print_statistics() const
    {
        std::cout << "> {\"Capacity [B]\": " << format_memory_size(capacity_)
                  << ", \"Max Size [B]\": "
                  << format_memory_size(statistics_.max_size_)
                  << ", \"Max Resident Objects\": "
                  << format_engineering(statistics_.max_resident_objs_)
                  << ", \"Uptime [ms]\": "
                  << format_time(statistics_.uptime_ms())
                  << ", \"Number of Insertions\": "
                  << format_engineering(statistics_.insert_ops_)
                  << ", \"Number of Updates\": "
                  << format_engineering(statistics_.update_ops_)
                  << ", \"Miss Ratio\": " << statistics_.miss_rate()
                  << ", \"Statistics\": " << statistics_.json() << "}"
                  << std::endl;
    }

private:
    static constexpr bool DEBUG = false;

    // Maximum number of bytes in the cache.
    size_t const capacity_;

    // Number of bytes in the current cache.
    size_t size_ = 0;

    // Maps key to [last access time, expiration time]
    std::unordered_map<uint64_t, CacheMetadata> map_;
    // Maps last access time to keys.
    List lru_cache_;
    // Maps expiration time to keys.
    std::multimap<uint64_t, uint64_t> ttl_cache_;

    // Statistics related to cache performance.
    CacheStatistics statistics_;
};
