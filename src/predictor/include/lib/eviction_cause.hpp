#pragma once

#include <string>

enum class EvictionCause {
    LRU,
    TTL,
    VolatileTTL,
    AccessExpired,
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
