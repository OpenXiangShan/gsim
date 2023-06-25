/**
 * @file debug.h
 * @brief macros for debugging
 */

#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>

#define Assert(cond, ...)           \
  do {                              \
    if (!(cond)) {                  \
      fprintf(stderr, "\33[1;31m"); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\33[0m\n");  \
      assert(cond);                 \
    }                               \
  } while (0)

#define TODO() Assert(0, "Implement ME!")
#ifdef DEBUG
#define MUX_DEBUG(...) __VA_ARGS__
#else
#define MUX_DEBUG(...)
#endif

#endif
