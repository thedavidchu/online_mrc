/** This implements the phase sampling described in the AET paper,
 *  whereby at the end of an epoch, we measure the Euclidean difference
 *  between the old and new histograms */

#include <assert.h>
#include <stdbool.h>

#include <glib.h>
#include <stdio.h>

#include "histogram/histogram.h"
#include "logger/logger.h"
#include "sampler/phase_sampler.h"
#include "unused/mark_unused.h"

bool
PhaseSampler__init(struct PhaseSampler *const me)
{
    me->saved_histograms = g_ptr_array_new();
    if (me->saved_histograms == NULL) {
        LOGGER_ERROR("failed to allocate ptr arrray");
        return false;
    }
    return true;
}

static void
free_gstr(void *data, void *user_data)
{
    UNUSED(user_data);

    if (remove(data) != 0) {
        LOGGER_ERROR("failed to remove file '%s'", data);
    }
    free(data);
}

void
PhaseSampler__destroy(struct PhaseSampler *const me)
{
    g_ptr_array_foreach(me->saved_histograms, free_gstr, NULL);
    g_ptr_array_free(me->saved_histograms, true);
    *me = (struct PhaseSampler){0};
}

bool
should_i_create_a_new_histogram(struct Histogram const *const old_hist,
                                struct Histogram const *const new_hist,
                                double const threshold)
{
    double norm2 = Histogram__euclidean_error(old_hist, new_hist);
    if (norm2 < 0.0) {
        LOGGER_ERROR("Euclidean distance must be positive");
        // If we have an error, we should try allocating a new histogram.
        return true;
    }
    return (norm2 > threshold);
}

static gchar *
create_temporary_file_path(size_t const id)
{
    // TODO Upon upgrading to GLib 2.78, you can simplify this with
    //      the function 'g_string_new_take'.
    gchar *cwd = g_get_current_dir();
    GString *tmp_name = g_string_new(NULL);
    g_string_append_printf(tmp_name, ".tmp-histogram-%zu.bin", id);
    gchar *path = g_build_path("/", cwd, tmp_name->str, NULL);

    free(cwd);
    g_string_free(tmp_name, true);
    return path;
}

bool
PhaseSampler__change_histogram(struct PhaseSampler *const me,
                               struct Histogram const *const old_hist)
{
    size_t const id = me->saved_histograms->len;
    gchar *new_tmp = create_temporary_file_path(id);
    LOGGER_TRACE("saving to '%s'", new_tmp);
    bool r = Histogram__save_to_file(old_hist, new_tmp);
    assert(r);

    g_ptr_array_add(me->saved_histograms, new_tmp);

    return true;
}
