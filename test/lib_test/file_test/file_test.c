#include <stdbool.h>

#include <glib.h>

#include "file/file.h"
#include "logger/logger.h"
#include "test/mytester.h"

static bool
test_absolute_path_case(char const *const rel_path,
                        char const *const oracle_path)
{
    LOGGER_TRACE("testing '%s' -> '%s'",
                 rel_path ? rel_path : "(null)",
                 oracle_path ? oracle_path : "(null)");
    char *abs_path = NULL;
    abs_path = get_absolute_path(rel_path);
    g_assert_cmpstr(abs_path, ==, oracle_path);
    free(abs_path);
    return true;
}

static bool
test_absolute_path(void)
{
    test_absolute_path_case(NULL, NULL);
    test_absolute_path_case("", NULL);
    // NOTE I say paths must start with './', '../', '~/', or '/'!
    test_absolute_path_case("illegal", NULL);
    test_absolute_path_case("/does/not/exist", "/does/not/exist");
    test_absolute_path_case("/does/not/exist/", "/does/not/exist/");

    // NOTE We don't just use the canonicalize function because it does
    //      not expand paths starting with '~'.
    char *oracle = NULL;
    char const *home = g_get_home_dir();

    oracle = g_canonicalize_filename("", home);
    test_absolute_path_case("~", oracle);
    free(oracle);

    oracle = g_canonicalize_filename("", home);
    test_absolute_path_case("~/", oracle);
    free(oracle);

    oracle = g_canonicalize_filename("projects", home);
    test_absolute_path_case("~/projects", oracle);
    free(oracle);

    oracle = g_canonicalize_filename("./non/existent", NULL);
    test_absolute_path_case("./non/existent", oracle);
    free(oracle);

    oracle = g_canonicalize_filename("../non/existent", NULL);
    test_absolute_path_case("../non/existent", oracle);
    free(oracle);

    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_absolute_path());
    return 0;
}
