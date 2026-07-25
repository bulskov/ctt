/*
 * A small, self-contained ctt suite that doubles as ctt's own smoke test.
 * Build via CMake (add_test target `ctt_example`) or directly:
 *
 *     cc -Iinclude examples/example_tests.c src/ctt.c -o example && ./example
 */
#include "ctt.h"

/* A trivial "stack" to have something to exercise. */
#define CAP 8
static int stack[CAP];
static int depth;

static int push(int v) { return depth < CAP ? (stack[depth++] = v, 1) : 0; }
static int pop(void)   { return stack[--depth]; }

/* Optional per-test fixture reset. Defining ctt_before_each overrides the
   library's weak default. */
void ctt_before_each(void)
{
    depth = 0;
}

CTT_TEST(push_then_pop_is_lifo)
{
    ASSERT_TRUE(push(1));
    ASSERT_TRUE(push(2));
    ASSERT_EQ(2, depth);
    ASSERT_EQ(2, pop());
    ASSERT_EQ(1, pop());
    ASSERT_EQ(0, depth);
}

CTT_TEST(push_respects_capacity)
{
    for (int i = 0; i < CAP; i++)
        ASSERT_TRUE(push(i));
    ASSERT_FALSE(push(99)); /* full */
    ASSERT_EQ(CAP, depth);
}

/* TEST_NAMED gives a custom display label. */
TEST_NAMED(relational_and_pointer_asserts, "relational + pointer asserts")
{
    ASSERT_NE(1, 2);
    ASSERT_LT(1, 2);
    ASSERT_GE(2, 2);
    int a, b;
    ASSERT_PTR_NE(&a, &b);
    ASSERT_PTR_EQ(&a, &a);
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "ctt example suite");
}
