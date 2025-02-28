/** @brief  Metadata for a cache object. */
#pragma once

#include "cache_metadata/cache_access.hpp"
#include "math/saturation_arithmetic.h"

#include <cstdint>
#include <optional>
#include <ostream>

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
                  std::uint64_t const expiration_time_ms)
        : insertion_time_ms_(insertion_time_ms),
          last_access_time_ms_(insertion_time_ms),
          expiration_time_ms_(expiration_time_ms),
          visited(false)
    {
    }

    /// @brief  Initialize metadata for variable-sized value.
    CacheMetadata(std::size_t const value_size,
                  std::uint64_t const insertion_time_ms,
                  std::uint64_t const expiration_time_ms)
        : size_(value_size),
          insertion_time_ms_(insertion_time_ms),
          last_access_time_ms_(insertion_time_ms),
          expiration_time_ms_(expiration_time_ms),
          visited(false)
    {
    }

    CacheMetadata(CacheAccess const &access)
        : size_(access.size_bytes),
          insertion_time_ms_(access.timestamp_ms),
          last_access_time_ms_(access.timestamp_ms),
          expiration_time_ms_(
              saturation_add(access.timestamp_ms,
                             access.ttl_ms.value_or(UINT64_MAX))),
          visited(false)
    {
    }

    template <class Stream>
    void
    to_stream(Stream &s, bool const newline = false) const
    {
        s << "CacheMetadata(frequency=" << frequency_ << ",";
        s << "insertion_time[ms]=" << insertion_time_ms_ << ",";
        s << "last_access_time[ms]=" << last_access_time_ms_ << ",";
        s << "expiration_time[ms]=" << expiration_time_ms_ << ",";
        s << "visited=" << visited << ")";
        if (newline) {
            s << std::endl;
        }
    }

    void
    visit(std::uint64_t const access_time_ms,
          std::optional<std::uint64_t> const new_expiration_time_ms)
    {
        ++frequency_;
        last_access_time_ms_ = access_time_ms;
        if (new_expiration_time_ms) {
            expiration_time_ms_ = new_expiration_time_ms.value();
        }
    }

    void
    unvisit()
    {
        visited = false;
    }

    /// @brief  Get the TTL from the last access.
    uint64_t
    ttl_ms() const
    {
        return expiration_time_ms_ - last_access_time_ms_;
    }
};
