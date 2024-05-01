#pragma once
#include "types/time_stamp_type.h"
#include <stdbool.h>

struct LookupReturn {
    bool success;
    TimeStampType timestamp;
};

enum PutUniqueStatus {
    LOOKUP_PUTUNIQUE_ERROR,
    LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE,
    LOOKUP_PUTUNIQUE_REPLACE_VALUE,
};
