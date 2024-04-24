#pragma once
#include "types/time_stamp_type.h"
#include <stdbool.h>

struct LookupReturn {
    bool success;
    TimeStampType timestamp;
};
