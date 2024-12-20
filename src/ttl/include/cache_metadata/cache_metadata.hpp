#pragma once

#include <cstdint>
#include <optional>
#include <ostream>

struct CacheMetadata {
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

    CacheMetadata(std::uint64_t const insertion_time_ms,
                  std::uint64_t const expiration_time_ms)
        : insertion_time_ms_(insertion_time_ms),
          last_access_time_ms_(insertion_time_ms),
          expiration_time_ms_(expiration_time_ms),
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
};
