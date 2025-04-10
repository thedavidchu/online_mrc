
#include "cpp_lib/util.hpp"
#include "arrays/is_last.h"

#include <cassert>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

#include <boost/algorithm/string/classification.hpp>
#include <boost/algorithm/string/split.hpp>

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
