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
    ret.u256_1 = shiftNum == 0 ? u256_1 : ((u256_1 << shiftNum) >> shiftNum);
  } else {
    ret.u256_1 = 0;
    int shiftNum = (256 - n);
    ret.u256_0 = shiftNum == 0 ? u256_0 : ((u256_0 << shiftNum) >> shiftNum);
  }
  return ret;
}

uint256_t uint512_t::tail256(int n) {
  uint256_t ret;
  int shiftNum = (256 - n);
  ret = shiftNum == 0 ? u256_0 : ((u256_0 << shiftNum) >> shiftNum);
  return ret;
}

uint256_t uint512_t::bits256(int hi, int lo) {
  uint256_t ret = 0;

  if (lo < 256) {
    ret = u256_0 >> lo;
  }
  if (hi < 256) {
    int shiftNum =  256 - (hi - lo + 1);
    ret = (ret << shiftNum) >> shiftNum;
  } else if (hi >= 256) {
    int shiftNum = 512 - hi - 1 - (256 - lo); // 256 + lo - hi
    ret |= (u256_1 << (512 - hi - 1)) >> shiftNum;
  }

  // if (lo > 256) ret = ret >> (lo - 256);
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
