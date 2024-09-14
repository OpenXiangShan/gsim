#include <stdio.h>
#include "uint.h"

void uint512_t::operator = (uint1024_t data) {
  // uint512_t ret;
  u256_1 = data.u512_0.u256_1;
  u256_0 = data.u512_0.u256_0;
  // return ret;
}

uint512_t uint512_t::tail(int n) {
  uint512_t ret;
  if (n > 256) {
    ret.u256_0 = u256_0;
    int shiftNum = (512 - n);
    ret.u256_1 = (u256_1 << shiftNum) >> shiftNum;
  } else {
    ret.u256_1 = 0;
    ret.u256_0 = (u256_0 << n) >> n;
  }
  return ret;
}

uint1024_t uint1024_t::tail(int n) {
  uint1024_t ret;
  if (n > 512) {
    ret.u512_0 = u512_0;
    ret.u512_1 = u512_1.tail(n - 512);
  } else {
    ret.u512_1 = 0;
    ret.u512_0 = u512_0.tail(n);
  }
  return ret;
}

uint2048_t uint2048_t::tail(int n) {
  uint2048_t ret;
  if (n > 1024) {
    ret.u1024_0 = u1024_0;
    ret.u1024_1 = u1024_1.tail(n - 1024);
  } else {
    ret.u1024_1 = 0;
    ret.u1024_0 = u1024_0.tail(n);
  }
  return ret;
}

uint512_t uint512_t::flip() {
  uint512_t ret;
  ret.u256_0 = ~u256_0;
  ret.u256_1 = ~u256_1;
  return ret;
}

uint1024_t uint1024_t::flip() {
  uint1024_t ret;
  ret.u512_0 = u512_0.flip();
  ret.u512_1 = u512_1.flip();
  return ret;
}

uint2048_t uint2048_t::flip() {
  uint2048_t ret;
  ret.u1024_0 = u1024_0.flip();
  ret.u1024_1 = u1024_1.flip();
  return ret;
}
