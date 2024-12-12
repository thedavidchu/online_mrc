#include <algorithm>
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

#include "clock_cache.hpp"
#include "fifo_cache.hpp"
#include "io/io.h"
#include "lfu_cache.hpp"
#include "logger/logger.h"
#include "lru_cache.hpp"
#include "modified_clock_cache.hpp"
#include "sieve_cache.hpp"
#include "trace/reader.h"
#include "trace/trace.h"
#include "ttl_clock_cache.hpp"
#include "ttl_fifo_cache.hpp"
#include "ttl_lfu_cache.hpp"
#include "ttl_lru_cache.hpp"
#include "ttl_sieve_cache.hpp"

template <typename T>
static double
run_cache(struct MemoryMap const *const mm,
          enum TraceFormat format,
          uint64_t const capacity)
{
    LOGGER_TRACE("running '%s' algorithm for size %zu", T::name, capacity);
    size_t num_entries = 0;
    size_t bytes_per_trace_item = 0;
    T cache(capacity);

    if (mm == NULL) {
        LOGGER_ERROR("invalid input");
        return -1.0;
    }
    // NOTE I let this helper function handle the format checks for me
    //      so that all logic dealing with parsing trace formats is
    //      contained within this library.
    bytes_per_trace_item = get_bytes_per_trace_item(format);
    if (bytes_per_trace_item == 0) {
        LOGGER_ERROR("invalid trace format");
        return -1.0;
    }
    num_entries = mm->num_bytes / bytes_per_trace_item;
    for (size_t i = 0; i < num_entries; ++i) {
        if (i % 1000000 == 0) {
            LOGGER_TRACE("Finished %zu / %zu", i, num_entries);
        }
        struct FullTraceItemResult r = construct_full_trace_item(
            &((uint8_t *)mm->buffer)[i * bytes_per_trace_item],
            format);
        assert(r.valid);

        // Skip PUT requests.
        if (r.item.command == 1) {
            continue;
        }

        cache.access_item(r.item.key);
    }
    assert(cache.statistics_.total_accesses_ < num_entries);
    cache.statistics_.print(T::name, capacity);
    return cache.statistics_.miss_rate();
}

template <typename T>
static std::optional<std::map<std::uint64_t, double>>
generate_mrc(char const *const trace_path,
             enum TraceFormat const format,
             std::vector<std::size_t> const &capacities)
{
    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    std::map<std::size_t, double> mrc = {};

    if (trace_path == NULL) {
        LOGGER_ERROR("invalid input path", format);
        goto cleanup_error;
    }

    // Memory map the input trace file
    if (!MemoryMap__init(&mm, trace_path, "rb")) {
        LOGGER_ERROR("failed to mmap '%s'", trace_path);
        goto cleanup_error;
    }
    for (auto cap : capacities) {
        double mr = run_cache<T>(&mm, format, cap);
        if (mr == -1.0) {
            LOGGER_ERROR("error in '%s' algorithm (N.B. name may be mangled)",
                         typeid(T).name());
            return std::nullopt;
        };
        mrc[cap] = mr;
    }
    MemoryMap__destroy(&mm);
    return std::make_optional(mrc);
cleanup_error:
    MemoryMap__destroy(&mm);
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
                 enum TraceFormat const format,
                 std::vector<std::size_t> const &capacities)>>
        algorithms =
            {
                {ClockCache::name, generate_mrc<ClockCache>},
                {"ModifiedClock", generate_modified_clock_mrc},
                {LRUCache::name, generate_mrc<LRUCache>},
                {LFUCache::name, generate_mrc<LFUCache>},
                {FIFOCache::name, generate_mrc<FIFOCache>},
                {SieveCache::name, generate_mrc<SieveCache>},

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
        auto mrc = fn(trace_path.c_str(), TRACE_FORMAT_KIA, sizes);
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
