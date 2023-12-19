#pragma once

#include <stdio.h>

enum LoggerLevels {
    LOGGER_LEVEL_TRACE = 0,
    LOGGER_LEVEL_DEBUG = 1,
    LOGGER_LEVEL_INFO = 2,
    LOGGER_LEVEL_WARN = 3,
    LOGGER_LEVEL_ERROR = 4,
    LOGGER_LEVEL_FATAL = 5,
};

#define LOGGER_TRACE(...)                                                      \
    do {                                                                       \
        fprintf(stderr, "[TRACE] %s:%d : ", __FILE__, __LINE__);               \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define LOGGER_DEBUG(...)                                                      \
    do {                                                                       \
        fprintf(stderr, "[DEBUG] %s:%d : ", __FILE__, __LINE__);               \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define LOGGER_INFO(...)                                                       \
    do {                                                                       \
        fprintf(stderr, "[INFO] %s:%d : ", __FILE__, __LINE__);                \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)
#define LOGGER_WARN(...)                                                       \
    do {                                                                       \
        fprintf(stderr, "[WARN] %s:%d : ", __FILE__, __LINE__);                \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define LOGGER_ERROR(...)                                                      \
    do {                                                                       \
        fprintf(stderr, "[ERROR] %s:%d : ", __FILE__, __LINE__);               \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)

#define LOGGER_FATAL(...)                                                      \
    do {                                                                       \
        fprintf(stderr, "[FATAL] %s:%d : ", __FILE__, __LINE__);               \
        fprintf(stderr, __VA_ARGS__);                                          \
        fprintf(stderr, "\n");                                                 \
    } while (0)
