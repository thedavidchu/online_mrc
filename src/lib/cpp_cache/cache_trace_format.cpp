#include "cpp_cache/cache_trace_format.hpp"
#include <cstddef>
#include <cstring>
#include <map>
#include <sstream>
#include <string>

using size_t = std::size_t;

// This must fit within an integer and be larger than all other values.
static size_t const INVALID_ID = 3;
static std::map<size_t, std::string> CACHE_TRACE_FORMAT_STRINGS = {
    {0, "Kia"},
    {1, "Sari"},
    {2, "YangWithClient"},
    {INVALID_ID, "Invalid"},
};

CacheTraceFormat
CacheTraceFormat__parse(std::string const &format_str)
{
    for (auto [id, s] : CACHE_TRACE_FORMAT_STRINGS) {
        if (s == format_str) {
            return CacheTraceFormat(id);
        }
    }
    return CacheTraceFormat::Invalid;
}

bool
CacheTraceFormat__valid(CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return true;
    case CacheTraceFormat::Sari:
        return true;
    case CacheTraceFormat::YangTwitter:
        return true;
    case CacheTraceFormat::Invalid:
        return false;
    default:
        return false;
    }
}

std::string const &
CacheTraceFormat__string(CacheTraceFormat const format)
{
    if (CACHE_TRACE_FORMAT_STRINGS.count((size_t)format)) {
        return CACHE_TRACE_FORMAT_STRINGS.at((size_t)format);
    }
    return CACHE_TRACE_FORMAT_STRINGS.at(3);
}

std::string
CacheTraceFormat__available(std::string const &sep)
{
    std::stringstream ss;
    for (auto [i, s] : CACHE_TRACE_FORMAT_STRINGS) {
        ss << s;
        if (i != INVALID_ID) {
            ss << sep;
        }
    }
    return ss.str();
}

size_t
CacheTraceFormat__bytes_per_entry(CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return 25;
    case CacheTraceFormat::Sari:
        return 20;
    case CacheTraceFormat::YangTwitter:
        return 24;
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}
