#include "cpp_lib/remaining_lifetime.hpp"

#include "cpp_lib/cache_metadata.hpp"
#include "cpp_lib/util.hpp"
#include "cpp_struct/hash_list.hpp"
#include "math/is_nth_iter.h"
#include <algorithm>
#include <cstdint>
#include <sstream>
#include <unordered_map>

RemainingLifetime::RemainingLifetime(
    HashList const &list,
    std::unordered_map<uint64_t, CachePredictiveMetadata> const &cache,
    uint64_t const current_time_ms,
    uint64_t const nr_samples)
    : nr_samples_(std::min(nr_samples, list.size())),
      sampling_period_(nr_samples_ == 0 ? UINT64_MAX
                                        : list.size() / nr_samples_)
{
    uint64_t i = 0;
    uint64_t size = 0;
    sizes_.reserve(nr_samples_);
    remaining_lifetimes_.reserve(nr_samples_);
    for (auto n : list) {
        auto const &m = cache.at(n->key);
        size += m.size_;
        if (is_nth_iter(i++, sampling_period_)) {
            sizes_.push_back(size);
            remaining_lifetimes_.push_back(m.ttl_ms(current_time_ms));
        }
    }

    // Correct the sizes (since it's backwards).
    for (auto &s : sizes_) {
        s = size - s;
    }
}

std::string
RemainingLifetime::json() const
{
    std::stringstream ss;
    ss << "{";
    ss << "\".type\": " << "\"RemainingLifetime\", ";
    ss << "\"Samples [#]\": " << nr_samples_ << ", ";
    ss << "\"Cache Sizes [B]\": " << vec2str(sizes_) << ", ";
    ss << "\"Remaining Lifetimes [ms]\": " << vec2str(remaining_lifetimes_);
    ss << "}";
    return ss.str();
}
