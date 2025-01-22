/** @brief  Represent a cache access. */
#pragma once
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
    std::optional<std::uint64_t> const ttl_s;

    /// @brief  Initialize a CacheAccess object from a FullTraceItem.
    CacheAccess(struct FullTraceItem const *const item)
        : timestamp_ms(item->timestamp_ms),
          command((CacheAccessCommand)item->command),
          key(item->key),
          size_bytes(item->size),
          ttl_s(item->ttl_s ? std::nullopt : std::optional(item->ttl_s))
    {
    }

    CacheAccess(std::uint64_t const timestamp_ms, std::uint64_t const key)
        : timestamp_ms(timestamp_ms),
          command(CacheAccessCommand::get),
          key(key),
          size_bytes(1),
          ttl_s(std::nullopt)
    {
    }
};
