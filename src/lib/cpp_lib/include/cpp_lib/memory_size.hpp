#pragma once

#include <cstdint>

using uint64_t = std::uint64_t;

namespace MemorySize {
constexpr uint64_t Byte = 1;
constexpr uint64_t KiB = 1024 * Byte;
constexpr uint64_t MiB = 1024 * KiB;
constexpr uint64_t GiB = 1024 * MiB;
constexpr uint64_t TiB = 1024 * GiB;
} // namespace MemorySize
