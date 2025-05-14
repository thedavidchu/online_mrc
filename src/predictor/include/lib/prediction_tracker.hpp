#pragma once

#include "cpp_lib/format_measurement.hpp"
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
        ss << "{" << "\"Guess LRU [#]\": " << format_engineering(guess_lru)
           << ", \"Guess TTL [#]\": " << format_engineering(guess_ttl)
           << ", \"Correct Evicts [#]\": "
           << format_engineering(right_evict_ops)
           << ", \"Correct Evicts [B]\": "
           << format_memory_size(right_evict_bytes)
           << ", \"Correct Expires [#]\": "
           << format_engineering(right_expire_ops)
           << ", \"Correct Expires [B]\": "
           << format_memory_size(right_expire_bytes)
           << ", \"Wrong Evicts [#]\": " << format_engineering(wrong_evict_ops)
           << ", \"Wrong Evicts [B]\": "
           << format_memory_size(wrong_evict_bytes)
           << ", \"Wrong Expires [#]\": "
           << format_engineering(wrong_expire_ops)
           << ", \"Wrong Expires [B]\": "
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
