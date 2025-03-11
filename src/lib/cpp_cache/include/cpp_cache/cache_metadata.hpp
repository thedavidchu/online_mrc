/** @brief  Metadata for a cache object. */
#pragma once

#include "cpp_cache/cache_access.hpp"

#include <cstdint>
#include <optional>

struct CacheMetadata {
    /// @note   Size of the object's value in bytes (since the key is a
    ///         uint64 and this metadata is considered "extra" but
    ///         constant). The default is 1 for a unit sized cache.
    std::size_t size_ = 1;
    /// @note   We don't consider the first access in the frequency
    ///         counter. There's no real reason, I just think it's nice
    ///         to start at 0 rather than 1.
    std::size_t frequency_ = 0;
    std::uint64_t insertion_time_ms_ = 0;
    std::uint64_t last_access_time_ms_ = 0;
    /// @note   I decided to store the expiration time rather than the
    ///         TTL for convenience. The TTL can be calculated by
    ///         subtracting the (last time the expiration time was set)
    ///         from the (expiration time).
    std::uint64_t expiration_time_ms_ = 0;
    bool visited = false;

    /// @brief  Initialize metadata for unit-sized value size.
    CacheMetadata(std::uint64_t const insertion_time_ms,
                  std::uint64_t const expiration_time_ms);

    /// @brief  Initialize metadata for variable-sized value.
    CacheMetadata(std::size_t const value_size,
                  std::uint64_t const insertion_time_ms,
                  std::uint64_t const expiration_time_ms);

    CacheMetadata(CacheAccess const &access);

    template <class Stream>
    void
    to_stream(Stream &s, bool const newline = false) const;

    void
    visit(std::uint64_t const access_time_ms,
          std::optional<std::uint64_t> const new_expiration_time_ms);

    void
    unvisit();

    /// @brief  Get the TTL from the last access.
    uint64_t
    ttl_ms() const;
};
