/** @brief  Analyze the activity per client.
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
#include <stdbool.h>
#include <unordered_map>

using size_t = std::size_t;
using uint64_t = std::uint64_t;
using uint32_t = std::uint32_t;
using uint16_t = std::uint16_t;

struct Data {
    uint16_t client_id = UINT16_MAX;
    uint16_t switched_clients = 0;
    uint16_t remained_with_client = 0;
};

template <typename T>
void
saturation_iincr(T &x)
{
    if (x == std::numeric_limits<T>::max()) {
        return;
    }
    x += 1;
}

void
analyze_clients(char const *const trace_path,
                CacheTraceFormat const format,
                bool const show_progress,
                bool const verbose = false)
{
    std::unordered_map<uint64_t, Data> map;
    Histogram client_read, client_write;
    CacheAccessTrace const trace(trace_path, format);
    ProgressBar pbar{trace.size(), show_progress};
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto &x = trace.get(i);
        if (map[x.key].client_id == UINT16_MAX) {
            map[x.key].client_id = x.client_id;
        } else if (map[x.key].client_id != x.client_id) {
            map[x.key].client_id = x.client_id;
            saturation_iincr(map[x.key].switched_clients);
        } else {
            saturation_iincr(map[x.key].remained_with_client);
        }

        if (x.is_read()) {
            client_read.update(x.client_id);
        } else if (x.is_write()) {
            client_write.update(x.client_id);
        }
    }

    Histogram switched, stayed, final_client_popularity;
    for (auto [k, d] : map) {
        final_client_popularity.update(d.client_id);
        switched.update(d.switched_clients);
        stayed.update(d.remained_with_client);
    }
    std::cout << "Final Client Popularity" << std::endl;
    std::cout << final_client_popularity.csv();
    std::cout << "Stayed with Client" << std::endl;
    std::cout << stayed.csv();
    std::cout << "Switched Client" << std::endl;
    std::cout << switched.csv();
    std::cout << "Reads per Client" << std::endl;
    std::cout << client_read.csv();
    std::cout << "Writes per client" << std::endl;
    std::cout << client_write.csv();
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
    analyze_clients(trace_path, format, show_progress);
    return 0;
}
