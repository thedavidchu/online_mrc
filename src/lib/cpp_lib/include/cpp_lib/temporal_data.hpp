#pragma once

#include "cpp_lib/util.hpp"
#include <cmath>
#include <cstddef>
#include <deque>
#include <string>
#include <vector>

using size_t = std::size_t;

/// @brief  A sliding window to aggregate temporal statistics.
class TemporalData {
public:
    void
    update(double const x)
    {
        while (data_.size() >= max_size_) {
            data_.pop_front();
        }
        data_.push_back(x);
    }

    size_t
    size() const
    {
        return data_.size();
    }

    /// @note   Skip non-finite (INF or NAN) values; return alternate
    ///         value if the data array is empty.
    double
    finite_mean_or(double const alt) const
    {
        double sum = 0.0;
        if (data_.size() == 0) {
            return alt;
        }
        for (auto d : data_) {
            if (std::isfinite(d)) {
                sum += d;
            }
        }
        return sum / data_.size();
    }

    std::string
    str() const
    {
        // Admittedly not efficient to construct a whole new object.
        // But this is called infrequently and it's maintainable.
        std::vector<double> tmp(data_.begin(), data_.end());
        return vec2str(tmp);
    }

private:
    size_t const max_size_ = 1 << 20;
    std::deque<double> data_;
};
