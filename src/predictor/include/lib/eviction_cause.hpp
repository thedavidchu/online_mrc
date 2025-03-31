#pragma once

#include <cassert>
#include <string>

enum class EvictionCause {
    LRU,
    TTL,
    // We ran out of LRU objects to evict, so we fell back to our
    // secondary eviction policy, which is to evict the soonest expiring
    // object.
    VolatileTTL,
    // We tried accessing an expired object.
    // Maybe the TTL queue didn't track this object.
    AccessExpired,
    // Updated object is too big for the cache.
    NoRoom,
};

static inline std::string
EvictionCause__string(EvictionCause const cause)
{
    switch (cause) {
    case EvictionCause::LRU:
        return "LRU";
    case EvictionCause::TTL:
        return "TTL";
    case EvictionCause::VolatileTTL:
        return "VolatileTTL";
    case EvictionCause::AccessExpired:
        return "AccessExpired";
    default:
        assert(0 && "impossible");
    }
}
