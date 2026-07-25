/*
 * ctt — a tiny single-header C unit-test library
 * https://github.com/bulskov/ctt
 *
 * Usage
 * -----
 *   #include "ctt.h"              // wherever you write tests
 *
 *   #define CTT_IMPLEMENTATION    // in exactly ONE .c file, before the include
 *   #include "ctt.h"             // (or link the prebuilt ctt::ctt target)
 *
 * Configuration macros (define before including):
 *   CTT_IMPLEMENTATION   emit the implementation in this translation unit.
 *   CTT_NO_SHORT_NAMES   do NOT define the unprefixed aliases (TEST, ASSERT_EQ,
 *                        FAIL, ...); use the CTT_-prefixed names instead. Define
 *                        this if a bare macro collides with your project.
 *
 * Public API is namespaced: macros are CTT_*, C symbols are ctt_*. Short,
 * unprefixed macro aliases are provided by default for ergonomic tests.
 *
 * Requires GCC or Clang (uses __attribute__((constructor)) for test
 * auto-registration and weak symbols for lifecycle hooks).
 *
 * License: MIT. See LICENSE.
 */
#ifndef CTT_H
#define CTT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>

#ifdef _WIN32
#define CTT_STRNCPY strncpy_s
#else
#define CTT_STRNCPY strncpy
#endif

/* ANSI colors + symbols (prefixed to avoid clashing with consumer macros). */
#define CTT_COL_RED "\x1b[31m"
#define CTT_COL_GREEN "\x1b[32m"
#define CTT_COL_YELLOW "\x1b[33m"
#define CTT_COL_BLUE "\x1b[34m"
#define CTT_COL_MAGENTA "\x1b[35m"
#define CTT_COL_CYAN "\x1b[36m"
#define CTT_COL_RESET "\x1b[0m"

#define CTT_CHECK "✓"
#define CTT_CROSS "✗"
#define CTT_ARROW "→"
#define CTT_SYM_INFO "ℹ"
#define CTT_SYM_WARN "⚠"

/* ------------------------------------------------------------------ */
/* Registry + result tracking                                          */
/* ------------------------------------------------------------------ */
typedef void (*ctt_test_fn)(void);

#define CTT_MAX_TESTS 512

typedef struct
{
    const char *name;
    ctt_test_fn func;
} Ctt_TestCase;

typedef struct
{
    int total_tests;
    int passed_tests;
    int failed_tests;
    char failed_test_names[CTT_MAX_TESTS][256];
    int failed_test_count;
    char current_test_name[256];
    clock_t test_start_time;
    clock_t total_time;
    int verbose_mode;
    int stop_on_first_failure;
    int jump_active; /* 1 while inside a test body: assertion failures longjmp out */
} Ctt_Results;

extern Ctt_Results ctt_results;
extern sigjmp_buf ctt_jmp_buf;

/* Called by the CTT_TEST macro's constructor before main(). */
void ctt_register(const char *name, ctt_test_fn func);

/* ------------------------------------------------------------------ */
/* Test declaration + auto-registration                                */
/* ------------------------------------------------------------------ */
#define CTT_TEST_NAMED(fn, label)                                      \
    static void fn(void);                                              \
    __attribute__((constructor)) static void _ctt_reg_##fn(void)       \
    {                                                                  \
        ctt_register(label, fn);                                       \
    }                                                                  \
    static void fn(void)

#define CTT_TEST(fn) CTT_TEST_NAMED(fn, #fn)

/* ------------------------------------------------------------------ */
/* Assertions                                                          */
/* ------------------------------------------------------------------ */
/* Internal helpers — not part of the public API. */
#define CTT_ABORT_()                       \
    do                                     \
    {                                      \
        ctt_record_failure();              \
        if (ctt_results.jump_active)       \
            siglongjmp(ctt_jmp_buf, 1);    \
    } while (0)

#define CTT_FAIL_LOC_() \
    printf("     at line %d in test '%s'\n", __LINE__, ctt_results.current_test_name)

