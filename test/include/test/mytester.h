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

#define ASSERT_TRUE_OR_CLEANUP(func_call, cleanup_expr)                        \
    do {                                                                       \
        bool r = (func_call);                                                  \
        if (!r) {                                                              \
            /* Clean up resources */                                           \
            (cleanup_expr);                                                    \
            LOGGER_ERROR(#func_call);                                          \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert(0 && "exit on failure!");                                   \
            return false;                                                      \
        }                                                                      \
    } while (0)

#define ASSERT_TRUE_WITHOUT_CLEANUP(func_call)                                 \
    do {                                                                       \
        bool r = (func_call);                                                  \
        if (!r) {                                                              \
            /* No clean up required! */                                        \
            LOGGER_ERROR(#func_call);                                          \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert(0 && "exit on failure!");                                   \
            return false;                                                      \
        }                                                                      \
    } while (0)

/// @brief  This macro makes a goto jump to the cleanup section after it
#define ASSERT_TRUE_OR_GOTO_CLEANUP(func_call, goto_label)                     \
    do {                                                                       \
        bool r = (func_call);                                                  \
        if (!r) {                                                              \
            LOGGER_ERROR(#func_call);                                          \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert(0 && "exit on failure!");                                   \
            goto((goto_label));                                                \
            /* NOTE This EXIT_FAILURE condition should never be reached        \
             *      because the above goto-expression should trigger. However, \
             *      I cannot trust the user to actually put a goto-expression  \
             *      where I ask, so I must handle that error too. I detest the \
             *      hackiness of C's macros. */                                \
            LOGGER_FATAL("user failed to provide a valid goto-label");         \
            exit(EXIT_FAILURE);                                                \
        }                                                                      \
    } while (0)
