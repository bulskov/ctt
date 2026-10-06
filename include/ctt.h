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
 * Portable to GCC and Clang on POSIX systems, and to MSVC, clang-cl and
 * MinGW on Windows. Test auto-registration uses __attribute__((constructor))
 * (a .CRT$XCU initializer on MSVC); the optional lifecycle hooks are weak
 * symbols (/alternatename on MSVC).
 *
 * License: MIT. See LICENSE.
 */
#ifndef CTT_H
#define CTT_H

/* ctt catches crashes with sigaction() and unwinds out of the handler with
   sigsetjmp/siglongjmp. Those are POSIX, not ISO C, and a strict -std=c99
   makes glibc hide them, so ask for them before including anything.
   Only under __STRICT_ANSI__: in the default -std=gnu* modes glibc
   already exposes them, and naming a feature macro there would *narrow* what
   the rest of the translation unit sees. */
#if defined(__STRICT_ANSI__) && !defined(_WIN32) &&                                       \
    !defined(_POSIX_C_SOURCE) && !defined(_XOPEN_SOURCE) &&                               \
    !defined(_GNU_SOURCE) && !defined(_DEFAULT_SOURCE) && !defined(_BSD_SOURCE)
/* Feature macros only take effect before the first libc header. __GLIBC__ is
   defined by <features.h>, so seeing it here means one was already included
   and we are too late to ask — say so plainly instead of failing later with a
   confusing "unknown type name 'sigjmp_buf'". */
#if defined(__GLIBC__)
#error "ctt.h must be included before any standard header when compiling with -std=c99 (or compile with -D_POSIX_C_SOURCE=200809L)"
#endif
#define _POSIX_C_SOURCE 200809L
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <setjmp.h>

/* Crashes and failed assertions unwind back to the runner with a long jump.
   POSIX has sigsetjmp, which also restores the signal mask so a crash handler
   can fire again for the next test. Windows has no signal mask, so plain
   setjmp/longjmp do the same job there. */
#ifdef _WIN32
#define CTT_JMP_BUF jmp_buf
#define CTT_SETJMP(env) setjmp(env)
#define CTT_LONGJMP(env, val) longjmp(env, val)
#else
#define CTT_JMP_BUF sigjmp_buf
#define CTT_SETJMP(env) sigsetjmp(env, 1)
#define CTT_LONGJMP(env, val) siglongjmp(env, val)
#endif

/* ANSI colors + symbols (prefixed to avoid clashing with consumer macros). */
#define CTT_COL_RED "\x1b[31m"
#define CTT_COL_GREEN "\x1b[32m"
#define CTT_COL_YELLOW "\x1b[33m"
#define CTT_COL_BLUE "\x1b[34m"
#define CTT_COL_MAGENTA "\x1b[35m"
#define CTT_COL_CYAN "\x1b[36m"
#define CTT_COL_RESET "\x1b[0m"

/* Spelled as UTF-8 byte escapes so every compiler emits the same bytes:
   MSVC would otherwise reinterpret a literal "✓" in the local code page
   unless the consumer builds with /utf-8. */
#define CTT_CHECK "\xE2\x9C\x93"          /* ✓ */
#define CTT_CROSS "\xE2\x9C\x97"          /* ✗ */
#define CTT_ARROW "\xE2\x86\x92"          /* → */
#define CTT_SYM_INFO "\xE2\x84\xB9"       /* ℹ */
#define CTT_SYM_WARN "\xE2\x9A\xA0"       /* ⚠ */
#define CTT_SYM_PASS "\xE2\x9C\x85"       /* ✅ */
#define CTT_SYM_FAIL "\xE2\x9D\x8C"       /* ❌ */
#define CTT_SYM_PARTY "\xF0\x9F\x8E\x89" /* 🎉 */
#define CTT_SYM_TEST "\xF0\x9F\xA7\xAA"  /* 🧪 */
#define CTT_SYM_BULLET "\xE2\x80\xA2"     /* • */

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
extern CTT_JMP_BUF ctt_jmp_buf;

/* Called by the CTT_TEST macro's constructor before main(). */
void ctt_register(const char *name, ctt_test_fn func);

/* ------------------------------------------------------------------ */
/* Test declaration + auto-registration                                */
/* ------------------------------------------------------------------ */
/* CTT_CONSTRUCTOR_(f) opens a function f that runs before main(). MSVC has no
   constructor attribute, so f is put in the CRT's C initializer table
   (.CRT$XCU). The pointer must have external linkage, and the /include
   pragma stops the linker from discarding it. As a result, two tests with
   the same function name in different .c files collide under MSVC. */
