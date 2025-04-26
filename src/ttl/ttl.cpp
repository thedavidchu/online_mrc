/** @brief  This file creates MRCs based on the listed algorithms.
 */
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "cache/clock_cache.hpp"
#include "cache/fifo_cache.hpp"
#include "cache/lfu_cache.hpp"
#include "cache/lru_cache.hpp"
#include "cache/sieve_cache.hpp"
#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "logger/logger.h"
#include "modified_clock_cache.hpp"
#include "ttl_cache/new_ttl_clock_cache.hpp"
#include "ttl_cache/ttl_clock_cache.hpp"
#include "ttl_cache/ttl_fifo_cache.hpp"
#include "ttl_cache/ttl_lfu_cache.hpp"
#include "ttl_cache/ttl_lru_cache.hpp"
#include "ttl_cache/ttl_sieve_cache.hpp"

template <typename T>
static double
run_cache(CacheAccessTrace const &trace, uint64_t const capacity)
{
    LOGGER_TRACE("running '%s' algorithm for size %zu", T::name, capacity);
    T cache(capacity);

    for (size_t i = 0; i < trace.size(); ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, trace.size());
        }
        CacheAccess access = trace.get(i);
        if (!access.is_read()) {
            continue;
        }
        cache.access_item(access);
    }
    assert(cache.statistics_.total_ops() < trace.size());
    cache.statistics_.print(T::name, capacity);
    return cache.statistics_.miss_rate();
}

template <typename T>
static std::optional<std::map<std::uint64_t, double>>
generate_mrc(char const *const trace_path,
             CacheTraceFormat const format,
             std::vector<std::size_t> const &capacities)
{
    // This initializes everything to the default, i.e. 0.
    CacheAccessTrace trace = CacheAccessTrace{trace_path, format};
    std::map<std::size_t, double> mrc = {};

    if (trace_path == NULL) {
        LOGGER_ERROR("invalid input path", format);
        goto cleanup_error;
    }

    for (auto cap : capacities) {
        double mr = run_cache<T>(trace, cap);
        if (mr == -1.0) {
            LOGGER_ERROR("error in '%s' algorithm (N.B. name may be mangled)",
                         typeid(T).name());
            return std::nullopt;
        };
        mrc[cap] = mr;
    }
    return std::make_optional(mrc);
cleanup_error:
    return std::nullopt;
}

static int
print_mrc(std::string algorithm, std::map<std::uint64_t, double> mrc)
{
    std::cout << algorithm << std::endl;
    for (auto [sz, mr] : mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    return 0;
}

static int
save_mrc(std::filesystem::path path, std::map<std::uint64_t, double> mrc)
{
    std::ofstream fs;

    // Save results
    fs.open(path);
    if (!fs.is_open()) {
        return -1;
    }
    for (auto [sz, mr] : mrc) {
        fs << sz << "," << mr << std::endl;
    }
    fs.close();

    return 0;
}

int
main(int argc, char *argv[])
{
    std::filesystem::path trace_path =
        "/home/david/projects/online_mrc/data/src2.bin";
    std::string stem = trace_path.stem().string();

    std::map<std::string,
             std::function<std::optional<std::map<std::uint64_t, double>>(
                 char const *const trace_path,
                 CacheTraceFormat const format,
                 std::vector<std::size_t> const &capacities)>>
        algorithms =
            {
                {ClockCache::name, generate_mrc<ClockCache>},
                {"ModifiedClock", generate_modified_clock_mrc},
                {LRUCache::name, generate_mrc<LRUCache>},
                {LFUCache::name, generate_mrc<LFUCache>},
                {FIFOCache::name, generate_mrc<FIFOCache>},
                {SieveCache::name, generate_mrc<SieveCache>},

                {NewTTLClockCache::name, generate_mrc<NewTTLClockCache>},
                {TTLClockCache::name, generate_mrc<TTLClockCache>},
                {TTLLRUCache::name, generate_mrc<TTLLRUCache>},
                {TTLLFUCache::name, generate_mrc<TTLLFUCache>},
                {TTLFIFOCache::name, generate_mrc<TTLFIFOCache>},
                {TTLSieveCache::name, generate_mrc<TTLSieveCache>},
            },
        run_algorithms = {};

    std::cout << "Algorithms include: ";
    for (auto [name, fn] : algorithms) {
        std::cout << name << ", ";
    }
    std::cout << std::endl;
    for (int i = 0; i < argc && *argv != NULL; ++i, ++argv) {
        std::string arg = std::string(*argv);
        if (algorithms.count(arg)) {
            auto it = algorithms[arg];
            run_algorithms.emplace(arg, it);
        } else {
            std::cout << "Unrecognized argument " << i << ": " << arg
                      << std::endl;
        }
    }

    std::vector<std::size_t> sizes = {
        0,     1,     1000,  2000,  3000,  4000,  5000,
        6000,  7000,  8000,  9000,  10000, 20000, 30000,
        40000, 50000, 60000, 70000, 80000, 90000, 100000,
    };
    for (std::size_t i = 100000; i < 350000; i += 10000) {
        sizes.push_back(i);
    }
    std::map<std::string, std::map<std::uint64_t, double>> mrcs;

    for (auto [name, fn] : run_algorithms) {
        auto mrc = fn(trace_path.c_str(), CacheTraceFormat::Kia, sizes);
        if (mrc) {
            mrcs.emplace(name, mrc.value());
            save_mrc(stem + "-" + name + "-mrc.dat", mrc.value());
        }
    }

    for (auto [name, mrc] : mrcs) {
        print_mrc(name, mrc);
    }
    return 0;
}
