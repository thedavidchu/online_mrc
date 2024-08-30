#pragma once
#include <stdbool.h>

#include "run/runner_arguments.h"
#include "trace/trace.h"

bool
run_runner(struct RunnerArguments const *const args,
           struct Trace const *const trace);