#ifdef _MSC_VER
#pragma section(".CRT$XCU", read)
#ifdef _M_IX86
#define CTT_SYM_PREFIX_ "_" /* 32-bit x86 decorates C symbols with '_' */
#else
#define CTT_SYM_PREFIX_ ""
#endif
#define CTT_CONSTRUCTOR_(f)                                            \
    static void f(void);                                               \
    __pragma(comment(linker, "/include:" CTT_SYM_PREFIX_ #f "_ptr"))  \
    __declspec(allocate(".CRT$XCU")) void (*f##_ptr)(void) = f;        \
    static void f(void)
#else
#define CTT_CONSTRUCTOR_(f) __attribute__((constructor)) static void f(void)
#endif

#define CTT_TEST_NAMED(fn, label)                                      \
    static void fn(void);                                              \
    CTT_CONSTRUCTOR_(ctt_reg_##fn)                                     \
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
            CTT_LONGJMP(ctt_jmp_buf, 1);   \
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

/* Unconditional failure with a printf-style message.  Use when the useful
   diagnostic is computed, not a fixed string. */
#define CTT_FAILF(...)                                                                    \
    do                                                                                    \
    {                                                                                     \
        printf("  " CTT_COL_RED CTT_CROSS " FAIL: ");                                     \
        printf(__VA_ARGS__);                                                              \
        printf(CTT_COL_RESET "\n");                                                       \
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

/* Substring search.  On failure both the needle and the full haystack are
   printed, which is what you want when the haystack is generated output. */
#define CTT_ASSERT_STR_CONTAINS(haystack, needle)                                               \
    do                                                                                          \
    {                                                                                           \
        const char *ctt_h_ = (haystack);                                                        \
        const char *ctt_n_ = (needle);                                                          \
        if (ctt_h_ == NULL || strstr(ctt_h_, ctt_n_) == NULL)                                   \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected to find '%s' in:" CTT_COL_RESET   \
                   "\n%s\n", ctt_n_, ctt_h_ ? ctt_h_ : "(null)");                               \
            CTT_FAIL_LOC_();                                                                    \
            CTT_ABORT_();                                                                       \
        }                                                                                       \
    } while (0)

#define CTT_ASSERT_STR_NOT_CONTAINS(haystack, needle)                                           \
    do                                                                                          \
    {                                                                                           \
        const char *ctt_h_ = (haystack);                                                        \
        const char *ctt_n_ = (needle);                                                          \
        if (ctt_h_ != NULL && strstr(ctt_h_, ctt_n_) != NULL)                                   \
        {                                                                                       \
            printf("  " CTT_COL_RED CTT_CROSS " FAIL: Expected NOT to find '%s' in:"             \
                   CTT_COL_RESET "\n%s\n", ctt_n_, ctt_h_);                                     \
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
#define FAILF CTT_FAILF
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
#define ASSERT_STR_CONTAINS CTT_ASSERT_STR_CONTAINS
#define ASSERT_STR_NOT_CONTAINS CTT_ASSERT_STR_NOT_CONTAINS
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
/* Missing from older MinGW headers. */
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
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
CTT_JMP_BUF ctt_jmp_buf;

static Ctt_TestCase ctt_registry[CTT_MAX_TESTS];
static int ctt_registered_count = 0;
static const char *ctt_name_filter = NULL;

/* Set inside a signal handler to the signal number; 0 means "no crash". */
static volatile sig_atomic_t ctt_crash_signal = 0;

/* Bounded copy that always terminates dst. Not strncpy: MSVC deprecates it,
   and its C11 strncpy_s replacement is missing from glibc. */
static void ctt_copy_str(char *dst, size_t size, const char *src)
{
    size_t n = strlen(src);
    if (n >= size)
        n = size - 1;
    memcpy(dst, src, n);
    dst[n] = '\0';
}

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
        ctt_copy_str(ctt_results.failed_test_names[ctt_results.failed_test_count],
                     sizeof(ctt_results.failed_test_names[0]),
                     ctt_results.current_test_name);
        ctt_results.failed_test_count++;
    }
    ctt_results.failed_tests++;
}

/* Default no-op lifecycle hooks. A suite that defines its own
   ctt_before_each/ctt_after_each overrides these (strong symbol wins).
   MSVC has no weak symbols. There, /alternatename makes the linker fall back
   to the defaults only when the suite leaves a hook undefined. */
#ifdef _MSC_VER
void ctt_default_before_each(void) {}
void ctt_default_after_each(void) {}
#ifdef _M_IX86
#pragma comment(linker, "/alternatename:_ctt_before_each=_ctt_default_before_each")
#pragma comment(linker, "/alternatename:_ctt_after_each=_ctt_default_after_each")
#else
#pragma comment(linker, "/alternatename:ctt_before_each=ctt_default_before_each")
#pragma comment(linker, "/alternatename:ctt_after_each=ctt_default_after_each")
#endif
#else
__attribute__((weak)) void ctt_before_each(void) {}
__attribute__((weak)) void ctt_after_each(void) {}
#endif

static void ctt_crash_handler(int sig)
{
#ifdef _WIN32
    /* The Windows CRT resets a handler to SIG_DFL before calling it. */
    signal(sig, ctt_crash_handler);
#endif
    ctt_crash_signal = sig;
    CTT_LONGJMP(ctt_jmp_buf, 2);
}

/* On MSVC, crashes are caught with structured exception handling (SEH)
   around each test step. The CRT's signal() only reports a few hardware
   faults (not integer division by zero) and only on the main thread. Each
   exception is mapped to the matching signal so reporting stays the same.
   Stack overflow is left alone: recovering from it needs _resetstkoflw().
   SIGABRT still goes through signal(), because abort() raises it. */
#ifdef _MSC_VER
static int ctt_seh_filter(unsigned long code)
{
    int sig;
    switch (code)
    {
    case EXCEPTION_ACCESS_VIOLATION:
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
    case EXCEPTION_IN_PAGE_ERROR:
#ifdef CTT_HAS_ASAN
        return EXCEPTION_CONTINUE_SEARCH; /* let ASan report it */
#else
        sig = SIGSEGV;
        break;
#endif
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
    case EXCEPTION_INT_OVERFLOW:
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
    case EXCEPTION_FLT_INVALID_OPERATION:
    case EXCEPTION_FLT_OVERFLOW:
    case EXCEPTION_FLT_UNDERFLOW:
    case EXCEPTION_FLT_INEXACT_RESULT:
    case EXCEPTION_FLT_DENORMAL_OPERAND:
    case EXCEPTION_FLT_STACK_CHECK:
        sig = SIGFPE;
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
    case EXCEPTION_PRIV_INSTRUCTION:
        sig = SIGILL;
        break;
    default:
        return EXCEPTION_CONTINUE_SEARCH;
    }
    ctt_crash_signal = sig;
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

/* Runs one step of a test (setup + body, or teardown). Only MSVC needs a
   guard frame here; elsewhere the signal handlers jump back on their own. */
static void ctt_run_step(void (*step)(ctt_test_fn), ctt_test_fn func)
{
#ifdef _MSC_VER
    __try
    {
        step(func);
    }
    __except (ctt_seh_filter(GetExceptionCode()))
    {
    }
#else
    step(func);
#endif
}

static void ctt_step_body(ctt_test_fn func)
{
    ctt_before_each();
    func();
}

static void ctt_step_teardown(ctt_test_fn func)
{
    (void)func;
    ctt_after_each();
}

static void ctt_install_crash_handlers(void)
{
#ifdef _WIN32
#ifdef _MSC_VER
    /* No "abort() has been called" dialog or Windows Error Reporting. */
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#else
    /* MinGW: its CRT routes hardware faults to signal() handlers. */
    signal(SIGFPE, ctt_crash_handler);
#ifndef CTT_HAS_ASAN
    signal(SIGSEGV, ctt_crash_handler);
#endif
#endif
    signal(SIGABRT, ctt_crash_handler);
#else
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
#endif
}

static const char *ctt_signal_name(int sig)
{
    switch (sig)
    {
    case SIGSEGV: return "SIGSEGV (segmentation fault)";
#ifdef SIGBUS
    case SIGBUS:  return "SIGBUS (bus error)";
#endif
    case SIGILL:  return "SIGILL (illegal instruction)";
    case SIGABRT: return "SIGABRT (abort)";
    case SIGFPE:  return "SIGFPE (arithmetic error)";
    default:      return "signal";
    }
}

void ctt_run_one(const char *name, ctt_test_fn func)
{
    ctt_results.total_tests++;
    ctt_copy_str(ctt_results.current_test_name, sizeof(ctt_results.current_test_name), name);

    printf("Running: %s", name);
    fflush(stdout);

    ctt_crash_signal = 0;
    int failed_before = ctt_results.failed_tests;
    ctt_results.test_start_time = clock();

    /* Run setup + body under a jump point. A failed assertion (value 1) or a
       crashing signal (value 2) unwinds back here. */
    ctt_results.jump_active = 1;
    if (CTT_SETJMP(ctt_jmp_buf) == 0)
        ctt_run_step(ctt_step_body, func);

    /* Teardown always runs, with jumping disabled so a failing assertion there
       records instead of unwinding. A fresh jump point still catches a crash
       during teardown so it can't kill the whole runner. */
    ctt_results.jump_active = 0;
    if (CTT_SETJMP(ctt_jmp_buf) == 0)
        ctt_run_step(ctt_step_teardown, func);

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
            printf(" " CTT_SYM_PASS " PASS (%.3fs)\n", t);
        else
            printf(" " CTT_SYM_PASS " PASS\n");
    }
    else
    {
        printf(" " CTT_SYM_FAIL " FAIL\n");
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
            printf("  " CTT_COL_RED CTT_SYM_BULLET " %s" CTT_COL_RESET "\n", ctt_results.failed_test_names[i]);
    }
    else
    {
        printf("\n" CTT_COL_GREEN CTT_SYM_PARTY " All tests passed!" CTT_COL_RESET "\n");
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
        printf(CTT_SYM_TEST " %s\n", suite_title);
        for (size_t i = 0; i < strlen(suite_title) + 3; i++)
            putchar('=');
        putchar('\n');
    }

    return ctt_run_all();
}

#endif /* CTT_IMPLEMENTATION_INCLUDED */
#endif /* CTT_IMPLEMENTATION */
