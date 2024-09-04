#include <stdbool.h>
#include <stddef.h>

#include "array/float64_array.h"
#include "logger/logger.h"
#include "statistics/statistics.h"

bool
Statistics__init(struct Statistics *const me, size_t const f64_per_item)
{
    if (me == NULL || f64_per_item == 0) {
        LOGGER_ERROR("invalid arguments");
        return false;
    }
    if (!Float64Array__init(&me->stats)) {
        LOGGER_ERROR("failed to initialize array");
        return false;
    }
    // NOTE I append the metadata during the initialization phase since
    //      then I don't have to modify the array's saving code. I admit
    //      that this may be semantically confusing, but it's easier to
    //      write this way... ah, classic shooting my future self in the
    //      foot. Ouch. You're welcome <3
    if (!Float64Array__append(&me->stats, (double)f64_per_item)) {
        LOGGER_ERROR("failed to append size to array");
        goto cleanup_err;
    }
    me->f64_per_item = f64_per_item;
    return true;
cleanup_err:
    Statistics__destroy(me);
    return false;
}

void
Statistics__destroy(struct Statistics *const me)
{
    Float64Array__destroy(&me->stats);
    *me = (struct Statistics){0};
}

bool
Statistics__append(struct Statistics *const me, double const *const data)
{
    bool ok = true;
    for (size_t i = 0; i < me->f64_per_item; ++i) {
        if (!Float64Array__append(&me->stats, data[i])) {
            LOGGER_WARN("failed to append %f to array", data[i]);
            ok = false;
        }
    }
    return ok;
}

bool
Statistics__save(struct Statistics const *const me, char const *const path)
{
    return Float64Array__save(&me->stats, path);
}
