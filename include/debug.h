/*
 macros for debugging
*/

#ifndef DEBUG_H
#define DEBUG_H

#include <assert.h>
#include <stdio.h>
void print_stacktrace();

#define ANSI_FG_BLACK   "\33[1;30m"
#define ANSI_FG_RED     "\33[1;31m"
#define ANSI_FG_GREEN   "\33[1;32m"
#define ANSI_FG_YELLOW  "\33[1;33m"
#define ANSI_FG_BLUE    "\33[1;34m"
#define ANSI_FG_MAGENTA "\33[1;35m"
#define ANSI_FG_CYAN    "\33[1;36m"
#define ANSI_FG_WHITE   "\33[1;37m"
#define ANSI_BG_BLACK   "\33[1;40m"
#define ANSI_BG_RED     "\33[1;41m"
#define ANSI_BG_GREEN   "\33[1;42m"
#define ANSI_BG_YELLOW  "\33[1;43m"
#define ANSI_BG_BLUE    "\33[1;44m"
#define ANSI_BG_MAGENTA "\33[1;45m"
#define ANSI_BG_CYAN    "\33[1;46m"
#define ANSI_BG_WHITE   "\33[1;47m"
#define ANSI_NONE       "\33[0m"

#define ANSI_FMT(str, fmt) fmt str ANSI_NONE

#define Log(format, ...) do { \
  printf(ANSI_FMT("[%s:%d %s] " format, ANSI_FG_BLUE) "\n", \
      __FILE__, __LINE__, __func__, ## __VA_ARGS__); \
  fflush(stdout); \
} while (0)

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
