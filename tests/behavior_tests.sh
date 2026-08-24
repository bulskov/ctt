#!/usr/bin/env bash
#
# Behavior tests for ctt — the paths that can't be asserted from inside a
# passing run. Each case compiles a small crafted suite, runs it, and checks
# stdout + the process exit code.
#
# Usage:  behavior_tests.sh [CC] [CTT_ROOT]
#   CC        C compiler       (default: $CC or cc)
#   CTT_ROOT  ctt repo root    (default: this script's parent dir)
#
# Suites are compiled WITHOUT sanitizers: these cases test control flow and the
# crash handler, and our SIGSEGV handler is intentionally disabled under ASan.

set -u

CC="${1:-${CC:-cc}}"
CTT_ROOT="${2:-$(cd "$(dirname "$0")/.." && pwd)}"
INC="$CTT_ROOT/include"
IMPL="$CTT_ROOT/src/ctt.c"

WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT

fails=0
ok()   { printf '  ok     - %s\n' "$1"; }
bad()  { printf '  NOT OK - %s\n' "$1"; fails=$((fails + 1)); }

# want <text> <needle> <desc>       : text must contain needle
want()    { case "$1" in *"$2"*) ok "$3" ;; *) bad "$3 (missing: $2)" ;; esac; }
# wantnot <text> <needle> <desc>    : text must NOT contain needle
wantnot() { case "$1" in *"$2"*) bad "$3 (unexpected: $2)" ;; *) ok "$3" ;; esac; }
# code <actual> <expected> <desc>
code()    { if [ "$1" -eq "$2" ]; then ok "$3"; else bad "$3 (exit $1, want $2)"; fi; }

# strip ANSI color escapes so we can match plain substrings like "Passed: 2"
strip() { printf '%s' "$1" | sed $'s/\x1b\\[[0-9;]*m//g'; }

# build <srcfile> <binname> ; echoes nothing, returns compiler status
build() { "$CC" -I"$INC" -g -O0 "$1" "$IMPL" -o "$2" 2>"$WORK/cc.log"; }

section() { printf '\n# %s\n' "$1"; }

