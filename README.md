# ctt

A tiny single-header C unit-test library. No dependencies, one file to drop in,
and a clean CMake target for `FetchContent`.

- **Single-header** — copy `include/ctt.h` and go, or link `ctt::ctt`.
- **Auto-registration** — write `TEST(...)`; no manual test list in `main()`.
- **Crash isolation** — a segfault in one test is caught and reported; the rest
  of the suite still runs.
- **Sanitizer-friendly** — built to run under AddressSanitizer / UBSan.
- **Namespaced** — `CTT_*` macros and `ctt_*` symbols, with optional short
  aliases (`TEST`, `ASSERT_EQ`, …) on by default.

Requires GCC or Clang (uses `__attribute__((constructor))` and weak symbols).

---

## Install

### CMake — FetchContent (recommended)

```cmake
include(FetchContent)
FetchContent_Declare(ctt
    GIT_REPOSITORY https://github.com/bulskov/ctt.git
    GIT_TAG v0.1.0)
FetchContent_MakeAvailable(ctt)

add_executable(my_tests test/my_tests.c)
target_link_libraries(my_tests PRIVATE ctt::ctt)

enable_testing()
add_test(NAME my_tests COMMAND my_tests)
```

When linked this way you do **not** need `CTT_IMPLEMENTATION` — the library
already compiles the implementation.

### CMake — installed package

```sh
cmake -B build && cmake --build build && cmake --install build --prefix /usr/local
```
then in the consumer:
```cmake
find_package(ctt REQUIRED)
target_link_libraries(my_tests PRIVATE ctt::ctt)
```

### Drop-in (no build system)

Copy `include/ctt.h` into your project. In **one** `.c` file:

```c
#define CTT_IMPLEMENTATION
#include "ctt.h"
```
Include plain `#include "ctt.h"` everywhere else. Or just compile the provided
`src/ctt.c` alongside your tests:

```sh
cc -Iinclude my_tests.c path/to/ctt/src/ctt.c -o my_tests
```

---

## Writing tests

```c
#include "ctt.h"

static int add(int a, int b) { return a + b; }

TEST(adds_two_numbers)
{
    ASSERT_EQ(4, add(2, 2));
}

int main(int argc, char *argv[])
{
    return ctt_main(argc, argv, "My Suite");   // parse flags, run all, return code
}
```

- `TEST(name)` declares **and** auto-registers a test; the function name is the
  label. `TEST_NAMED(name, "label")` sets a custom label.
- `ctt_main()` parses flags, runs every registered test, prints a summary, and
  returns the exit code (`0` = all passed). Pass `NULL` for no banner.

### Setup / teardown

Define the hooks to build and free fixtures around every test (both optional):

```c
static List *list;
void ctt_before_each(void) { list = list_new(); }   // runs before each test
void ctt_after_each(void)  { list_free(list); }      // runs after each, even on failure
```

A failed assertion in `ctt_before_each` aborts the test body; `ctt_after_each`
always runs so fixtures are freed.

---

## Assertions

Every assertion prints a diagnostic with file line and test name, records the
failure, and aborts the current test (later tests still run). Assertions work
anywhere the test reaches — including the hooks and helper functions.

| Assertion | Passes when |
|---|---|
| `ASSERT(cond)` | `cond` is true |
| `ASSERT_TRUE(c)` / `ASSERT_FALSE(c)` | `c` is true / false |
| `ASSERT_EQ(a,b)` / `ASSERT_NE(a,b)` | `a == b` / `a != b` (as `long long`) |
| `ASSERT_LT/LE/GT/GE(a,b)` | `a < b`, `<=`, `>`, `>=` (as `long long`) |
| `ASSERT_NULL(p)` / `ASSERT_NOT_NULL(p)` | `p` is / isn't `NULL` |
| `ASSERT_PTR_EQ(a,b)` / `ASSERT_PTR_NE(a,b)` | pointers equal / differ |
| `ASSERT_STR_EQ(a,b)` | `strcmp(a,b) == 0` |
| `ASSERT_STRN_EQ(a,b,n)` | `strncmp(a,b,n) == 0` |
| `ASSERT_ARRAY_EQ(a,b,n)` | first `n` int elements equal |
| `ASSERT_FLOAT_EQ(a,b,tol)` | `|a - b| <= tol` |
| `FAIL("msg")` | never — fails immediately with a message |

`TEST_INFO("fmt", ...)` prints only under `-v`; `TEST_WARN(...)` always prints.
Neither fails the test.

Each has a `CTT_`-prefixed canonical name (`CTT_ASSERT_EQ`, `CTT_TEST`,
`CTT_FAIL`, …). See naming below.

---

## Naming and collisions

Public API is namespaced: **macros** are `CTT_*`, **C symbols** are `ctt_*`.
For convenience the unprefixed aliases (`TEST`, `ASSERT_EQ`, `FAIL`, …) are
defined by default.

If a bare name collides with your project, opt out before including and use the
`CTT_` names:

```c
#define CTT_NO_SHORT_NAMES
#include "ctt.h"

CTT_TEST(x) { CTT_ASSERT_EQ(1, 1); }
```

---

## Running

The test binary accepts:

| Flag | Effect |
|---|---|
| `-v`, `--verbose` | show `TEST_INFO` output |
| `--stop-on-failure` | stop at the first failing test |
| `--filter <substr>` | run only tests whose name contains `<substr>` |

Exit code is `0` when all pass, `1` otherwise — so it plugs straight into CTest
or CI.

---

## Sanitizers & crashes

Build your tests with `-fsanitize=address,undefined` for leak / use-after-free /
UB detection. `SIGSEGV`, `SIGBUS`, `SIGABRT`, and `SIGFPE` are caught: the
faulting test is reported as `CRASHED` and the runner moves on. Under
AddressSanitizer, `SIGSEGV`/`SIGBUS` are left to ASan for richer diagnostics.

---

## License

MIT — see [LICENSE](LICENSE).
