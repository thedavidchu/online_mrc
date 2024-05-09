// NOTE This file name starts with an underscore because I don't want you to
//      directly include it in other non-hash header files.
#pragma once

#include <stdint.h>

typedef uint32_t Hash32BitType;
typedef uint64_t Hash64BitType;

struct Hash128BitType {
    Hash64BitType hash[2];
};
