#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_trace.hpp"
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
        auto x = trace.get(i);
        if (!sampler.sample(x.key)) {
            continue;
        }
        // TODO The implementation for z is opaque to the program but more
        //      efficient.
        auto y = x.binary(format);
        auto z = trace.get_raw(i);
        assert(y == z);
        output.insert(output.end(), z.begin(), z.end());
    }

    std::ofstream of{opath, std::ios::out | std::ios::binary};
    of.write(reinterpret_cast<const char *>(output.data()), output.size());

    return 0;
}
