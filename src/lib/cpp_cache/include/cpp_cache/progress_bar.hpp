#pragma once

#include "cpp_cache/format_measurement.hpp"
#include <cstddef>
#include <cstdint>
#include <ctime>
#include <iostream>
#include <optional>
#include <ostream>
#include <string>

using size_t = std::size_t;
using uint64_t = std::uint64_t;

/// @brief  A progress bar based on Python's TQDM library
class ProgressBar {
    /// @brief  Format time as "<min>:<sec>", e.g. "10:20".
    static inline std::string
    format_time_min_sec(double const tm_s)
    {
        uint64_t min = tm_s / 60;
        uint64_t sec = (uint64_t)tm_s % 60;
        return std::to_string(min) + ":" + std::string(sec < 10 ? 1 : 0, '0') +
               std::to_string(sec);
    }

    bool
    should_print() const
    {
        return counter_ % update_frequency_ == 0;
    }

    /// @brief  Format a percentage with padding on the front, e.g. " 10%".
    std::string
    percentage_done()
    {
        double percentage = (double)counter_ / size_ * 100;
        std::string p = std::to_string((int)percentage);
        return std::string(3 - p.size(), ' ') + p + "%";
    }

    void
    print_progress_bar()
    {
        if (!ostrm_) {
            return;
        }
        std::time_t curr_time = std::time(nullptr);
        double dur_s = std::difftime(curr_time, start_time_);
        size_t progress = (double)counter_ / size_ * granularity_;
        *ostrm_ << "\r" << percentage_done() << "|"
                << std::string(progress, '=')
                << std::string(granularity_ - progress, ' ') << "| "
                << format_underscore(counter_) << "/"
                << format_underscore(size_) << " ["
                << format_time_min_sec(dur_s) << "<?, "
                << (double)counter_ / dur_s << "it/s]";
        std::flush(std::cout);
    }

public:
    /// @param size - the total size in terms of tick increments.
    /// @param show - whether to show the progress bar.
    /// @param granularity - the granularity at which to show the progress bar
    ///                      ticks.
    ProgressBar(size_t const size,
                bool const show = true,
                size_t const granularity = 50)
        : size_(size),
          ostrm_(show ? &std::cout : nullptr),
          granularity_(granularity)
    {
        print_progress_bar();
        start_time_ = std::time(nullptr);
    }
    ProgressBar(size_t const size,
                std::ostream *const ostrm,
                size_t const granularity = 50)
        : size_(size),
          ostrm_(ostrm),
          granularity_(granularity)
    {
        print_progress_bar();
        start_time_ = std::time(nullptr);
    }

    void
    tick(size_t const increment = 1)
    {
        counter_ += increment;
        if (should_print()) {
            print_progress_bar();
        }

        // Create a new line after the counter when we're finished.
        if (counter_ >= size_) {
            print_progress_bar();
            std::cout << std::endl;
        }
    }

private:
    std::time_t start_time_;
    static constexpr size_t update_frequency_ = 1 << 20;
    size_t counter_ = 0;
    size_t const size_;
    std::ostream *const ostrm_;
    size_t const granularity_;
};
