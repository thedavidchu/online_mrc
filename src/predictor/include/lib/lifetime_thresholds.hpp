#pragma once
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <tuple>
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

    /// @brief  Return if any of the things are more than 1% out of whack.
    /// @note   How did I choose 1%? I just did, that's how.
    bool
    should_refresh() const
    {
        return true;
    }

public:
    LifeTimeThresholds(double const uncertainty)
        : uncertainty_(uncertainty),
          coarse_histogram_({0, 0, 0})
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

        if (lower_threshold > lifetime) {
            ++std::get<0>(coarse_histogram_);
        } else if (upper_threshold > lifetime) {
            ++std::get<1>(coarse_histogram_);
        } else {
            ++std::get<2>(coarse_histogram_);
        }
    }

    void
    refresh_thresholds()
    {
        auto x = recalculate_thresholds();
        lower_threshold = x.first;
        upper_threshold = x.second;
        std::cout << "Refreshed thresholds: " << lower_threshold << ", "
                  << upper_threshold << std::endl;
        coarse_histogram_ = {0, 0, 0};
    }

    /// @note   Automatically refresh the thresholds every 1<<20.
    std::pair<uint64_t, uint64_t>
    get_thresholds()
    {
        if (should_refresh()) {
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
    // This tells us how far off our current estimate of the thresholds
    // may possibly be.
    std::tuple<uint64_t, uint64_t, uint64_t> coarse_histogram_;
};
