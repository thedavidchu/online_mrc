#include <assert.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "arrays/is_last.h"
#include "invariants/implies.h"
#include "logger/logger.h"
#include "trace/trace.h"

static void
print_trace_item_array(FILE *stream, struct Trace const *const me)
{
    assert(stream != NULL && me != NULL);
    assert(implies(me->length != 0, me->trace != NULL));
    for (size_t i = 0; i < me->length; ++i) {
        fprintf(stream, "%" PRIu64, me->trace[i].key);
        if (!is_last(i, me->length)) {
            fprintf(stream, ", ");
        }
    }
}

bool
Trace__init(struct Trace *const me, size_t const length)
{
    if (me == NULL) {
        return false;
    }

    *me = (struct Trace){.trace = calloc(length, sizeof(*me->trace)),
                         .length = length};
    if (me->trace == NULL) {
        LOGGER_ERROR("could not allocate buffer of size %zu * %zu",
                     length,
                     sizeof(*me->trace));
        *me = (struct Trace){.trace = NULL, .length = 0};
        return false;
    }
    return true;
}

void
Trace__write_as_json(FILE *stream, struct Trace const *const me)
{
    if (stream == NULL) {
        LOGGER_WARN("cannot print with NULL stream");
        return;
    }
    if (me == NULL) {
        fprintf(stream, "{\"type\": null}\n");
        return;
    }
    if (me->trace == NULL) {
        fprintf(stream, "{\"type\": \"Trace\", \".trace\": null}\n");
        return;
    }
    fprintf(stream,
            "{\"type\": \"Trace\", \".length\": %zu, \".trace\": [",
            me->length);
    print_trace_item_array(stream, me);
    fprintf(stream, "]}\n");
}

void
Trace__destroy(struct Trace *const me)
{
    if (me == NULL)
        return;

    // HACK This is an utter hack to get around the const-qualifiers.
    free((void *)me->trace);
    *me = (struct Trace){0};
}
