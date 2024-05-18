#include <stddef.h>
#include <stdint.h>
#include <sys/time.h>

#include "timer/timer.h"
#include "unused/mark_unused.h"

static const uint64_t USEC_PER_SEC = 1000000;
static const double SEC_PER_USEC = 1e-6;

double
get_wall_time_sec(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);

    UNUSED(USEC_PER_SEC);
    return (double)tv.tv_sec + tv.tv_usec * SEC_PER_USEC;
}
