#pragma once

#include <cassert>
#include <cstddef>
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
