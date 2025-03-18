#include "lib/lifetime_thresholds.hpp"

#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <utility>

template <typename A, typename B>
static void
print_pair(std::pair<A, B> const &p)
{
    std::cout << "{" << p.first << "," << p.second << "}" << std::endl;
}

static bool
test_empty()
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    r = t.thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());
    r = t.thresholds();
    assert(r.first == 0 && r.second == std::numeric_limits<uint64_t>::max());

    return true;
}

static bool
test_thresholds(bool const debug = false)
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    for (uint64_t i = 1; i < 100; ++i) {
        t.register_cache_eviction(i, 1);
        t.refresh_thresholds();
        r = t.thresholds();
        if (debug) {
            std::cout << "> " << i << ": " << r.first << ", " << r.second
                      << std::endl;
        }
    }

    return true;
}

static bool
test_refresh(bool const debug = false)
{
    LifeTimeThresholds t(0.25, 0.75);
    std::pair<uint64_t, uint64_t> r;

    // Sample from 1..=100 and make sure the thresholds change at the
    // expected times.
    for (size_t i = 0; i < t.MIN_COARSE_COUNT - 1; ++i) {
        t.register_cache_eviction(i % 100 + 1, 1);
        r = t.thresholds();
        assert(r.first == 0 &&
               r.second == std::numeric_limits<uint64_t>::max());
    }
    // It is only on the millionth access that we expect the thresholds
    // to change.
    t.register_cache_eviction(0, 1);
    r = t.thresholds();
    if (debug) {
        print_pair(r);
    }
    assert(r.first == 24 && r.second == 75);

    // Sample from a different population; the thresholds won't change
    // until expected.
    for (size_t i = 0; i < t.MIN_COARSE_COUNT - 1; ++i) {
        t.register_cache_eviction(i % 101 + 100, 1);
        r = t.thresholds();
        assert(r.first == 24 && r.second == 75);
    }

    // It is only now that the thresholds change.
    t.register_cache_eviction(0, 1);
    r = t.thresholds();
    if (debug) {
        print_pair(r);
    }
    assert(r.first == 49 && r.second == 150);

    return true;
}

int
main()
{
    bool r = 0;
    r = test_empty();
    assert(r);
    r = test_thresholds(false);
    assert(r);
    r = test_refresh(false);
    assert(r);
    return 0;
}
