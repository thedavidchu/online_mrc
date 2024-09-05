#include <assert.h>
#include <stdbool.h>
#include <stdint.h>

#include <glib.h>

#include "lookup/dictionary.h"
#include "lookup/lookup.h"
#include "test/mytester.h"

#define MAX_SIZE 10
#define STREAM   stdout

static char const *const keys[MAX_SIZE] =
    {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
static char const *const values[MAX_SIZE] = {"zero",
                                             "one",
                                             "two",
                                             "three",
                                             "four",
                                             "five",
                                             "six",
                                             "seven",
                                             "eight",
                                             "nine"};
static char const *const alt_keys[MAX_SIZE] =
    {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
static char const *const alt_values[MAX_SIZE] =
    {"A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};

static void
test_successful_gets(struct Dictionary const *const me,
                     char const *const *const successful_keys,
                     char const *const *const successful_values)
{
    if (successful_keys == NULL || successful_values == NULL) {
        return;
    }
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        char const *const value = Dictionary__get(me, successful_keys[i]);
        g_assert_cmpstr(value, ==, successful_values[i]);
    }
}

static void
test_unsuccesful_gets(struct Dictionary const *const me,
                      char const *const *const unsuccessful_keys)
{
    if (unsuccessful_keys == NULL) {
        return;
    }
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        char const *const nullptr = Dictionary__get(me, unsuccessful_keys[i]);
        g_assert_false(nullptr);
    }
}

static bool
test_dictionary(void)
{
    struct Dictionary me = {0};
    g_assert_true(Dictionary__init(&me));
    test_unsuccesful_gets(&me, keys);
    test_unsuccesful_gets(&me, alt_keys);
    Dictionary__write(&me, STREAM, true);

    // Test successful inserts
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        enum PutUniqueStatus r = Dictionary__put(&me, keys[i], values[i]);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_INSERT_KEY_VALUE);
    }
    test_successful_gets(&me, keys, values);
    test_unsuccesful_gets(&me, alt_keys);
    Dictionary__write(&me, STREAM, true);

    // Test successful duplicate inserts
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        enum PutUniqueStatus r = Dictionary__put(&me, keys[i], values[i]);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_REPLACE_VALUE);
    }
    test_successful_gets(&me, keys, values);
    test_unsuccesful_gets(&me, alt_keys);

    // Test successful replacements
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        enum PutUniqueStatus r = Dictionary__put(&me, keys[i], alt_values[i]);
        g_assert_cmpint(r, ==, LOOKUP_PUTUNIQUE_REPLACE_VALUE);
    }
    test_successful_gets(&me, keys, alt_values);
    test_unsuccesful_gets(&me, alt_keys);
    Dictionary__write(&me, STREAM, true);

    // Test successful deletes
    for (uint64_t i = 0; i < MAX_SIZE; ++i) {
        bool r = Dictionary__remove(&me, keys[i]);
        g_assert_true(r);
    }
    test_unsuccesful_gets(&me, keys);
    test_unsuccesful_gets(&me, alt_keys);
    // Test unsuccessful deletes
    for (uint64_t i = MAX_SIZE; i < MAX_SIZE; ++i) {
        bool r = Dictionary__remove(&me, alt_keys[i]);
        g_assert_false(r);
    }
    test_unsuccesful_gets(&me, keys);
    test_unsuccesful_gets(&me, alt_keys);
    Dictionary__write(&me, STREAM, true);

    Dictionary__destroy(&me);
    return true;
}

int
main(void)
{
    ASSERT_FUNCTION_RETURNS_TRUE(test_dictionary());
    return EXIT_SUCCESS;
}
