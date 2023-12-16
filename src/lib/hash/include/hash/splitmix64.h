#pragma once

#include <stdint.h>

#include "hash/types.h"

Hash64BitType
splitmix64_hash(const uint64_t key);
