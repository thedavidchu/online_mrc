/// TODO
/// 1. Test with real trace (how to get TTLs?)
/// 2. How to count miscounts?
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdlib.h>
#include <sys/types.h>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_command.hpp"
#include "cpp_cache/cache_trace_format.hpp"
#include "lib/lifetime_cache.hpp"
#include "logger/logger.h"

#include "cpp_cache/cache_trace.hpp"
#include "lib/iterator_spaces.hpp"
#include "lib/predictive_lru_ttl_cache.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

bool
test_lru()
{
    PredictiveCache p(2, 0.0, 1.0, LifeTimeCacheMode::EvictionTime);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 10},
                              CacheAccess{1, 1, 1, 10},
                              CacheAccess{2, 2, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    assert(p.get(2) == nullptr);

    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 2);
    assert(p.get(0) != nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) == nullptr);

    // Test third state.
    p.access(accesses[2]);
    assert(p.size() == 2);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    assert(p.get(2) != nullptr);

    return true;
}

bool
test_ttl()
{
    PredictiveCache p(2, 0.0, 1.0, LifeTimeCacheMode::EvictionTime);
    CacheAccess accesses[] = {CacheAccess{0, 0, 1, 1},
                              CacheAccess{1001, 1, 1, 10}};
    // Test initial state.
    assert(p.size() == 0);
    assert(p.get(0) == nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test first state.
    p.access(accesses[0]);
    assert(p.size() == 1);
    assert(p.get(0) != nullptr);
    assert(p.get(1) == nullptr);
    p.print();
    // Test second state.
    p.access(accesses[1]);
    assert(p.size() == 1);
    assert(p.get(0) == nullptr);
    assert(p.get(1) != nullptr);
    p.print();
    return true;
}

bool
test_trace(CacheAccessTrace const &trace,
           size_t const capacity_bytes,
           double const lower_ratio,
           double const upper_ratio,
           LifeTimeCacheMode const lifetime_cache_mode)
{
    PredictiveCache p(capacity_bytes,
                      lower_ratio,
                      upper_ratio,
                      lifetime_cache_mode);
    LOGGER_TIMING("starting test_trace()");
    p.start_simulation();
    for (size_t i = 0; i < trace.size(); ++i) {
        auto const &access = trace.get(i);
        if (access.command == CacheCommand::Get) {
            p.access(access);
        }
    }
    p.end_simulation();
    LOGGER_TIMING("finished test_trace()");
    p.print_statistics();

    return true;
}

int
main(int argc, char *argv[])
{
    switch (argc) {
    case 1:
        assert(test_lru());
        std::cout << "---\n";
        assert(test_ttl());
        std::cout << "OK!" << std::endl;
        break;
    case 6: {
        char const *const path = argv[1];
        char const *const format = argv[2];
        double const lower_ratio = atof(argv[3]);
        double const upper_ratio = atof(argv[4]);
        LifeTimeCacheMode const lifetime_cache_mode =
            LifeTimeCacheMode__parse(argv[5]);
        LOGGER_INFO("Running: %s %s", path, format);
        for (auto cap : semilogspace((size_t)16 << 30, 10)) {
            assert(test_trace(
                CacheAccessTrace(path, CacheTraceFormat__parse(format)),
                cap,
                lower_ratio,
                upper_ratio,
                lifetime_cache_mode));
        }
        std::cout << "OK!" << std::endl;
        break;
    }
    default:
        std::cout << "Usage: predictor [<trace> <format> <lower_ratio 0.0-1.0> "
                     "<upper_ratio 0.0-1.0> <lifetime_cache_mode "
                     "EvictionTime|LifeTime>"
                  << std::endl;
        exit(1);
    }
    return 0;
}