/* Unconditional failure with a custom message. */
#define CTT_FAIL(msg)                                                                     \
    do                                                                                    \
    {                                                                                     \
        printf("  " CTT_COL_RED CTT_CROSS " FAIL: %s" CTT_COL_RESET "\n", (msg));         \
        CTT_FAIL_LOC_();                                                                  \
        CTT_ABORT_();                                                                     \
    } while (0)

#define CTT_ASSERT(condition)                                                             \
    do                                                                                    \
    {                                                                                     \
        if (!(condition))                                                                 \
        {                                                                                 \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: %s" CTT_COL_RESET "\n", #condition); \
            CTT_FAIL_LOC_();                                                              \
            CTT_ABORT_();                                                                 \
        }                                                                                 \
    } while (0)

#define CTT_ASSERT_EQ(expected, actual)                                                        \
    do                                                                                          \
    {                                                                                           \
        long long _e = (long long)(expected);                                                   \
        long long _a = (long long)(actual);                                                     \
        if (_e != _a)                                                                           \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected %lld, got %lld" CTT_COL_RESET     \
                   "\n", _e, _a);                                                               \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

/* Relational assertions. Operands compared as long long (same domain as
   CTT_ASSERT_EQ) — good for ints, sizes, and pointer differences. */
#define CTT_CMP_(a, op, b)                                                                      \
    do                                                                                          \
    {                                                                                           \
        long long _a = (long long)(a);                                                          \
        long long _b = (long long)(b);                                                          \
        if (!(_a op _b))                                                                        \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: %lld " #op " %lld is false" CTT_COL_RESET  \
                   "\n", _a, _b);                                                               \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_NE(expected, actual) CTT_CMP_((expected), !=, (actual))
#define CTT_ASSERT_LT(a, b) CTT_CMP_((a), <, (b))
#define CTT_ASSERT_LE(a, b) CTT_CMP_((a), <=, (b))
#define CTT_ASSERT_GT(a, b) CTT_CMP_((a), >, (b))
#define CTT_ASSERT_GE(a, b) CTT_CMP_((a), >=, (b))

#define CTT_ASSERT_PTR_EQ(expected, actual)                                                     \
    do                                                                                          \
    {                                                                                           \
        if ((void *)(expected) != (void *)(actual))                                             \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected %p, got %p" CTT_COL_RESET         \
                   "\n", (void *)(expected), (void *)(actual));                                 \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_PTR_NE(unexpected, actual)                                                   \
    do                                                                                          \
    {                                                                                           \
        if ((void *)(unexpected) == (void *)(actual))                                           \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: pointers equal (%p), expected differ"      \
                   CTT_COL_RESET "\n", (void *)(actual));                                       \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_STR_EQ(expected, actual)                                                     \
    do                                                                                          \
    {                                                                                           \
        if (strcmp((expected), (actual)) != 0)                                                  \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected '%s', got '%s'" CTT_COL_RESET     \
                   "\n", (expected), (actual));                                                 \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_STRN_EQ(expected, actual, size)                                              \
    do                                                                                          \
    {                                                                                           \
        if (strncmp((expected), (actual), size) != 0)                                           \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected '%s', got '%s'" CTT_COL_RESET     \
                   "\n", (expected), (actual));                                                 \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_NOT_NULL(ptr)                                                                \
    do                                                                                          \
    {                                                                                           \
        if ((ptr) == NULL)                                                                      \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected non-NULL pointer, got NULL"       \
                   CTT_COL_RESET "\n");                                                         \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_NULL(ptr)                                                                    \
    do                                                                                          \
    {                                                                                           \
        if ((ptr) != NULL)                                                                      \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected NULL pointer, got %p"             \
                   CTT_COL_RESET "\n", (void *)(ptr));                                          \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_TRUE(condition)                                                              \
    do                                                                                          \
    {                                                                                           \
        if (!(condition))                                                                       \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected true, got false: %s"              \
                   CTT_COL_RESET "\n", #condition);                                             \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_FALSE(condition)                                                             \
    do                                                                                          \
    {                                                                                           \
        if (condition)                                                                          \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected false, got true: %s"              \
                   CTT_COL_RESET "\n", #condition);                                             \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_ARRAY_EQ(expected, actual, size)                                             \
    do                                                                                          \
    {                                                                                           \
        for (int _i = 0; _i < (size); _i++)                                                     \
        {                                                                                       \
            if ((expected)[_i] != (actual)[_i])                                                 \
            {                                                                                   \
                printf("  " CTT_COL_RED CTT_CROSS                                               \
                       " FAIL: Array differs at index %d: expected %d, got %d" CTT_COL_RESET     \
                       "\n", _i, (int)(expected)[_i], (int)(actual)[_i]);                       \
                CTT_FAIL_LOC_();                                                                \
                CTT_ABORT_();                                                                   \
            }                                                                                   \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_FLOAT_EQ(expected, actual, tolerance)                                        \
    do                                                                                          \
    {                                                                                           \
        double _diff = ((expected) - (actual));                                                 \
        if (_diff < 0)                                                                           \
            _diff = -_diff;                                                                      \
        if (_diff > (tolerance))                                                                 \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected %f, got %f (diff: %f > %f)"        \
                   CTT_COL_RESET "\n", (double)(expected), (double)(actual), _diff,             \
                   (double)(tolerance));                                                        \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_INFO(msg, ...)                                             \
    do                                                                 \
    {                                                                  \
        if (ctt_results.verbose_mode)                                  \
            printf("  " CTT_SYM_INFO "  INFO: " msg "\n", ##__VA_ARGS__); \
    } while (0)

#define CTT_WARN(msg, ...) \
    printf("  " CTT_SYM_WARN "  WARN: " msg "\n", ##__VA_ARGS__)

/* ------------------------------------------------------------------ */
/* Runner API                                                          */
/* ------------------------------------------------------------------ */
void ctt_init(void);
void ctt_run_one(const char *name, ctt_test_fn func);
int ctt_run_all(void); /* runs every registered test; returns exit code */
void ctt_print_summary(void);
void ctt_set_verbose(int verbose);
void ctt_set_stop_on_failure(int stop);
void ctt_set_filter(const char *substr);
void ctt_record_failure(void);
void ctt_setup_console(void);

/* Lifecycle hooks — define these in your suite to build/free fixtures around
   every test. Weak no-op defaults are provided, so they are optional. */
void ctt_before_each(void);
void ctt_after_each(void);

/* Standard entry point: parses -v/--verbose, --stop-on-failure,
   --filter <substr>, runs all registered tests, prints the summary, and
   returns the process exit code (0 = all passed). Pass NULL for no banner. */
int ctt_main(int argc, char *argv[], const char *suite_title);

/* ------------------------------------------------------------------ */
/* Short, unprefixed aliases (opt out with CTT_NO_SHORT_NAMES)          */
/* ------------------------------------------------------------------ */
#ifndef CTT_NO_SHORT_NAMES
#define TEST CTT_TEST
#define TEST_NAMED CTT_TEST_NAMED
#define FAIL CTT_FAIL
#define ASSERT CTT_ASSERT
#define ASSERT_EQ CTT_ASSERT_EQ
#define ASSERT_NE CTT_ASSERT_NE
#define ASSERT_LT CTT_ASSERT_LT
#define ASSERT_LE CTT_ASSERT_LE
#define ASSERT_GT CTT_ASSERT_GT
#define ASSERT_GE CTT_ASSERT_GE
#define ASSERT_TRUE CTT_ASSERT_TRUE
#define ASSERT_FALSE CTT_ASSERT_FALSE
#define ASSERT_NULL CTT_ASSERT_NULL
#define ASSERT_NOT_NULL CTT_ASSERT_NOT_NULL
#define ASSERT_PTR_EQ CTT_ASSERT_PTR_EQ
#define ASSERT_PTR_NE CTT_ASSERT_PTR_NE
#define ASSERT_STR_EQ CTT_ASSERT_STR_EQ
#define ASSERT_STRN_EQ CTT_ASSERT_STRN_EQ
#define ASSERT_ARRAY_EQ CTT_ASSERT_ARRAY_EQ
#define ASSERT_FLOAT_EQ CTT_ASSERT_FLOAT_EQ
#define TEST_INFO CTT_INFO
#define TEST_WARN CTT_WARN
#endif /* CTT_NO_SHORT_NAMES */

#endif /* CTT_H */

/* ================================================================== */
/* Implementation                                                      */
/* ================================================================== */
#ifdef CTT_IMPLEMENTATION
#ifndef CTT_IMPLEMENTATION_INCLUDED
#define CTT_IMPLEMENTATION_INCLUDED

#include <signal.h>

/* AddressSanitizer installs its own SIGSEGV/SIGBUS handler with much better
   diagnostics, so we only install ours when ASan is NOT active. */
#if defined(__SANITIZE_ADDRESS__)
#define CTT_HAS_ASAN 1
#elif defined(__has_feature)
#if __has_feature(address_sanitizer)
#define CTT_HAS_ASAN 1
#endif
#endif

#ifdef _WIN32
#include <windows.h>
void ctt_setup_console(void)
{
    SetConsoleOutputCP(CP_UTF8);
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}
#else
void ctt_setup_console(void) {}
#endif

Ctt_Results ctt_results;
sigjmp_buf ctt_jmp_buf;

static Ctt_TestCase ctt_registry[CTT_MAX_TESTS];
static int ctt_registered_count = 0;
static const char *ctt_name_filter = NULL;

/* Set inside a signal handler to the signal number; 0 means "no crash". */
static volatile sig_atomic_t ctt_crash_signal = 0;

void ctt_register(const char *name, ctt_test_fn func)
{
    if (ctt_registered_count >= CTT_MAX_TESTS)
    {
        fprintf(stderr, "ctt: too many tests (max %d)\n", CTT_MAX_TESTS);
        return;
    }
    ctt_registry[ctt_registered_count].name = name;
    ctt_registry[ctt_registered_count].func = func;
    ctt_registered_count++;
}

void ctt_init(void)
{
    memset(&ctt_results, 0, sizeof(ctt_results));
}

void ctt_set_verbose(int verbose) { ctt_results.verbose_mode = verbose; }
void ctt_set_stop_on_failure(int stop) { ctt_results.stop_on_first_failure = stop; }
void ctt_set_filter(const char *substr) { ctt_name_filter = substr; }

void ctt_record_failure(void)
{
    if (ctt_results.failed_test_count < CTT_MAX_TESTS)
    {
        CTT_STRNCPY(ctt_results.failed_test_names[ctt_results.failed_test_count],
                    ctt_results.current_test_name,
                    sizeof(ctt_results.failed_test_names[0]) - 1);
        ctt_results.failed_test_count++;
    }
    ctt_results.failed_tests++;
}

/* Default no-op lifecycle hooks. A suite that defines its own
   ctt_before_each/ctt_after_each overrides these (strong symbol wins). */
__attribute__((weak)) void ctt_before_each(void) {}
__attribute__((weak)) void ctt_after_each(void) {}

static void ctt_crash_handler(int sig)
{
    ctt_crash_signal = sig;
    siglongjmp(ctt_jmp_buf, 2);
}

static void ctt_install_crash_handlers(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = ctt_crash_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGABRT, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
#ifndef CTT_HAS_ASAN
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGBUS, &sa, NULL);
#endif
}

static const char *ctt_signal_name(int sig)
{
    switch (sig)
    {
    case SIGSEGV: return "SIGSEGV (segmentation fault)";
    case SIGBUS:  return "SIGBUS (bus error)";
    case SIGABRT: return "SIGABRT (abort)";
    case SIGFPE:  return "SIGFPE (arithmetic error)";
    default:      return "signal";
    }
}

void ctt_run_one(const char *name, ctt_test_fn func)
{
    ctt_results.total_tests++;
    CTT_STRNCPY(ctt_results.current_test_name, name, sizeof(ctt_results.current_test_name) - 1);
    ctt_results.current_test_name[sizeof(ctt_results.current_test_name) - 1] = '\0';

    printf("Running: %s", name);
    fflush(stdout);

    ctt_crash_signal = 0;
    int failed_before = ctt_results.failed_tests;
    ctt_results.test_start_time = clock();

    /* Run setup + body under a jump point. A failed assertion (value 1) or a
       crashing signal (value 2) unwinds back here. */
    ctt_results.jump_active = 1;
    int jumped = sigsetjmp(ctt_jmp_buf, 1);
    if (jumped == 0)
    {
        ctt_before_each();
        func();
    }

    /* Teardown always runs, with jumping disabled so a failing assertion there
       records instead of unwinding. A fresh jump point still catches a crash
       during teardown so it can't kill the whole runner. */
    ctt_results.jump_active = 0;
    if (sigsetjmp(ctt_jmp_buf, 1) == 0)
    {
        ctt_after_each();
    }

    clock_t end = clock();
    ctt_results.total_time += end - ctt_results.test_start_time;

    if (ctt_crash_signal != 0)
    {
        printf("  " CTT_COL_RED CTT_CROSS " CRASH: caught %s" CTT_COL_RESET "\n",
               ctt_signal_name(ctt_crash_signal));
        if (ctt_results.failed_tests == failed_before)
            ctt_record_failure();
        printf(" " CTT_COL_RED CTT_CROSS " CRASHED\n" CTT_COL_RESET);
    }
    else if (ctt_results.failed_tests == failed_before)
    {
        ctt_results.passed_tests++;
        double t = ((double)(end - ctt_results.test_start_time)) / CLOCKS_PER_SEC;
        if (t > 0.001)
            printf(" ✅ PASS (%.3fs)\n", t);
        else
            printf(" ✅ PASS\n");
    }
    else
    {
        printf(" ❌ FAIL\n");
    }
}

int ctt_run_all(void)
{
    ctt_install_crash_handlers();

    for (int i = 0; i < ctt_registered_count; i++)
    {
        if (ctt_name_filter && strstr(ctt_registry[i].name, ctt_name_filter) == NULL)
            continue;

        ctt_run_one(ctt_registry[i].name, ctt_registry[i].func);

        if (ctt_results.stop_on_first_failure && ctt_results.failed_tests > 0)
            break;
    }

    ctt_print_summary();
    return ctt_results.failed_tests > 0 ? 1 : 0;
}

void ctt_print_summary(void)
{
    printf("\n" CTT_COL_BLUE "=== Test Summary ===" CTT_COL_RESET "\n");
    printf("Total tests: %d\n", ctt_results.total_tests);
    printf("Passed: " CTT_COL_GREEN "%d " CTT_CHECK CTT_COL_RESET "\n", ctt_results.passed_tests);
    printf("Failed: " CTT_COL_RED "%d " CTT_CROSS CTT_COL_RESET "\n", ctt_results.failed_tests);

    if (ctt_results.total_time > 0)
    {
        double total = ((double)ctt_results.total_time) / CLOCKS_PER_SEC;
        printf("Total time: " CTT_COL_CYAN "%.3f seconds" CTT_COL_RESET "\n", total);
        if (ctt_results.total_tests > 0)
            printf("Avg per test: " CTT_COL_CYAN "%.3f seconds" CTT_COL_RESET "\n",
                   total / ctt_results.total_tests);
    }

    if (ctt_results.failed_tests > 0)
    {
        printf("\n" CTT_COL_RED CTT_CROSS " Failed tests:" CTT_COL_RESET "\n");
        for (int i = 0; i < ctt_results.failed_test_count; i++)
            printf("  " CTT_COL_RED "• %s" CTT_COL_RESET "\n", ctt_results.failed_test_names[i]);
    }
    else
    {
        printf("\n" CTT_COL_GREEN "🎉 All tests passed!" CTT_COL_RESET "\n");
    }
}

int ctt_main(int argc, char *argv[], const char *suite_title)
{
    ctt_setup_console();
    ctt_init();

    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0)
            ctt_set_verbose(1);
        else if (strcmp(argv[i], "--stop-on-failure") == 0)
            ctt_set_stop_on_failure(1);
        else if (strcmp(argv[i], "--filter") == 0 && i + 1 < argc)
            ctt_set_filter(argv[++i]);
    }

    if (suite_title)
    {
        printf("🧪 %s\n", suite_title);
        for (size_t i = 0; i < strlen(suite_title) + 3; i++)
            putchar('=');
        putchar('\n');
    }

    return ctt_run_all();
}

#endif /* CTT_IMPLEMENTATION_INCLUDED */
#endif /* CTT_IMPLEMENTATION */
