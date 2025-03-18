#pragma once

#include <cmath>
#include <cstdint>
#include <map>
#include <unordered_map>

using uint64_t = std::uint64_t;

class Histogram {
    std::map<double, uint64_t>
    ordered_histogram() const
    {
        std::map<double, uint64_t> ordered_hist;
        for (auto [b, frq] : histogram_) {
            ordered_hist[b] = frq;
        }
        return ordered_hist;
    }

public:
    /// @note   I just let it overflow...
    void
    update(double const bucket, uint64_t const frq = 1)
    {
        total_ += frq;
        histogram_[bucket] += frq;
    }

    double
    mean()
    {
        double accum = 0.0;
        for (auto [b, frq] : histogram_) {
            accum += b * frq;
        }
        return accum / total_;
    }

    double
    mode()
    {
        double mode = NAN;
        uint64_t mode_frq = 0;
        for (auto [b, frq] : histogram_) {
            if (frq >= mode_frq) {
                mode = b;
                mode_frq = frq;
            }
        }
        return mode;
    }

    /// @brief  Get where at least 'ratio' of objects are lesser.
    double
    percentile(double const ratio)
    {
        double target = ratio * total_;
        double cnt = 0.0;
        std::map<double, uint64_t> ordered_hist = ordered_histogram();
        for (auto [b, frq] : ordered_hist) {
            cnt += frq;
            if (cnt >= target) {
                return b;
            }
        }
        return INFINITY;
    }

private:
    uint64_t total_ = 0;
    std::unordered_map<double, uint64_t> histogram_;
};
