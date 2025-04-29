/** @brief  Create a histogram of TTLs in write accesses. */

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "cpp_lib/histogram.hpp"
#include "cpp_lib/progress_bar.hpp"

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <stdbool.h>
#include <sys/types.h>

using size_t = std::size_t;
using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint16_t = std::uint16_t;
using uint8_t = std::uint8_t;

void
count_ttls(char const *const trace_path,
           CacheTraceFormat const format,
           bool const show_progress)
{
    Histogram histogram{};
    CacheAccessTrace const trace{trace_path, format};
    ProgressBar pbar{trace.size(), show_progress};
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);
        if (x.has_ttl()) {
            histogram.update(x.ttl_ms);
        }
    }
    std::cout << histogram.csv();
}

bool
parse_bool(char const *str)
{
    if (std::strcmp(str, "true") == 0) {
        return true;
    }
    if (std::strcmp(str, "false") == 0) {
        return true;
    }
    assert(0 && "unrecognized bool parameter");
}

int
main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: " << argv[0]
                  << " <trace-path> <format> [<show_progress>=false]"
                  << std::endl;
        return EXIT_FAILURE;
    }
    char const *const trace_path = argv[1];
    CacheTraceFormat format = CacheTraceFormat__parse(argv[2]);
    bool const show_progress = (argc == 4) ? parse_bool(argv[3]) : false;
    assert(CacheTraceFormat__valid(format));
    count_ttls(trace_path, format, show_progress);
    return 0;
}
