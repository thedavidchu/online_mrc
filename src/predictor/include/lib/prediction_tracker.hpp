#pragma once

#include "cpp_cache/format_measurement.hpp"
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <string>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class PredictionTracker {
public:
    /// @brief  Record a store in the LRU queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    record_store_lru()
    {
        ++guess_lru;
        return true;
    }

    /// @brief  Record a store in the TTL queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    record_store_ttl()
    {
        ++guess_ttl;
        return true;
    }

    void
    update_correctly_evicted(size_t const bytes)
    {
        right_evict_bytes += bytes;
        right_evict_ops += 1;
    }

    void
    update_correctly_expired(size_t const bytes)
    {
        right_expire_bytes += bytes;
        right_expire_ops += 1;
    }

    void
    update_wrongly_evicted(size_t const bytes)
    {
        wrong_evict_bytes += bytes;
        wrong_evict_ops += 1;
    }

    void
    update_wrongly_expired(size_t const bytes)
    {
        wrong_expire_bytes += bytes;
        wrong_expire_ops += 1;
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{" << "\"guess_lru\": " << format_engineering(guess_lru)
           << ", \"guess_ttl\": " << format_engineering(guess_ttl)
           << ", \"right_evict_ops\": " << format_engineering(right_evict_ops)
           << ", \"right_evict_bytes\": "
           << format_memory_size(right_evict_bytes)
           << ", \"right_expire_ops\": " << format_engineering(right_expire_ops)
           << ", \"right_expire_bytes\": "
           << format_memory_size(right_expire_bytes)
           << ", \"wrong_evict_ops\": " << format_engineering(wrong_evict_ops)
           << ", \"wrong_evict_bytes\": "
           << format_memory_size(wrong_evict_bytes)
           << ", \"wrong_expire_ops\": " << format_engineering(wrong_expire_ops)
           << ", \"wrong_expire_bytes\": "
           << format_memory_size(wrong_expire_bytes) << "}";
        return ss.str();
    }

    uint64_t guess_lru = 0;
    uint64_t guess_ttl = 0;

    uint64_t right_evict_ops = 0;
    uint64_t right_evict_bytes = 0;

    uint64_t right_expire_ops = 0;
    uint64_t right_expire_bytes = 0;

    uint64_t wrong_evict_ops = 0;
    uint64_t wrong_evict_bytes = 0;

    uint64_t wrong_expire_ops = 0;
    uint64_t wrong_expire_bytes = 0;
};
