#include "lib/lru_ttl_cache.hpp"
#include "cpp_cache/cache_trace.hpp"
#include "cpp_cache/cache_trace_format.hpp"
#include "cpp_cache/format_measurement.hpp"

bool
test_trace(CacheAccessTrace const &trace, size_t const capacity_bytes)
{
    LRUTTLCache p(capacity_bytes);
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
    if (argc == 3) {
        char const *const path = argv[1];
        char const *const format = argv[2];
        LOGGER_INFO("Running: %s %s", path, format);
        for (auto cap : logspace((size_t)16 << 30, 10)) {
            assert(test_trace(
                CacheAccessTrace(path, CacheTraceFormat__parse(format)),
                cap));
        }
        std::cout << "OK!" << std::endl;
    } else {
        std::cout << "Usage: predictor [<trace> <format>]" << std::endl;
        exit(1);
    }
    return 0;
}
