#include <cassert>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <optional>
#include <sstream>

#include "cpp_cache/cache_access.hpp"
#include "cpp_cache/cache_command.hpp"
#include "cpp_cache/cache_trace_format.hpp"
#include "math/saturation_arithmetic.h"
#include "trace/trace.h"

/// @brief  Initialize a CacheAccess object from a FullTraceItem.
CacheAccess::CacheAccess(struct FullTraceItem const *const item)
    : timestamp_ms(item->timestamp_ms),
      command(item->command ? CacheCommand::Set : CacheCommand::Get),
      key(item->key),
      key_size_b(0),
      value_size_b(item->size),
      // A TTL of 0 in Kia's traces implies no TTL. I assume it's
      // the same in Sari's, but I don't know.
      ttl_ms(item->ttl_s == 0
                 ? std::nullopt
                 : std::optional(saturation_multiply(1000, item->ttl_s))),
      client_id(0)
{
}

CacheAccess::CacheAccess(std::uint64_t const timestamp_ms,
                         std::uint64_t const key)
    : timestamp_ms(timestamp_ms),
      command(CacheCommand::Get),
      key(key),
      key_size_b(0),
      value_size_b(1),
      ttl_ms(std::nullopt),
      client_id(0)
{
}

CacheAccess::CacheAccess(std::uint64_t const timestamp_ms,
                         std::uint64_t const key,
                         std::uint64_t size_bytes,
                         std::optional<std::uint64_t> ttl_ms)
    : timestamp_ms(timestamp_ms),
      command(CacheCommand::Get),
      key(key),
      key_size_b(0),
      value_size_b(size_bytes),
      ttl_ms(ttl_ms),
      client_id(0)
{
}

static uint64_t
read_le_u64(uint8_t const *const ptr)
{
    uint64_t r = 0;
    std::memcpy(&r, ptr, sizeof(r));
    return le64toh(r);
}

static uint32_t
read_le_u32(uint8_t const *const ptr)
{
    uint32_t r = 0;
    std::memcpy(&r, ptr, sizeof(r));
    return le32toh(r);
}

static uint8_t
read_le_u8(uint8_t const *const ptr)
{
    return *ptr;
}

static uint64_t
parse_timestamp_ms(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return read_le_u64(&record[0]);
    case CacheTraceFormat::Sari:
        return 1000 * (uint64_t)read_le_u32(&record[0]);
    case CacheTraceFormat::YangTwitter:
        return read_le_u32(&record[0]);
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

static uint64_t
parse_key(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return read_le_u64(&record[9]);
    case CacheTraceFormat::Sari:
        return read_le_u64(&record[4]);
    case CacheTraceFormat::YangTwitter:
        return read_le_u32(&record[4]);
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

static uint64_t
parse_key_size_b(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return 0;
    case CacheTraceFormat::Sari:
        return 0;
    case CacheTraceFormat::YangTwitter: {
        uint32_t kv_sz = read_le_u32(&record[12]);
        return kv_sz >> 22;
    }
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

static uint64_t
parse_value_size_b(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return read_le_u32(&record[17]);
    case CacheTraceFormat::Sari:
        return read_le_u32(&record[12]);
    case CacheTraceFormat::YangTwitter: {
        uint32_t kv_sz = read_le_u32(&record[12]);
        return kv_sz & 0x003FFFFF;
    }
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

static CacheCommand
parse_command(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return CacheCommand(read_le_u8(&record[8]) ? CacheCommand::Set
                                                   : CacheCommand::Get);
    case CacheTraceFormat::Sari:
        return CacheCommand::Get;
    case CacheTraceFormat::YangTwitter: {
        uint32_t op_ttl = read_le_u32(&record[16]);
        return CacheCommand(op_ttl >> 24);
    }
    case CacheTraceFormat::Invalid:
        return CacheCommand::Invalid;
    default:
        return CacheCommand::Invalid;
    }
}

static std::optional<uint64_t>
parse_ttl_ms(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia: {
        uint64_t ttl = read_le_u32(&record[21]);
        return (ttl == 0) ? std::nullopt : std::optional<uint64_t>{1000 * ttl};
    }
    case CacheTraceFormat::Sari: {
        uint64_t ttl = read_le_u32(&record[16]);
        return (ttl == 0) ? std::nullopt : std::optional<uint64_t>{1000 * ttl};
    }
    case CacheTraceFormat::YangTwitter: {
        uint32_t op_ttl = read_le_u32(&record[16]);
        uint64_t ttl = op_ttl & 0x00FFFFFF;
        return (ttl == 0) ? std::nullopt : std::optional<uint64_t>{1000 * ttl};
    }
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}
static uint64_t
parse_client_id(uint8_t const *const record, CacheTraceFormat const format)
{
    switch (format) {
    case CacheTraceFormat::Kia:
        return 0;
    case CacheTraceFormat::Sari:
        return 0;
    case CacheTraceFormat::YangTwitter:
        return read_le_u32(&record[20]);
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

CacheAccess::CacheAccess(uint8_t const *const record,
                         CacheTraceFormat const format)
    : timestamp_ms(parse_timestamp_ms(record, format)),
      command(parse_command(record, format)),
      key(parse_key(record, format)),
      key_size_b(parse_key_size_b(record, format)),
      value_size_b(parse_value_size_b(record, format)),
      ttl_ms(parse_ttl_ms(record, format)),
      client_id(parse_client_id(record, format))
{
}

std::optional<uint64_t>
CacheAccess::expiration_time_ms() const
{
    return saturation_add(timestamp_ms, ttl_ms.value_or(UINT64_MAX));
}

std::string
CacheAccess::twitter_csv(bool const newline) const
{
    std::stringstream ss;
    ss << timestamp_ms << ",";
    ss << key << ",";
    ss << key_size_b << ",";
    ss << value_size_b << ",";
    ss << client_id << ",";
    ss << CacheCommand__string(command) << ",";
    ss << ttl_ms.value_or(0);
    if (newline) {
        ss << std::endl;
    }
    return ss.str();
}
