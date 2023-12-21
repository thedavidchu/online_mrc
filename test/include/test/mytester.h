#pragma once
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

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
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);                     \
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
            printf("[ERROR] %s:%d\n", __FILE__, __LINE__);                     \
            /* NOTE This assertion is for debugging purposes so that we have a \
            finer grain understanding of where the failure occurred. */        \
            assert(0 && "exit on failure!");                                   \
            return false;                                                      \
        }                                                                      \
    } while (0)
