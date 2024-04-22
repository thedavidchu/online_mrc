#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#include "logger/logger.h"

#define ASSERT_FUNCTION_RETURNS_TRUE(func_call)                                \
    do {                                                                       \
        bool r = (func_call);                                                  \
        if (!r) {                                                              \
            assert(0 && #func_call " failed");                                 \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
        printf("[SUCCESS] %s:%d " #func_call " succeeded!\n",                  \
               __FILE__,                                                       \
               __LINE__);                                                      \
    } while (0)
