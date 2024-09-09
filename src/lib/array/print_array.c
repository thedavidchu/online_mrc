#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "array/print_array.h"
#include "arrays/is_last.h"
#include "logger/logger.h"

bool
_print_bool(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%s", *(bool const *const)element_ptr ? "true" : "false");
    return true;
}

bool
_print_int(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%d", *(int const *const)element_ptr);
    return true;
}

bool
_print_binary64(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%" PRIx64, *(uint64_t const *const)element_ptr);
    return true;
}

bool
_print_uint64(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%" PRIu64, *(uint64_t const *const)element_ptr);
    return true;
}

bool
_print_size(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%zu", *(size_t const *const)element_ptr);
    return true;
}

bool
_print_double(FILE *const stream, void const *const element_ptr)
{
    fprintf(stream, "%f", *(double const *const)element_ptr);
    return true;
}

bool
print_array(FILE *const stream,
            void const *const array,
            size_t const length,
            size_t const element_size,
            bool const newline,
            bool (*const print)(FILE *const stream,
                                void const *const element_ptr))
{
    bool ok = true;
    if (stream == NULL) {
        return false;
    }
    if (array == NULL) {
        fprintf(stream, "(null)%s", newline ? "\n" : "");
        return true;
    }
    fprintf(stream, "[");
    for (size_t i = 0; i < length; ++i) {
        void const *const element_ptr =
            &((char const *const)array)[i * element_size];
        if (!print(stream, element_ptr)) {
            LOGGER_ERROR("bad print on iteration %zu", i);
            ok = false;
        }
        if (!is_last(i, length)) {
            fprintf(stream, ", ");
        }
    }
    fprintf(stream, "]%s", newline ? "\n" : "");

    return ok;
}
