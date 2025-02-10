#include <stdio.h>
#include "uint.h"

#if SUPPORT_U256

#else
uint256_t operator | (uint128_t a, uint256_t b) {
  uint256_t ret;
  ret.u128_1 = b.u128_1;
  ret.u128_0 = a | b.u128_0;
  return ret;
}
uint256_t operator | (uint64_t a, uint256_t b) {
  uint256_t ret;
  ret.u128_1 = b.u128_1;
  ret.u128_0 = a | b.u128_0;
  return ret;
}
uint128_t uint256_t::tail128(int n) {
  uint128_t ret;
  int shiftNum = (128 - n);
  ret = (u128_0 << shiftNum) >> shiftNum;
  return ret;
}
uint256_t uint256_t::tail256(int n) {
  uint256_t ret;
  int shiftNum = (256 - n);
  ret = shiftNum == 0 ? *this : ((*this << shiftNum) >> shiftNum);
  return ret;
}
uint128_t uint256_t::bits128(int hi, int lo) {
  uint128_t ret = bits256(hi, lo).u128_0;
  return ret;
}
uint256_t uint256_t::bits256(int hi, int lo) {
  uint256_t ret = 0;

  ret = *this >> lo;
  int shiftNum = 256 - (hi - lo + 1);
  ret = (ret << shiftNum) >> shiftNum;
  return ret;
}

uint128_t int256_t::tail128(int n) {
  uint128_t ret;
  int shiftNum = (128 - n);
  ret = (u128_0 << shiftNum) >> shiftNum;
  return ret;
}

uint256_t int256_t::tail256(int n) {
    uint256_t ret;
    ret.u128_0 = u128_0;
    ret.u128_1 = s128_1;
    int shiftNum = (256 - n);
    ret = (ret << shiftNum) >> shiftNum;
    return ret;
}

uint256_t uint256_t::flip() {
  uint256_t ret;
  ret.u128_0 = ~u128_0;
  ret.u128_1 = ~u128_1;
  return ret;
}

uint256_t::operator int256_t() {
  int256_t ret;
  ret.u128_0 = u128_0;
  ret.s128_1 = u128_1;
  return ret;
}

uint256_t::uint256_t(int256_t val) {
  u128_0 = val.u128_0;
  u128_1 = val.s128_1;
}

#endif

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

uint128_t uint512_t::tail128(int n) {
  uint128_t ret;
  int shiftNum = (128 - n);
  ret = ((uint128_t)u256_0 << shiftNum) >> shiftNum;
  return ret;
}

uint256_t uint512_t::tail256(int n) {
  uint256_t ret;
  int shiftNum = (256 - n);
  ret = shiftNum == 0 ? u256_0 : ((u256_0 << shiftNum) >> shiftNum);
  return ret;
}

uint128_t uint512_t::bits128(int hi, int lo) {
  return (uint128_t)bits256(hi, lo);
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

  return ret;
}

uint512_t __attribute__ ((noinline)) uint512_t::bits512(int hi, int lo) {
  uint512_t ret = *this;
  /* clear high */
  int lshiftBits = 512 - hi - 1;
  int rshiftBits = lshiftBits + lo;
  if(lshiftBits == 0) ret = *this;
  else ret = *this << lshiftBits;
  if (rshiftBits != 0) ret = ret >> rshiftBits;
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
