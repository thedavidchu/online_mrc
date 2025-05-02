#pragma once

#include <cstddef>
#include <deque>

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

    double
    mean() const
    {
        double sum = 0.0;
        for (auto d : data_) {
            sum += d;
        }
        return sum / data_.size();
    }

private:
    size_t const max_size_ = 1 << 20;
    std::deque<double> data_;
};
