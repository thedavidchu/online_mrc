#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>

#include "array/binary64_array.h"
#include "array/print_array.h"
#include "logger/logger.h"
#include "statistics/statistics.h"

bool
Statistics__init(struct Statistics *const me, size_t const f64_per_item)
{
    if (me == NULL || f64_per_item == 0) {
        LOGGER_ERROR("invalid arguments");
        return false;
    }
    if (!Binary64Array__init(&me->stats)) {
        LOGGER_ERROR("failed to initialize array");
        return false;
    }
    // NOTE I append the metadata during the initialization phase since
    //      then I don't have to modify the array's saving code. I admit
    //      that this may be semantically confusing, but it's easier to
    //      write this way... ah, classic shooting my future self in the
    //      foot. Ouch. You're welcome <3
    if (!Binary64Array__append(&me->stats, &f64_per_item)) {
        LOGGER_ERROR("failed to append size to array");
        goto cleanup_err;
    }
    me->b64_per_item = f64_per_item;
    return true;
cleanup_err:
    Statistics__destroy(me);
    return false;
}

void
Statistics__destroy(struct Statistics *const me)
{
    Binary64Array__destroy(&me->stats);
    *me = (struct Statistics){0};
}

bool
Statistics__append_binary64(struct Statistics *const me, void const *const data)
{
    // NOTE I need data to be 64 bits wide so I arbitrarily decided
    //      to use uint64_t instead of double. The decision is based
    //      on the fact that uint64_t has '64' in the name.
    if (!Binary64Array__append_array(&me->stats, data, me->b64_per_item)) {
        LOGGER_WARN("failed to append array at %p to array", data);
        print_array(LOGGER_STREAM,
                    data,
                    me->b64_per_item,
                    sizeof(uint64_t),
                    true,
                    _print_binary64);
        return false;
    }
    return true;
}

bool
Statistics__append_float64(struct Statistics *const me,
                           double const *const data)
{
    return Statistics__append_binary64(me, data);
}

bool
Statistics__append_uint64(struct Statistics *const me,
                          uint64_t const *const data)
{
    return Statistics__append_binary64(me, data);
}

bool
Statistics__save(struct Statistics const *const me, char const *const path)
{
    return Binary64Array__save(&me->stats, path);
}
