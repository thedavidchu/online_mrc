#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "clock_cache.hpp"
#include "fifo_cache.hpp"
#include "io/io.h"
#include "logger/logger.h"
#include "lru_cache.hpp"
#include "trace/reader.h"
#include "trace/trace.h"
#include "ttl_fifo_cache.hpp"
#include "ttl_lru_cache.hpp"

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
static std::optional<std::vector<std::pair<std::uint64_t, double>>>
generate_mrc(char const *const trace_path,
             enum TraceFormat const format,
             std::vector<std::size_t> const &capacities)
{
    // This initializes everything to the default, i.e. 0.
    struct MemoryMap mm = {};
    std::vector<std::pair<std::size_t, double>> mrc = {};

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
        mrc.push_back({cap, mr});
    }
    MemoryMap__destroy(&mm);

    // Sort the MRC just in case downstream relies on this!
    std::sort(mrc.begin(), mrc.end());
    return std::make_optional(mrc);
cleanup_error:
    MemoryMap__destroy(&mm);
    return std::nullopt;
}

static int
print_mrc(std::string algorithm,
          std::vector<std::pair<std::uint64_t, double>> mrc)
{
    std::cout << algorithm << std::endl;
    for (auto [sz, mr] : mrc) {
        std::cout << sz << "," << mr << std::endl;
    }
    return 0;
}

static int
save_mrc(std::filesystem::path path,
         std::vector<std::pair<std::uint64_t, double>> mrc)
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
    bool run_lru = false, run_clock = false, run_fifo = false;
    bool run_ttl_lru = false, run_ttl_fifo = false,
         run_ttl_modified_clock = false;
    std::cout << "Hello, World!" << std::endl;
    for (int i = 0; i < argc && *argv != NULL; ++i, ++argv) {
        std::string arg = std::string(*argv);
        std::cout << "Arg " << i << ": '" << arg << "'" << std::endl;
        if (arg == "fifo") {
            run_fifo = true;
        } else if (arg == "lru") {
            run_lru = true;
        } else if (arg == "clock") {
            run_clock = true;
        } else if (arg == "ttl-lru") {
            run_ttl_lru = true;
        } else if (arg == "ttl-fifo") {
            run_ttl_fifo = true;
        } else if (arg == "ttl-modified-clock") {
            run_ttl_modified_clock = true;
        }
    }

    std::vector<std::size_t> sizes = {
        1000,  2000,  3000,  4000,  5000,  6000,  7000,  8000,  9000,   10000,
        20000, 30000, 40000, 50000, 60000, 70000, 80000, 90000, 100000,
    };
    for (std::size_t i = 100000; i < 350000; i += 10000) {
        sizes.push_back(i);
    }
    std::vector<std::pair<std::uint64_t, double>> ttl_lru_mrc;
    std::vector<std::pair<std::uint64_t, double>> ttl_modified_clock_mrc;
    std::vector<std::pair<std::uint64_t, double>> ttl_fifo_mrc;
    std::vector<std::pair<std::uint64_t, double>> clock_mrc;
    std::vector<std::pair<std::uint64_t, double>> lru_mrc;
    std::vector<std::pair<std::uint64_t, double>> fifo_mrc;

    if (run_lru) {
        lru_mrc =
            generate_mrc<LRUCache>(trace_path.c_str(), TRACE_FORMAT_KIA, sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
        save_mrc(stem + "-lru-mrc.dat", lru_mrc);
    }
    if (run_ttl_lru) {
        ttl_lru_mrc =
            generate_mrc<TTLLRUCache>(trace_path.c_str(),
                                      TRACE_FORMAT_KIA,
                                      sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
        save_mrc(stem + "-ttl-lru-mrc.dat", ttl_lru_mrc);
    }
    if (run_ttl_modified_clock) {
        for (std::size_t sz = 1024; sz < 350000; sz += 4096) {
            double mr = run_ttl_modified_clock_cache(trace_path.c_str(),
                                                     TRACE_FORMAT_KIA,
                                                     sz);
            assert(mr != -1.0);
            ttl_modified_clock_mrc.push_back({sz, mr});
        }
        save_mrc(stem + "-ttl-modified-clock-mrc.dat", ttl_modified_clock_mrc);
    }
    if (run_ttl_fifo) {
        ttl_fifo_mrc =
            generate_mrc<TTLFIFOCache>(trace_path.c_str(),
                                       TRACE_FORMAT_KIA,
                                       sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
        save_mrc(stem + "-ttl-fifo-mrc.dat", ttl_fifo_mrc);
    }
    if (run_clock) {
        clock_mrc =
            generate_mrc<ClockCache>(trace_path.c_str(),
                                     TRACE_FORMAT_KIA,
                                     sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
        save_mrc(stem + "-clock-mrc.dat", clock_mrc);
    }
    if (run_fifo) {
        fifo_mrc =
            generate_mrc<FIFOCache>(trace_path.c_str(), TRACE_FORMAT_KIA, sizes)
                .value_or(std::vector<std::pair<std::uint64_t, double>>());
        save_mrc(stem + "-fifo-mrc.dat", fifo_mrc);
    }

    print_mrc(ClockCache::name, clock_mrc);
    print_mrc(FIFOCache::name, fifo_mrc);
    print_mrc(LRUCache::name, lru_mrc);
    print_mrc("TTL-LRU", ttl_lru_mrc);
    print_mrc("TTL-Modified-Clock", ttl_modified_clock_mrc);
    print_mrc("TTL-FIFO", ttl_fifo_mrc);

    return 0;
}
