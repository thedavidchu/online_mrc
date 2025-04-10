/// TODO
/// 1. Test with real trace (how to get TTLs?)
/// 2. How to count miscounts?
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <functional>
#include <future>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <thread>
#include <utility>
#include <vector>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_command.hpp"
#include "cpp_cache/cache_trace_format.hpp"
#include "cpp_cache/parse_measurement.hpp"
#include "cpp_cache/progress_bar.hpp"
#include "cpp_lib/util.hpp"
#include "lib/lifetime_cache.hpp"
#include "logger/logger.h"

#include "cpp_cache/cache_trace.hpp"
#include "lib/predictive_lru_ttl_cache.hpp"

using size_t = std::size_t;
using uint64_t = std::uint64_t;

static bool
run_single_cache(std::promise<std::string> ret,
                 int const id,
                 CacheAccessTrace &trace,
                 size_t const capacity_bytes,
                 double const lower_ratio,
                 double const upper_ratio,
                 LifeTimeCacheMode const lifetime_cache_mode)
{
    PredictiveCache p(capacity_bytes,
                      lower_ratio,
                      upper_ratio,
                      lifetime_cache_mode);
    std::stringstream ss;
    LOGGER_TIMING(
        "starting test_trace(trace: %s, cap: %zu, lt: %f, ut: %f, mode: %s)",
        trace.path().c_str(),
        capacity_bytes,
        lower_ratio,
        upper_ratio,
        LifeTimeCacheMode__str(lifetime_cache_mode));
    ProgressBar pbar{trace.size(), id == 0};
    p.start_simulation();
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto const &access = trace.get_wait(i);
        if (access.command == CacheCommand::Get) {
            p.access(access);
        }
    }
    p.end_simulation();
    LOGGER_TIMING(
        "finished test_trace(trace: %s, cap: %zu, lt: %f, ut: %f, mode: %s)",
        trace.path().c_str(),
        capacity_bytes,
        lower_ratio,
        upper_ratio,
        LifeTimeCacheMode__str(lifetime_cache_mode));
    p.print_statistics(ss);
    ret.set_value(ss.str());

    return true;
}

static void
run_caches(std::string const &path,
           CacheTraceFormat format,
           std::vector<uint64_t> const &capacity_bytes,
           double const lower_ratio,
           double const upper_ratio,
           LifeTimeCacheMode const lifetime_cache_mode)
{
    CacheAccessTrace trace{path, format, capacity_bytes.size()};
    std::vector<std::thread> workers;
    std::vector<std::future<std::string>> futs;

    int id = 0;
    for (auto c : capacity_bytes) {
        std::promise<std::string> promise;
        futs.push_back(promise.get_future());
        workers.emplace_back(run_single_cache,
                             std::move(promise),
                             id++,
                             std::ref(trace),
                             c,
                             lower_ratio,
                             upper_ratio,
                             lifetime_cache_mode);
    }
    for (auto &w : workers) {
        w.join();
    }
    int i = 0;
    for (auto &r : futs) {
        std::cout << "Run: " << path << " " << CacheTraceFormat__string(format)
                  << " " << lower_ratio << " " << upper_ratio << " "
                  << capacity_bytes[i++] << " "
                  << LifeTimeCacheMode__str(lifetime_cache_mode) << " "
                  << std::endl;
        std::cout << r.get();
    }
}

static std::vector<uint64_t>
parse_capacities(std::string const &str)
{
    std::vector<std::string> strs = string_split(str, " ");
    if (strs.size() > 10) {
        LOGGER_WARN("potentially too many sizes (%zu), may exceed DRAM",
                    strs.size());
    }
    std::vector<uint64_t> caps;
    for (auto s : strs) {
        caps.push_back(parse_memory_size(s).value());
    }
    std::cout << "Capacities [B]: " << vec2str(caps) << std::endl;
    return caps;
}

int
main(int argc, char *argv[])
{
    if (argc != 7) {
        std::cout
            << "Usage: predictor [<trace> <format> <lower_ratio 0.0-1.0> "
               "<upper_ratio 0.0-1.0> <cache-capacity>+ <lifetime_cache_mode "
               "EvictionTime|LifeTime>"
            << std::endl;
        exit(1);
    }

    std::string const path = argv[1];
    CacheTraceFormat const format = CacheTraceFormat__parse(argv[2]);
    double const lower_ratio = atof(argv[3]);
    double const upper_ratio = atof(argv[4]);
    // This will panic if it was unsuccessful.
    std::vector<uint64_t> capacity_bytes = parse_capacities(argv[5]);
    LifeTimeCacheMode const lifetime_cache_mode =
        LifeTimeCacheMode__parse(argv[6]);
    LOGGER_INFO("Running: %s %s",
                path.c_str(),
                CacheTraceFormat__string(format).c_str());
    run_caches(path,
               format,
               capacity_bytes,
               lower_ratio,
               upper_ratio,
               lifetime_cache_mode);
    std::cout << "OK!" << std::endl;
    return 0;
}
