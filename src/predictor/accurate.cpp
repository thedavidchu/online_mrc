#include "accurate/lfu_ttl_cache.hpp"
#include "accurate/redis_ttl.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/cache_trace_format.hpp"
#include "cpp_lib/progress_bar.hpp"
#include "cpp_lib/util.hpp"
#include "shards/fixed_rate_shards_sampler.h"
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <future>
#include <iostream>
#include <ostream>
#include <string>
#include <thread>
#include <vector>

enum class Policy { LRU, LFU, Redis, Memcached, CacheLib };

/// @param message: error message (without newline).
static inline void
hard_assert(bool const condition, std::string const &message)
{
    if (!condition) {
        std::cout << message << std::endl;
        exit(EXIT_FAILURE);
    }
}

struct CommandLineArguments {
private:
    static void
    print_usage(std::string exe)
    {
        std::cout << "> Usage: " << exe
                  << " <input-path> <format Sari|Kia> <policy "
                     "LRU|LFU|Redis|Memcached|CacheLib> "
                     "<capacities \"1KiB 2KiB\"> <shards_ratio (0.0,1.0]>"
                  << std::endl;
    }

public:
    CommandLineArguments(int const argc, char const *const argv[])
    {
        if (argc != 1 + 5) {
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }

        input_path = argv[1];
        trace_format = CacheTraceFormat__parse(argv[2]);
        if (!CacheTraceFormat__valid(trace_format)) {
            std::cout << "Bad format: " << argv[2] << std::endl;
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        std::unordered_map<std::string, Policy> policies{
            {"LRU", Policy::LRU},
            {"LFU", Policy::LFU},
            {"Redis", Policy::Redis},
            {"Memcached", Policy::Memcached},
            {"CacheLib", Policy::CacheLib},
        };
        if (!policies.contains(argv[3])) {
            std::cout << "Invalid policy: " << argv[3] << std::endl;
            print_usage(argv[0]);
            exit(EXIT_FAILURE);
        }
        policy = policies.at(argv[3]);
        cache_capacities = parse_capacities(argv[4]);
        shards_ratio = atof(argv[5]);
        assert(0 < shards_ratio && shards_ratio <= 1.0);
    }

    std::string input_path;
    CacheTraceFormat trace_format;
    Policy policy;
    std::vector<uint64_t> cache_capacities;
    double shards_ratio;
    // I hard-code this as false because I have everything print at the end so
    // it doesn't make sense to include progress bars if they're not printed in
    // real time.
    bool const show_progress = false;
};

template <class T>
bool
run_single_accurate_cache(std::promise<std::string> promise,
                          uint64_t id,
                          CacheAccessTrace &trace,
                          uint64_t const capacity_bytes,
                          double const shards_ratio,
                          bool const show_progress)
{
    T cache{(uint64_t)(capacity_bytes * shards_ratio)};
    FixedRateShardsSampler sampler{shards_ratio, true};
    std::stringstream ss;
    LOGGER_TIMING("starting test_trace(trace: %s, nominal cap: %zu, sampled "
                  "cap: %zu, shards: %f)",
                  trace.path().c_str(),
                  capacity_bytes,
                  (size_t)(capacity_bytes * shards_ratio),
                  shards_ratio);
    ProgressBar pbar{trace.size(), show_progress && id == 0};
    cache.start_simulation();
    for (size_t i = 0; i < trace.size(); ++i) {
        pbar.tick();
        auto const &access = trace.get_wait(i);
        if (!sampler.sample(access.key)) {
            continue;
        }
        if (access.is_read()) {
            cache.access(access);
        }
    }
    cache.end_simulation();
    LOGGER_TIMING("finished test_trace(trace: %s, cap: %zu, shards: %f)",
                  trace.path().c_str(),
                  capacity_bytes,
                  shards_ratio);
    ss << cache.json({{"SHARDS", sampler.json(false)}});
    promise.set_value(ss.str());
    return true;
}

void
run_cache(CommandLineArguments const &args)
{
    CacheAccessTrace trace{args.input_path,
                           args.trace_format,
                           args.cache_capacities.size()};
    std::vector<std::thread> workers;
    std::vector<std::future<std::string>> futs;

    int id = 0;
    for (auto c : args.cache_capacities) {
        std::promise<std::string> promise;
        futs.push_back(promise.get_future());
        switch (args.policy) {
        case Policy::LRU:
            hard_assert(false, "unimplemented");
            break;
        case Policy::LFU:
            workers.emplace_back(run_single_accurate_cache<LFU_TTL_Cache>,
                                 std::move(promise),
                                 id++,
                                 std::ref(trace),
                                 c,
                                 args.shards_ratio,
                                 false);
            break;
        case Policy::Redis:
            workers.emplace_back(run_single_accurate_cache<RedisTTL>,
                                 std::move(promise),
                                 id++,
                                 std::ref(trace),
                                 c,
                                 args.shards_ratio,
                                 false);
            break;
        case Policy::Memcached:
            hard_assert(false, "unimplemented");
            break;
        case Policy::CacheLib:
            hard_assert(false, "unimplemented");
            break;
        default:
            hard_assert(false, "unrecognized policy");
            break;
        }
    }
    for (auto &w : workers) {
        w.join();
    }
    int i = 0;
    for (auto &r : futs) {
        std::cout << "Run: " << args.input_path << " "
                  << CacheTraceFormat__string(args.trace_format) << " "
                  << args.cache_capacities[i++] << " " << std::endl;
        std::cout << "> " << r.get() << std::endl;
    }
}

int
main(int argc, char *argv[])
{
    CommandLineArguments args{argc, argv};
    run_cache(args);
    return 0;
}
