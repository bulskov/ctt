/*
 * ctt's own tests — positive paths.
 *
 * These run ctt *on itself*: every assertion here should PASS, confirming the
 * macros compile and accept valid inputs, that both the short (ASSERT_EQ) and
 * prefixed (CTT_ASSERT_EQ) spellings work, and that the lifecycle hooks fire.
 *
 * The negative/crash paths (a failing assertion is reported, a crash is caught,
 * setup abort, --filter, exit codes) cannot be asserted from inside a passing
 * run — see behavior_tests.sh for those.
 */
#include "ctt.h"

static int before_ran;
static int after_ran;

void ctt_before_each(void) { before_ran = 1; }
void ctt_after_each(void) { after_ran = 1; }

CTT_TEST(before_each_runs)
{
    ASSERT_EQ(1, before_ran);
}

CTT_TEST(after_each_ran_last_time)
{
    /* Not the first test, so a previous ctt_after_each must have fired. */
    ASSERT_EQ(1, after_ran);
}

CTT_TEST(equality)
{
    ASSERT_EQ(5, 5);
    ASSERT_NE(5, 6);
}

CTT_TEST(relational)
{
    ASSERT_LT(1, 2);
    ASSERT_LE(2, 2);
    ASSERT_GT(3, 2);
    ASSERT_GE(3, 3);
}

CTT_TEST(booleans)
{
    ASSERT(1 == 1);
    ASSERT_TRUE(1);
    ASSERT_FALSE(0);
}

CTT_TEST(nullness)
{
    int x;
    ASSERT_NOT_NULL(&x);
    ASSERT_NULL(NULL);
}

CTT_TEST(pointers)
{
    int a, b;
    ASSERT_PTR_EQ(&a, &a);
    ASSERT_PTR_NE(&a, &b);
}

CTT_TEST(strings)
{
    ASSERT_STR_EQ("hi", "hi");
    ASSERT_STRN_EQ("hello", "help", 3); /* "hel" == "hel" */
}

CTT_TEST(substrings)
{
    ASSERT_STR_CONTAINS("hello world", "lo wo"); /* middle */
    ASSERT_STR_CONTAINS("hello world", "hello"); /* at the start */
    ASSERT_STR_CONTAINS("hello world", "world"); /* at the end */
    ASSERT_STR_CONTAINS("hello", "hello");       /* whole haystack */
    ASSERT_STR_CONTAINS("hello", "");            /* empty needle is always found */

    ASSERT_STR_NOT_CONTAINS("hello world", "zzz");
    ASSERT_STR_NOT_CONTAINS("hello", "hello!");  /* needle longer than haystack */
    ASSERT_STR_NOT_CONTAINS("hello", "HELLO");   /* the search is case-sensitive */
    ASSERT_STR_NOT_CONTAINS("", "x");            /* empty haystack */
}

/* The generated-output case these macros exist for: a multi-line buffer
   searched for a line that should (or should not) be in it. */
CTT_TEST(substrings_multiline)
{
    const char *out = "compiling foo.c\nwarning: unused variable 'x'\ndone\n";

    ASSERT_STR_CONTAINS(out, "warning: unused variable 'x'");
    ASSERT_STR_CONTAINS(out, "\ndone\n");
    ASSERT_STR_NOT_CONTAINS(out, "error:");
}

CTT_TEST(arrays)
{
    int expected[] = {1, 2, 3};
    int actual[] = {1, 2, 3};
    ASSERT_ARRAY_EQ(expected, actual, 3);
}

CTT_TEST(floats)
{
    ASSERT_FLOAT_EQ(1.0, 1.0001, 0.01);
}

/* Prefixed spellings resolve to the same behavior. */
CTT_TEST(prefixed_names_work)
{
    CTT_ASSERT_EQ(2, 2);
    CTT_ASSERT_TRUE(1);
    CTT_ASSERT_STR_EQ("x", "x");
    CTT_ASSERT_STR_CONTAINS("xyz", "y");
    CTT_ASSERT_STR_NOT_CONTAINS("xyz", "q");
}

TEST_NAMED(custom_label, "a custom display label")
{
    ASSERT_TRUE(1);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "ctt self-tests (positive paths)");
}
