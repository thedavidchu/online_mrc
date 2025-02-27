#include "list/lifetime_thresholds.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>
#include <vector>

bool
test_empty()
{
    LifeTimeThresholds t(0.25);
    std::pair<uint64_t, uint64_t> r;

    r = t.get_thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.get_thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.get_thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.get_thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());

    return true;
}

bool
test_simple()
{
    LifeTimeThresholds t(0.25);
    std::pair<uint64_t, uint64_t> r;

    for (uint64_t i = 1; i < 100; ++i) {
        t.register_cache_eviction(i, 1);
        t.refresh_thresholds();
        r = t.get_thresholds();
        std::cout << "> " << i << ": " << r.first << ", " << r.second
                  << std::endl;
    }

    return true;
}

int
main()
{
    bool r = 0;
    r = test_empty();
    assert(r);
    r = test_simple();
    assert(r);
    return 0;
}
