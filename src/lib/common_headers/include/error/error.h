#pragma once

#include <stdio.h>

#define LOG_ERROR(msg)                                                         \
    fprintf(stderr, "[ERROR] %s:%d : %s\n", __FILE__, __LINE__, (msg))