# --------------------------------------------------------------------------
section "failing assertion is reported and the run continues (exit 1)"
cat > "$WORK/c1.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(a_pass)  { ASSERT_EQ(1, 1); }
TEST(b_fail)  { ASSERT_EQ(1, 2); }
TEST(c_after) { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c1"); }
EOF
if build "$WORK/c1.c" "$WORK/c1"; then
    raw="$("$WORK/c1" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1 on failure"
    want "$out" "Total tests: 3" "all three tests ran"
    want "$out" "Passed: 2" "two passed"
    want "$out" "Failed: 1" "one failed"
    want "$out" "Running: c_after" "runner continued past the failure"
    want "$out" "• b_fail" "failed test named in summary"
else
    bad "case c1 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "a failing assertion in setup aborts the test body"
cat > "$WORK/c2.c" <<'EOF'
#include "ctt.h"
#include <string.h>
void ctt_before_each(void) {
    if (strcmp(ctt_results.current_test_name, "needs_setup") == 0)
        ASSERT_TRUE(0);   /* force setup failure for this test only */
}
void ctt_after_each(void) {}
TEST(needs_setup) { printf("BODY_RAN\n"); }
TEST(unaffected)  { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c2"); }
EOF
if build "$WORK/c2.c" "$WORK/c2"; then
    raw="$("$WORK/c2" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1"
    wantnot "$out" "BODY_RAN" "test body skipped when setup fails"
    want "$out" "Running: unaffected" "other tests still run"
    want "$out" "• needs_setup" "setup-failed test marked failed"
else
    bad "case c2 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "a crash is caught and the runner continues (exit 1)"
cat > "$WORK/c3.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(will_crash)   { int *p = 0; *p = 5; }
TEST(after_crash)  { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c3"); }
EOF
if build "$WORK/c3.c" "$WORK/c3"; then
    raw="$("$WORK/c3" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1 after a crash"
    want "$out" "CRASH" "crash was reported"
    want "$out" "Running: after_crash" "runner continued after the crash"
else
    bad "case c3 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "--filter selects a subset by substring"
cat > "$WORK/c4.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(alpha_one) { ASSERT_TRUE(1); }
TEST(alpha_two) { ASSERT_TRUE(1); }
TEST(beta_one)  { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c4"); }
EOF
if build "$WORK/c4.c" "$WORK/c4"; then
    raw="$("$WORK/c4" --filter alpha 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 0 "filtered run all-pass exits 0"
    want "$out" "Total tests: 2" "only the two matching tests ran"
    want "$out" "Running: alpha_one" "alpha_one ran"
    wantnot "$out" "Running: beta_one" "beta_one was filtered out"
else
    bad "case c4 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "--stop-on-failure halts after the first failure"
cat > "$WORK/c5.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(s_pass)  { ASSERT_TRUE(1); }
TEST(s_fail)  { ASSERT_EQ(1, 2); }
TEST(s_never) { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c5"); }
EOF
if build "$WORK/c5.c" "$WORK/c5"; then
    raw="$("$WORK/c5" --stop-on-failure 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1"
    want "$out" "Running: s_fail" "reached the failing test"
    wantnot "$out" "Running: s_never" "stopped before the next test"
else
    bad "case c5 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "a failing substring assertion prints the needle and the whole haystack"
cat > "$WORK/c6.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(missing_needle) {
    const char *out = "compiling foo.c\nwarning: bad thing\ndone\n";
    ASSERT_STR_CONTAINS(out, "error: not here");
}
TEST(unwanted_needle) {
    ASSERT_STR_NOT_CONTAINS("compiling foo.c\nerror: boom\n", "error:");
}
TEST(c6_after) { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c6"); }
EOF
if build "$WORK/c6.c" "$WORK/c6"; then
    raw="$("$WORK/c6" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1"
    want "$out" "Expected to find 'error: not here' in:" "CONTAINS names the needle"
    want "$out" "compiling foo.c" "CONTAINS prints the haystack"
    want "$out" "warning: bad thing" "CONTAINS prints every line of the haystack"
    want "$out" "Expected NOT to find 'error:' in:" "NOT_CONTAINS names the needle"
    want "$out" "error: boom" "NOT_CONTAINS prints the haystack"
    want "$out" "Running: c6_after" "runner continued past both failures"
    want "$out" "Failed: 2" "both substring assertions failed"
else
    bad "case c6 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
# The two macros treat a NULL haystack differently, on purpose: CONTAINS cannot
# find anything in NULL so it fails (printing "(null)" rather than crashing),
# while NOT_CONTAINS is satisfied — NULL holds no needle. Pinned here so the
# asymmetry is a decision rather than an accident.
section "a NULL haystack fails CONTAINS and passes NOT_CONTAINS"
cat > "$WORK/c7.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(null_contains)     { const char *p = NULL; ASSERT_STR_CONTAINS(p, "x"); }
TEST(null_not_contains) { const char *p = NULL; ASSERT_STR_NOT_CONTAINS(p, "x"); }
int main(int c, char **v) { return ctt_main(c, v, "c7"); }
EOF
if build "$WORK/c7.c" "$WORK/c7"; then
    raw="$("$WORK/c7" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1"
    want "$out" "(null)" "NULL haystack is reported, not dereferenced"
    want "$out" "• null_contains" "CONTAINS fails on a NULL haystack"
    wantnot "$out" "• null_not_contains" "NOT_CONTAINS passes on a NULL haystack"
    want "$out" "Failed: 1" "exactly one of the two failed"
else
    bad "case c7 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
section "FAILF formats its message and fails immediately"
cat > "$WORK/c8.c" <<'EOF'
#include "ctt.h"
void ctt_before_each(void) {}
void ctt_after_each(void) {}
TEST(failf_formats) {
    int n = 42;
    FAILF("computed %d for %s", n, "the widget");
    printf("UNREACHABLE\n");
}
TEST(failf_bare_format) { FAILF("no varargs here"); }
TEST(c8_after) { ASSERT_TRUE(1); }
int main(int c, char **v) { return ctt_main(c, v, "c8"); }
EOF
if build "$WORK/c8.c" "$WORK/c8"; then
    raw="$("$WORK/c8" 2>&1)"; rc=$?; out="$(strip "$raw")"
    code "$rc" 1 "exits 1"
    want "$out" "FAIL: computed 42 for the widget" "printf-style arguments are formatted"
    wantnot "$out" "UNREACHABLE" "FAILF aborts the rest of the test"
    want "$out" "FAIL: no varargs here" "a lone format string works"
    want "$out" "Running: c8_after" "runner continued past both failures"
    want "$out" "Failed: 2" "both FAILF tests failed"
else
    bad "case c8 failed to compile"; cat "$WORK/cc.log"
fi

# --------------------------------------------------------------------------
printf '\n'
if [ "$fails" -eq 0 ]; then
    printf 'behavior_tests: all checks passed\n'
    exit 0
else
    printf 'behavior_tests: %d check(s) failed\n' "$fails"
    exit 1
fi
