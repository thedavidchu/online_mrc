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
#include <memory>
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
           std::shared_ptr<std::ostream> progress_strm)
{
    Histogram histogram{};
    CacheAccessTrace const trace{trace_path, format};
    ProgressBar pbar{trace.size(), progress_strm};
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);
        if (x.has_ttl()) {
            histogram.update(x.ttl_ms);
        }
    }
    std::cout << histogram.csv();
}

int
main(int argc, char *argv[])
{
    if (argc != 3 && argc != 4) {
        std::cout << "Usage: " << argv[0]
                  << " <trace-path> <format> [<progress-stream>=nullptr]"
                  << std::endl;
        return EXIT_FAILURE;
    }
    char const *const trace_path = argv[1];
    CacheTraceFormat format = CacheTraceFormat__parse(argv[2]);
    std::shared_ptr<std::ostream> const progress_strm =
        (argc == 4) ? str2stream(argv[3]) : nullptr;
    assert(CacheTraceFormat__valid(format));
    count_ttls(trace_path, format, progress_strm);
    return 0;
}
