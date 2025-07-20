#pragma once

#include <cassert>
#include <string>

enum class EvictionCause {
    // Main capacity-based eviction policy.
    MainCapacity,
    ProactiveTTL,
    // We ran out of LRU objects to evict, so we fell back to our
    // secondary eviction policy, which is to evict the soonest expiring
    // object.
    VolatileTTL,
    // We tried accessing an expired object.
    // Maybe the TTL queue didn't track this object.
    // AKA 'Lazy TTL'.
    AccessExpired,
    // Updated object is too big for the cache.
    NoRoom,
    // Evicted due to sampling algorithm (e.g. Fixed-Size SHARDS)
    Sampling,
};

static inline std::string
EvictionCause__string(EvictionCause const cause)
{
    switch (cause) {
    case EvictionCause::MainCapacity:
        return "LRU";
    case EvictionCause::ProactiveTTL:
        return "ProactiveTTL";
    case EvictionCause::VolatileTTL:
        return "VolatileTTL";
    case EvictionCause::AccessExpired:
        return "AccessExpired";
    default:
        assert(0 && "impossible");
    }
}
