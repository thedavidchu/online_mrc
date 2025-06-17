#include "cpp_lib/util.hpp"
#include "cpp_lib/parse_measurement.hpp"
#include "logger/logger.h"
#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>
#include <cassert>
#include <cstring>
#include <string>
#include <vector>

bool
atob_or_panic(char const *const a)
{
    if (strcmp(a, "true") == 0) {
        return true;
    }
    if (strcmp(a, "false") == 0) {
        return false;
    }
    assert(0 && "unexpected 'true' or 'false'");
}

/// @brief  Split a string at any point where a delimiter is found.
std::vector<std::string>
string_split(std::string const &src, std::string const &delim)
{
    std::vector<std::string> r;
    boost::algorithm::split(r, src, boost::is_any_of(delim));
    return r;
}

std::vector<uint64_t>
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
    return caps;
}
