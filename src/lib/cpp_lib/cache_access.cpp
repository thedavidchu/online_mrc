#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <endian.h>
#include <sstream>

#include "cpp_lib/cache_access.hpp"
#include "cpp_lib/cache_command.hpp"
#include "cpp_lib/cache_trace_format.hpp"

CacheAccess::CacheAccess(std::uint64_t const timestamp_ms,
                         std::uint64_t const key,
                         std::uint64_t size_bytes,
                         double const ttl_ms)
    : timestamp_ms(timestamp_ms),
      command(CacheCommand::GetSet),
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
    // Align the bytes.
    std::memcpy(&r, ptr, sizeof(r));
    return le64toh(r);
}

static uint32_t
read_le_u32(uint8_t const *const ptr)
{
    uint32_t r = 0;
    // Align the bytes.
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
    case CacheTraceFormat::YangTwitterX:
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
    case CacheTraceFormat::YangTwitterX:
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
    case CacheTraceFormat::YangTwitterX: {
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
    case CacheTraceFormat::YangTwitterX: {
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
        return CacheCommand::GetSet;
    case CacheTraceFormat::YangTwitterX: {
        uint32_t op_ttl = read_le_u32(&record[16]);
        return CacheCommand(op_ttl >> 24);
    }
    case CacheTraceFormat::Invalid:
        return CacheCommand::Invalid;
    default:
        return CacheCommand::Invalid;
    }
}

static double
parse_ttl_ms(uint8_t const *const record, CacheTraceFormat const format)
{
    double const ttl_ms =
        CacheCommand__is_any_write(parse_command(record, format)) ? INFINITY
                                                                  : NAN;
    switch (format) {
    case CacheTraceFormat::Kia: {
        uint64_t ttl = read_le_u32(&record[21]);
        return (ttl == 0) ? ttl_ms : 1000 * ttl;
    }
    case CacheTraceFormat::Sari: {
        uint64_t ttl = read_le_u32(&record[16]);
        // Sari processed his trace to assign TTLs to every GET and
        // filter out all other accesses. This means that a TTL of 0
        // corresponds to an item who should live in the cache
        // indefinitely.
        return (ttl == 0) ? INFINITY : 1000 * ttl;
    }
    case CacheTraceFormat::YangTwitterX: {
        uint32_t op_ttl = read_le_u32(&record[16]);
        uint64_t ttl = op_ttl & 0x00FFFFFF;
        return (ttl == 0) ? ttl_ms : 1000 * ttl;
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
    case CacheTraceFormat::YangTwitterX:
        return read_le_u32(&record[20]);
    case CacheTraceFormat::Invalid:
        return 0;
    default:
        return 0;
    }
}

/// @note   Kia's binary format is as follows:
///
///         Field Name      | Size (bytes)  | Offset (bytes)
///         ----------------|---------------|---------------
///         Timestamp [ms]  | u64 (8 bytes) | 0
///         Command         | u64 (1 byte)  | 8
///         Key             | u64 (8 bytes) | 9
///         Object size [B] | u32 (4 bytes) | 17
///         Time-to-live [s]| u32 (4 bytes) | 21
///
///         N.B. Everything is little-endian.
///         N.B. Key is the MurmurHash3 64-bit hash of the original key.
///         N.B. Object size = Key size + value size.
///         N.B. Only includes GET and SET requests.
/// @note   Sari's binary format is as follows:
///
///         Field Name      | Size (bytes)  | Offset (bytes)
///         ----------------|---------------|---------------
///         Timestamp [s]   | u32 (4 bytes) | 0
///         Key             | u64 (8 bytes) | 4
///         Object size [B] | u32 (4 bytes) | 12
///         Time-to-live [s]| u32 (4 bytes) | 16
///
///         N.B. Everything is little-endian as far as I can tell.
///         N.B. Object size = Key size + value size.
///         N.B. Only includes GET whose object has an associated TTL in
///              the original Twitter trace. See Sari's TTLs Matter repo
///              for more details.
/// @note   Yang's Twitter binary format is as follows:
///
///         Field Name      | Size (bytes)     | Offset (bytes)
///         ----------------|------------------|---------------
///         Timestamp [ms]  | u32 (4 bytes)    | 0
///         Key             | u64 (8 bytes)    | 4
///         Key Size [B]    | u32 (10/32 bits) | 12 (10 upper bits)
///         Value Size [B]  |     (22/32 bits) | 12 (22 lower bits)
///         Command         | u32 (8/32 bits)  | 16 (8 upper bits)
///         Time-to-live [s]|     (24/32 bits) | 16 (24 lower bits)
///         Client ID*      | u32 (4 bytes)    | 20
///
///         N.B. Everything is little-endian.
///         N.B. Key is the MurmurHash3 64-bit hash of the original key.
///         N.B. *Client ID is not part of Yang's original format, but
///         it is included in the Twitter trace data, so I include it.
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

uint64_t
CacheAccess::size_bytes() const
{
    return key_size_b + value_size_b;
}

bool
CacheAccess::is_read() const
{
    return CacheCommand__is_any_read(command);
}

bool
CacheAccess::is_write() const
{
    return CacheCommand__is_any_write(command);
}

/// @note   Based on the way we instantiate TTLs, this is essentially
///         equivalent to checking if the operation is a write.
bool
CacheAccess::has_ttl() const
{
    return !std::isnan(ttl_ms);
}

double
CacheAccess::expiration_time_ms() const
{
    return timestamp_ms + ttl_ms;
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
    ss << (std::isfinite(ttl_ms) ? (uint64_t)ttl_ms : 0);
    if (newline) {
        ss << std::endl;
    }
    return ss.str();
}
