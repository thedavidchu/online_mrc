/** @brief  Represent a cache access. */
#pragma once
#include <cassert>
#include <cstdint>
#include <optional>

#include "trace/trace.h"

enum class CacheAccessCommand : uint8_t {
    get,
    set,
};

struct CacheAccess {
    std::uint64_t const timestamp_ms;
    CacheAccessCommand const command;
    std::uint64_t const key;
    std::uint32_t const size_bytes;
    std::optional<std::uint64_t> const ttl_ms;

    /// @brief  Initialize a CacheAccess object from a FullTraceItem.
    CacheAccess(struct FullTraceItem const *const item);

    CacheAccess(std::uint64_t const timestamp_ms, std::uint64_t const key);

    CacheAccess(std::uint64_t const timestamp_ms,
                std::uint64_t const key,
                std::uint64_t size_bytes,
                std::optional<std::uint64_t> ttl_ms);
};
