#include "cpp_lib/cache_command.hpp"
#include <cstddef>
#include <map>
#include <string>

using size_t = std::size_t;

static std::map<size_t, std::string> const CACHE_COMMAND_STRINGS = {
    {0, "nop"},
    {1, "get"},
    {2, "gets"},
    {3, "set"},
    {4, "add"},
    {5, "cas"},
    {6, "replace"},
    {7, "append"},
    {8, "prepend"},
    {9, "delete"},
    {10, "incr"},
    {11, "decr"},
    {12, "read"},
    {13, "write"},
    {14, "update"},
    {254, "getset"},
    {255, "invalid"},
};

CacheCommand
CacheCommand__parse(char const *const str)
{
    for (auto [cmd, s] : CACHE_COMMAND_STRINGS) {
        if (s == std::string(str)) {
            return CacheCommand(cmd);
        }
    }
    return CacheCommand::Invalid;
}

std::string const &
CacheCommand__string(CacheCommand const cmd)
{
    if (CACHE_COMMAND_STRINGS.count((size_t)cmd)) {
        return CACHE_COMMAND_STRINGS.at((size_t)cmd);
    }
    return CACHE_COMMAND_STRINGS.at(255);
}

bool
CacheCommand__is_any_read(CacheCommand const cmd)
{
    return cmd == CacheCommand::Get || cmd == CacheCommand::Gets ||
           cmd == CacheCommand::Read || cmd == CacheCommand::GetSet;
}

bool
CacheCommand__is_any_write(CacheCommand const cmd)
{
    return (CacheCommand::Set <= cmd && cmd <= CacheCommand::Decr) ||
           cmd == CacheCommand::Update || cmd == CacheCommand::GetSet;
}
