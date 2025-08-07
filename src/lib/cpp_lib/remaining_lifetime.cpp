#include "cpp_lib/remaining_lifetime.hpp"

#include "cpp_lib/util.hpp"
#include "cpp_struct/hash_list.hpp"
#include "math/is_nth_iter.h"
#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

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
    return map2str(std::vector<std::pair<std::string, std::string>>{
        {".type", "\"RemainingLifetime\""},
        {"Samples [#]", val2str(nr_samples_)},
        {"Cache Sizes [B]", vec2str(sizes_)},
        {"Remaining Lifetimes [ms]", vec2str(remaining_lifetimes_)},
    });
}
