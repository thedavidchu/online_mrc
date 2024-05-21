#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "trace/trace.h"

void
Trace__destroy(struct Trace *const me)
{
    if (me == NULL)
        return;

    // HACK This is an utter hack to get around the const-qualifiers.
    free((void *)me->trace);
    *me = (struct Trace){0};
}
