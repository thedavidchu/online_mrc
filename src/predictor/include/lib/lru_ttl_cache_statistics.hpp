/** This is similar to the cache statistics, but specific to LRU/TTL. */
#pragma once

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/temporal_data.hpp"
#include "cpp_lib/temporal_sampler.hpp"
#include <cstdint>
#include <optional>
#include <string>

class LRUTTLStatistics {
    bool
    should_sample()
    {
        return temporal_sampler_.should_sample(current_time_ms_.value_or(0));
    }

    void
    sample()
    {
        temporal_times_.update(current_time_ms_.value_or(0));

        temporal_lru_sizes_.update(lru_size_);
        temporal_lru_sizes_bytes_.update(lru_size_bytes_);
        temporal_ttl_sizes_.update(ttl_size_);
        temporal_ttl_sizes_bytes_.update(ttl_size_bytes_);
    }

public:
    void
    access(CacheAccess const &access,
           uint64_t const lru_size,
           uint64_t const lru_size_bytes,
           uint64_t const ttl_size,
           uint64_t const ttl_size_bytes)
    {
        current_time_ms_ = access.timestamp_ms;

        lru_size_ = lru_size;
        lru_size_bytes_ = lru_size_bytes;
        ttl_size_ = ttl_size;
        ttl_size_bytes_ = ttl_size_bytes;

        if (should_sample()) {
            sample();
        }
    }

    std::string
    json() const
    {
        std::stringstream ss;
        ss << "{\"Temporal Times [ms]\": " << temporal_times_.str()
           << ", \"LRU Size [#]\": " << lru_size_
           << ", \"LRU Size [B]\": " << lru_size_bytes_
           << ", \"TTL Size [#]\": " << ttl_size_
           << ", \"TTL Size [B]\": " << ttl_size_bytes_
           << ", \"Temporal Sampler\": " << temporal_sampler_.json()
           << ", \"Temporal LRU Sizes [#]\": " << temporal_lru_sizes_.str()
           << ", \"Temporal LRU Sizes [B]\": "
           << temporal_lru_sizes_bytes_.str()
           << ", \"Temporal TTL Sizes [#]\": " << temporal_ttl_sizes_.str()
           << ", \"Temporal TTL Sizes [B]\": "
           << temporal_ttl_sizes_bytes_.str() << "}";
        return ss.str();
    }

private:
    std::optional<uint64_t> current_time_ms_ = std::nullopt;
    uint64_t lru_size_ = 0;
    uint64_t lru_size_bytes_ = 0;
    uint64_t ttl_size_ = 0;
    uint64_t ttl_size_bytes_ = 0;

    TemporalSampler temporal_sampler_{TemporalSampler::HOUR_IN_MS, false};

    TemporalData temporal_times_;
    TemporalData temporal_lru_sizes_;
    TemporalData temporal_lru_sizes_bytes_;
    TemporalData temporal_ttl_sizes_;
    TemporalData temporal_ttl_sizes_bytes_;
};
