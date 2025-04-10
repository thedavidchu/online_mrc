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
#include "cpp_cache/parse_measurement.hpp"
#include "lib/lifetime_cache.hpp"
#include "logger/logger.h"

#include "cpp_cache/cache_trace.hpp"
#include "lib/iterator_spaces.hpp"
#include "lib/predictive_lru_ttl_cache.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;


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
    case 7: {
        char const *const path = argv[1];
        char const *const format = argv[2];
        double const lower_ratio = atof(argv[3]);
        double const upper_ratio = atof(argv[4]);
        // This will panic if it was unsuccessful.
        uint64_t const cache_capacity = parse_memory_size(argv[5]).value();
        LifeTimeCacheMode const lifetime_cache_mode =
            LifeTimeCacheMode__parse(argv[6]);
        LOGGER_INFO("Running: %s %s", path, format);
        test_trace(CacheAccessTrace(path, CacheTraceFormat__parse(format)),
                   cache_capacity,
                   lower_ratio,
                   upper_ratio,
                   lifetime_cache_mode);
        std::cout << "OK!" << std::endl;
        break;
    }
    default:
        std::cout
            << "Usage: predictor [<trace> <format> <lower_ratio 0.0-1.0> "
               "<upper_ratio 0.0-1.0> <cache-capacity> <lifetime_cache_mode "
               "EvictionTime|LifeTime>"
            << std::endl;
        exit(1);
    }
    return 0;
}
