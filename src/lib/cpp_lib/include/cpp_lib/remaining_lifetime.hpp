#pragma once

#include "cpp_lib/cache_metadata.hpp"
#include "cpp_struct/hash_list.hpp"
#include <cstdint>
#include <string>
#include <vector>

class RemainingLifetime {
public:
    /// @brief  Return the remaining lifetime of a sampled subset of the
    ///         LRU list.
    RemainingLifetime(HashList const &list,
                      std::unordered_map<uint64_t, CacheMetadata> const &cache,
                      uint64_t const current_time_ms,
                      uint64_t const nr_samples);

    std::string
    json() const;

private:
    uint64_t const nr_samples_;
    uint64_t const sampling_period_;
    std::vector<uint64_t> sizes_;
    std::vector<double> remaining_lifetimes_;
};
