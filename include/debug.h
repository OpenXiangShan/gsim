/*
 macros for debugging
*/

#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>
void print_stacktrace();

#define Assert(cond, ...)           \
  do {                              \
    if (!(cond)) {                  \
      print_stacktrace();           \
      fprintf(stderr, "\33[1;31m"); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\33[0m\n");  \
      fflush(stdout);               \
      assert(cond);                 \
    }                               \
  } while (0)

#define TODO() Assert(0, "Implement ME!")
#define Panic() Assert(0, "Should Not Reach Here!")
#ifdef DEBUG
#define MUX_DEBUG(...) __VA_ARGS__
#else
#define MUX_DEBUG(...)
#endif

#endif
