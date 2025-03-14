#pragma once

enum class EvictionCause {
    LRU,
    TTL,
    VolatileTTL,
};
