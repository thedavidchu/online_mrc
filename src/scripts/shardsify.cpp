#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_trace.hpp"
#include "cpp_lib/util.hpp"
#include "shards/fixed_rate_shards_sampler.h"
#include <cassert>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <functional>
#include <iomanip>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

std::string
help_message(int argc, char **argv)
{
    assert(argc >= 1);
    std::stringstream ss;
    ss << "usage: " << std::string(argv[0])
       << " <input-path> <format> <shards-ratio> <output-path>" << std::endl;
    return ss.str();
}

static std::string
bytevec2str(std::vector<std::byte> const &v)
{
    std::function<std::string(std::byte const &val)> byte2str =
        [](std::byte const &val) -> std::string {
        std::stringstream ss;
        // Printing a 'char' or 'uint8_t' prints the ASCII code rather
        // than a number.
        ss << std::hex << std::setw(2) << std::setfill('0')
           << static_cast<int>(val);
        return ss.str();
    };
    return vec2str(v, byte2str);
}

int
main(int argc, char *argv[])
{
    if (argc != 4 + 1) {
        std::cerr << help_message(argc, argv);
        exit(EXIT_FAILURE);
    }
    std::string ipath = argv[1], opath = argv[4];
    CacheTraceFormat const format{CacheTraceFormat__parse(argv[2])};
    assert(format != CacheTraceFormat::Invalid);
    double const shards_ratio{atof(argv[3])};
    FixedRateShardsSampler sampler{shards_ratio, true};
    CacheAccessTrace trace{ipath, format};
    std::vector<std::byte> output;

    for (size_t i = 0; i < trace.size(); ++i) {
        auto access = trace.get(i);
        if (!sampler.sample(access.key)) {
            continue;
        }
        // TODO The implementation for z is opaque to the program but more
        //      efficient.
        if (false) {
            std::cout << access.ttl_ms << std::endl;
        }
        auto a_bin = access.binary(format);
        auto t_bin = trace.get_raw(i);
        if (false) {
            std::cout << bytevec2str(a_bin) << std::endl;
            std::cout << bytevec2str(t_bin) << std::endl;
        }
        assert(a_bin == t_bin);
        output.insert(output.end(), t_bin.begin(), t_bin.end());
    }

    // Writing everything at the end allows us to cancel the operations
    // with human reaction times. However, it requires copying and
    // storing lots of data in memory.
    std::ofstream of{opath, std::ios::out | std::ios::binary};
    of.write(reinterpret_cast<const char *>(output.data()), output.size());

    return 0;
}
