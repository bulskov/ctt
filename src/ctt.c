/*
 * ctt implementation unit.
 *
 * This is the single translation unit that emits ctt's implementation, so the
 * CMake target has something to compile into a static library. Consumers who
 * only drop in include/ctt.h can ignore this file and instead put
 *
 *     #define CTT_IMPLEMENTATION
 *     #include "ctt.h"
 *
 * in one .c file of their own.
 */
#define CTT_IMPLEMENTATION
#include "ctt.h"
