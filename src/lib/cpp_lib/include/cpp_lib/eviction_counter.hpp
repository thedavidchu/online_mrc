#pragma once

#include "cpp_lib/format_measurement.hpp"
#include <cmath>
#include <cstdint>
#include <sstream>
#include <string>

using uint64_t = std::uint64_t;

class EvictionCounter {
public:
    void
    evict(uint64_t const size_bytes, double const ttl_ms)
    {
        ops_ += 1;
        bytes_ += size_bytes;

        if (ttl_ms > 0.0) {
            preexpire_evict_ops_ += 1;
            preexpire_evict_bytes_ += size_bytes;
            preexpire_evict_ms_ += ttl_ms;
            if (std::isfinite(ttl_ms)) {
                preexpire_evict_ms_bytes_ += size_bytes * ttl_ms;
            }
        } else if (ttl_ms < 0.0) {
            postexpire_evict_ops_ += 1;
            postexpire_evict_bytes_ += size_bytes;
            postexpire_evict_ms_ += -ttl_ms;
            if (std::isfinite(ttl_ms)) {
                postexpire_evict_ms_bytes_ += size_bytes * ttl_ms;
            }
        } else {
            atexpire_evict_ops_ += 1;
            atexpire_evict_bytes_ += size_bytes;
        }
    }

    uint64_t
    ops() const
    {
        return ops_;
    }

    uint64_t
    bytes() const
    {
        return bytes_;
    }

    /// @brief  Return a JSON string without a newline.
    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{";
        ss << "\"[#]\": " << format_engineering(ops_);
        ss << ", \"[B]\": " << format_memory_size(bytes_);

        // --- Pre-expire statistics. ---
        ss << ", \"Pre-Expire Evicts [#]\": "
           << format_engineering(preexpire_evict_ops_);
        ss << ", \"Pre-Expire Evicts [B]\": "
           << format_memory_size(preexpire_evict_bytes_);
        ss << ", \"Pre-Expire Evicts [ms]\": "
           << format_time(preexpire_evict_ms_);
        ss << ", \"Pre-Expire Evicts [ms.B]\": "
           << format_engineering(preexpire_evict_ms_bytes_);

        // --- At-expire statistics. ---
        ss << ", \"At-Expire Evicts [#]\": "
           << format_engineering(atexpire_evict_ops_);
        ss << ", \"At-Expire Evicts [B]\": "
           << format_memory_size(atexpire_evict_bytes_);

        // --- Post-expire statistics. ---
        ss << ", \"Post-Expire Evicts [#]\": "
           << format_engineering(postexpire_evict_ops_);
        ss << ", \"Post-Expire Evicts [B]\": "
           << format_memory_size(postexpire_evict_bytes_);
        ss << ", \"Post-Expire Evicts [ms]\": "
           << format_time(postexpire_evict_ms_);
        ss << ", \"Post-Expire Evicts [ms.B]\": "
           << format_engineering(postexpire_evict_ms_bytes_);

        ss << "}";
        return ss.str();
    }

private:
    uint64_t ops_ = 0;
    uint64_t bytes_ = 0;

    uint64_t preexpire_evict_ops_ = 0;
    uint64_t preexpire_evict_bytes_ = 0;
    double preexpire_evict_ms_ = 0.0;
    double preexpire_evict_ms_bytes_ = 0.0;

    // By defintion, the time components will be zero.
    uint64_t atexpire_evict_ops_ = 0;
    uint64_t atexpire_evict_bytes_ = 0;

    uint64_t postexpire_evict_ops_ = 0;
    uint64_t postexpire_evict_bytes_ = 0;
    double postexpire_evict_ms_ = 0.0;
    double postexpire_evict_ms_bytes_ = 0.0;
};
