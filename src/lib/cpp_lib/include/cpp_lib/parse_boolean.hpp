#pragma once

#include <string>

/// @note   TIL you can implicitly convert a 'char *' to either
///         'std::string const&' or 'std::string' but not 'std::string &'.
static inline bool
parse_bool_or(std::string const &str, bool const alt)
{
    if (str == "true") {
        return true;
    }
    if (str == "false") {
        return false;
    }
    return alt;
}
