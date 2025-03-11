#pragma once

#include <cstddef>
#include <cstdint>

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
        ++guessed_lru_;
        return true;
    }

    /// @brief  Record a store in the TTL queue.
    /// @param access: CacheAccess const & - cache access.
    /// @param dt: uint64_t const - time the cache has been alive.
    bool
    record_store_ttl()
    {
        ++guessed_ttl_;
        return true;
    }

    void
    update_correctly_evicted(size_t const bytes)
    {
        correctly_evicted_bytes_ += bytes;
        correctly_evicted_ops_ += 1;
    }

    void
    update_correctly_expired(size_t const bytes)
    {
        correctly_expired_bytes_ += bytes;
        correctly_expired_ops_ += 1;
    }

    void
    update_wrongly_evicted(size_t const bytes)
    {
        wrongly_evicted_bytes_ += bytes;
        wrongly_evicted_ops_ += 1;
    }

    void
    update_wrongly_expired(size_t const bytes)
    {
        wrongly_expired_bytes_ += bytes;
        wrongly_expired_ops_ += 1;
    }

    uint64_t guessed_lru_ = 0;
    uint64_t guessed_ttl_ = 0;

    uint64_t correctly_evicted_bytes_ = 0;
    uint64_t correctly_expired_bytes_ = 0;
    uint64_t wrongly_evicted_bytes_ = 0;
    uint64_t wrongly_expired_bytes_ = 0;

    // Track the number of correct/wrong operations
    uint64_t correctly_evicted_ops_ = 0;
    uint64_t correctly_expired_ops_ = 0;
    uint64_t wrongly_evicted_ops_ = 0;
    uint64_t wrongly_expired_ops_ = 0;
};
