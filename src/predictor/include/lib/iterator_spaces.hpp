#pragma once

#include "cpp_lib/format_measurement.hpp"
#include "math/positive_ceiling_divide.h"
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <vector>

using size_t = std::size_t;

static inline std::vector<size_t>
linspace(size_t max, size_t num)
{
    std::vector<size_t> r;
    assert(max > num);
    for (size_t i = 0; i < num; ++i) {
        r.push_back(max / num * (i + 1));
    }
    return r;
}

static inline std::vector<size_t>
logspace(size_t max, size_t num)
{
    std::vector<size_t> r;
    assert(max > num);
    for (size_t i = 0; i < num; ++i) {
        r.push_back(max >> (num - 1 - i));
    }
    return r;
}

/// @brief  Space things by factor of sqrt(2).
static inline std::vector<size_t>
semilogspace(size_t max, size_t num)
{
    std::vector<size_t> r = logspace(max, POSITIVE_CEILING_DIVIDE(num, 2));
    std::reverse(r.begin(), r.end());
    std::vector<size_t> s;
    assert(max > num);
    for (size_t i = 0; i < num; ++i) {
        if (i % 2 == 0) {
            s.push_back(r[i / 2]);
        } else {
            s.push_back(r[i / 2] / std::sqrt(2));
        }
    }
    std::reverse(s.begin(), s.end());
    return s;
}

template <typename T>
static inline void
print_vector(std::vector<T> vector)
{
    std::cout << "{";
    for (auto x : vector) {
        std::cout << x << ", ";
    }
    std::cout << "}" << std::endl;
}
