/// TODO
/// 1. Test with real trace (how to get TTLs?)
/// 2. How to count miscounts?
/// 3. How to do certainty?
/// 4. This is really slow. Profile it to see how long the LRU stack is
///     taking...
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
#include "logger/logger.h"
#include "trace/reader.h"

#include "lib/util.hpp"

#include "lib/predictive_lru_ttl_cache.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

bool
test_lru()
{
    PredictiveCache p(2, 0.0, true, true);
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
    PredictiveCache p(2, 0.0, true, true);
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

#include "cpp_cache/cache_trace.hpp"
#include "cpp_cache/format_measurement.hpp"

bool
test_trace(CacheAccessTrace const &trace,
           size_t const capacity_bytes,
           double const uncertainty,
           bool record_reaccess,
           bool repredict_reaccess)
{
    PredictiveCache p(capacity_bytes,
                      uncertainty,
                      record_reaccess,
                      repredict_reaccess);
    LOGGER_TIMING("starting test_trace()");
    for (size_t i = 0; i < trace.size(); ++i) {
        p.access(trace.get(i));
    }
    LOGGER_TIMING("finished test_trace()");
    p.print_statistics();

    return true;
}

#include "lib/iterator_spaces.hpp"

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
        double const uncertainty = atof(argv[3]);
        bool const record_reaccess = atob_or_panic(argv[4]);
        bool const repredict_on_reaccess = atob_or_panic(argv[5]);
        LOGGER_INFO("Running: %s %s", path, format);
        for (auto cap : logspace((size_t)16 << 30, 10)) {
            assert(test_trace(
                CacheAccessTrace(path, parse_trace_format_string(format)),
                cap,
                uncertainty,
                record_reaccess,
                repredict_on_reaccess));
        }
        std::cout << "OK!" << std::endl;
        break;
    }
    default:
        std::cout << "Usage: predictor [<trace> <format> <uncertainty 0.0-0.5> "
                     "<record_reaccess true|false> <repredict true|false>]"
                  << std::endl;
        exit(1);
    }
    return 0;
}
