/** @brief  Metadata for a cache object. */
#pragma once

#include "cpp_lib/cache_access.hpp"

#include <cstdint>
#include <iostream>
#include <optional>

struct CacheMetadata {
    /// @note   Size of the object's value in bytes (since the key is a
    ///         uint64 and this metadata is considered "extra" but
    ///         constant). The default is 1 for a unit sized cache.
    std::size_t size_ = 1;
    /// @note   We don't consider the first access in the frequency
    ///         counter. There's no real reason, I just think it's nice
    ///         to start at 0 rather than 1.
    /// @todo   Start frequency counter from 1. It just makes more sense.
    ///         This is sort of like the "hit counter".
    std::size_t frequency_ = 0;
    std::uint64_t insertion_time_ms_ = 0;
    std::uint64_t last_access_time_ms_ = 0;
    /// @note   I decided to store the expiration time rather than the
    ///         TTL because the TTL stops making sense after time moves
    ///         onward. Of course, we can figure out what the expiration
    ///         time is, but that's more work.
    double expiration_time_ms_ = 0;
    bool visited = false;

    /// @brief  Initialize metadata for unit-sized value size.
    CacheMetadata(std::uint64_t const insertion_time_ms,
                  double const expiration_time_ms);

    /// @brief  Initialize metadata for variable-sized value.
    CacheMetadata(std::size_t const value_size,
                  std::uint64_t const insertion_time_ms,
                  double const expiration_time_ms);

    CacheMetadata(CacheAccess const &access);

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

    /// @note   Does not update expiration time.
    void
    visit_without_ttl_refresh(CacheAccess const &access);

    /// @note   Updates expiration time.
    void
    visit(std::uint64_t const access_time_ms,
          std::optional<double> const new_expiration_time_ms = std::nullopt);

    void
    unvisit();

    double
    ttl_ms(uint64_t const current_time_ms) const;
};
