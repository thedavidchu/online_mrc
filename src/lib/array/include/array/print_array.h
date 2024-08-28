/// @brief  Print an array.
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

bool
_print_bool(FILE *const stream, void const *const element_ptr);

bool
_print_int(FILE *const stream, void const *const element_ptr);

bool
_print_uint64(FILE *const stream, void const *const element_ptr);

bool
_print_size(FILE *const stream, void const *const element_ptr);

bool
_print_double(FILE *const stream, void const *const element_ptr);

bool
print_array(FILE *const stream,
            void const *const array,
            size_t const length,
            size_t const element_size,
            bool const newline,
            bool (*const print)(FILE *const stream,
                                void const *const element_ptr));
