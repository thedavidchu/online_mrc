#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <utility>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

class LifeTimeThresholds {
private:
    std::pair<uint64_t, uint64_t>
    recalculate_thresholds() const
    {
        assert(uncertainty_ >= 0.0 && uncertainty_ <= 0.5);

        uint64_t lower = 0, upper = std::numeric_limits<uint64_t>::max();
        uint64_t accum = 0;
        uint64_t prev_lifetime = 0;
        bool found_lower = false;
        // Find the (50{+,-}uncertainty)th percentile lifetimes.
        for (auto [lifetime, frq] : histogram_) {
            accum += frq;
            if (!found_lower && (double)accum / total_ > 0.5 - uncertainty_) {
                lower = prev_lifetime;
                found_lower = true;
            }
            if ((double)accum / total_ >= 0.5 + uncertainty_) {
                upper = lifetime;
                break;
            }
            prev_lifetime = lifetime;
        }

        return {lower, upper};
    }

public:
    LifeTimeThresholds(double const uncertainty)
        : uncertainty_(uncertainty)
    {
        assert(uncertainty >= 0.0 && uncertainty <= 0.5);
    }

    void
    register_cache_eviction(uint64_t const lifetime, uint64_t const size)
    {
        // We are counting objects for now.
        // TODO Count bytes in the future, maybe?
        (void)size;
        total_ += 1;
        histogram_[lifetime] += 1;
    }

    void
    refresh_thresholds()
    {
        auto x = recalculate_thresholds();
        lower_threshold = x.first;
        upper_threshold = x.second;
    }

    /// @note   Automatically refresh the thresholds every 1<<20.
    std::pair<uint64_t, uint64_t>
    get_thresholds()
    {
        static uint64_t cnt = 0;
        if (cnt++ % (1 << 20) == 0) {
            refresh_thresholds();
        }
        return {lower_threshold, upper_threshold};
    }

private:
    double const uncertainty_;
    uint64_t lower_threshold = 0;
    uint64_t upper_threshold = std::numeric_limits<uint64_t>::max();

    uint64_t total_ = 0;
    std::map<uint64_t, uint64_t> histogram_;
};
