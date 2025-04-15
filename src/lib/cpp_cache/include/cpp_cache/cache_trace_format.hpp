#pragma once

#include <cstddef>
#include <string>

using size_t = std::size_t;

enum class CacheTraceFormat {
    Kia,
    Sari,
    YangTwitterX,
    Invalid,
};

CacheTraceFormat
CacheTraceFormat__parse(std::string const &format_str);

bool
CacheTraceFormat__valid(CacheTraceFormat const format);

std::string const &
CacheTraceFormat__string(CacheTraceFormat const format);

std::string
CacheTraceFormat__available(std::string const &sep = "|");

size_t
CacheTraceFormat__bytes_per_entry(CacheTraceFormat const format);
