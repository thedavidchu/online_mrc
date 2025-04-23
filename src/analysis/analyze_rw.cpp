/** @brief  Analyze the sets without gets or get-miss without set.
 */

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
#include <limits>
#include <stdbool.h>
#include <sys/types.h>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint16_t = std::uint16_t;
using uint8_t = std::uint8_t;

struct Data {
    bool read : 1 = 0;
    bool write : 1 = 0;
    // Check whether there was a read miss without write
    bool read_miss_without_write : 1 = 0;
    bool write_before_read : 1 = 0;
};

void
filter_gets_before_sets(char const *const trace_path,
                        CacheTraceFormat const format,
                        bool const show_progress)
{
    std::unordered_map<uint64_t, Data> map;
    CacheAccessTrace const trace(trace_path, format);
    ProgressBar pbar{trace.size(), show_progress};
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);
        auto &d = map[x.key];
        if (x.is_read()) {
            d.read = 1;
            if (x.value_size_b == 0) {
                d.read_miss_without_write = 1;
            }
        } else if (x.is_write()) {
            // Since there was a write, reset the read-miss-without-write bit.
            d.read_miss_without_write = 0;
            d.write = 1;
            if (d.read == 0) {
                d.write_before_read = 1;
            }
        }
    }

    uint64_t read_only = 0, write_only = 0, rw = 0, read_miss_without_write = 0,
             write_before_read = 0;
    for (auto [k, d] : map) {
        read_only += d.read && !d.write;
        write_only += !d.read && d.write;
        rw += d.read && d.write;
        read_miss_without_write += d.read_miss_without_write;
        write_before_read += d.write_before_read;
    }

    std::cout << "Total Keys: " << map.size() << std::endl;
    std::cout << "Read-only Keys: " << read_only << std::endl;
    std::cout << "Write-only Keys: " << write_only << std::endl;
    std::cout << "Read-Write Keys: " << rw << std::endl;
    std::cout << "Read-Miss without Write Keys: " << read_miss_without_write
              << std::endl;
    std::cout << "Writes Before Read Keys: " << write_before_read << std::endl;
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
                  << " <trace-path> <format> [<show_progress>=true]"
                  << std::endl;
        return EXIT_FAILURE;
    }
    char const *const trace_path = argv[1];
    CacheTraceFormat format = CacheTraceFormat__parse(argv[2]);
    bool const show_progress = (argc == 4) ? parse_bool(argv[3]) : true;
    assert(CacheTraceFormat__valid(format));
    filter_gets_before_sets(trace_path, format, show_progress);
    return 0;
}
