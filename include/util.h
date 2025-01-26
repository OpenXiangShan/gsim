#ifndef UTIL_H
#define UTIL_H

#include <sys/time.h>

OPType str2op_expr2(std::string name);
OPType str2op_expr1(std::string name);
OPType str2op_expr1int1(std::string name);

int upperPower2(int x);
int upperLog2(int x);

std::string to_hex_string(BASIC_TYPE x);
std::pair<int, std::string> firStrBase(std::string s);
std::string format(const char *fmt, ...);
void print_stacktrace();

static inline struct timeval getTime() {
  struct timeval now;
  gettimeofday(&now, NULL);
  return now;
}

static inline uint64_t diffTime(struct timeval &t0, struct timeval &t1) {
  uint64_t t0_us = t0.tv_sec * 1000000 + t0.tv_usec;
  uint64_t t1_us = t1.tv_sec * 1000000 + t1.tv_usec;
  return t1_us - t0_us;
}

static inline void showTime(const char *msg, struct timeval &t0, struct timeval &t1) {
  uint64_t diff_ms = diffTime(t0, t1) / 1000;
  printf(ANSI_FMT("%s = %ld ms\n", ANSI_FG_MAGENTA), msg, diff_ms);
  fflush(stdout);
}

#endif
