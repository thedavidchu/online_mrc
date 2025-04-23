#pragma once

#include <cstdint>
#include <string>

using uint8_t = std::uint8_t;

enum class CacheCommand : uint8_t {
    Nop = 0,
    Get = 1,
    Gets = 2,
    Set = 3,
    Add = 4,
    Cas = 5,
    Replace = 6,
    Append = 7,
    Prepend = 8,
    Delete = 9,
    Incr = 10,
    Decr = 11,
    Read = 12,
    Write = 13,
    Update = 14,
    Invalid = 255
};

CacheCommand
CacheCommand__parse(char const *const str);

std::string const &
CacheCommand__string(CacheCommand const cmd);
