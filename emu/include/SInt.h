#ifndef SINT_H
#define SINT_H

#include <cstdint>
#include "common.h"

#define sint64_t long long
#define LOW32(x) ((uint64_t)((uint32_t)x))
#define HIGH32(x) (x >> 32)
#define SIGN(x) ((x & 0x8000000000000000) >> 63)
// #define VAL(x, w) (x & ((1 << (w-1)) - 1))

template <int width, int dataNum = (width + 63) / 64>
class SInt {
public:
  // uint64_t mask = ~(1 << (width % 64))
  // int sign = 0;
  int padBits = (width % 64 == 0) ? 0 : 64 - width % 64;
  uint64_t data[dataNum];
  SInt() {
    for(int i = 0; i < dataNum; i++) data[i] = 0;
  }
  SInt(uint64_t val) {
    data[0] = val;
    for(int i = 1; i < dataNum; i++) data[i] = 0;
    signExtend();
  }
  SInt(std::vector<uint64_t>& vec) {
    for(int i = 0; i < MIN(dataNum, vec.size()); i++) data[i] = vec[i];
    for(int i = vec.size(); i < dataNum; i ++) data[i] = 0;
    signExtend();
  }

  SInt(SInt<width>& s) {
    for(int i = 0; i < dataNum; i++) data[i] = s.data[i];
  }

  bool sign() {
    return SIGN(data[dataNum-1]);
  }

  void signExtend() {
    data[dataNum - 1] = ((sint64_t)data[dataNum - 1] << padBits) >> padBits;
  }

  void zeroExtend() {
    data[dataNum - 1] = (data[dataNum - 1] << padBits) >> padBits;
  }

  template<int w>
  SInt<width + 1> operator+(SInt<w>& right) {
    SInt<width + 1> ret;
    int carry = 0;
    for (int i = 0; i < dataNum; i++) {
      ret.data[i] = data[i] + right.data[i] + carry;
      carry = (ret.data[i] < data[i]) || (carry && (ret.data[i] == data[i]));
    }
    return ret;
  }

  template<int w>
  SInt<width + 1> operator-(SInt<w>& right) {
    SInt<width + 1>ret;
    int carry = 1;
    for(int i = 0; i < dataNum; i++) {
      ret.data[i] = data[i] + (~right.data[i]) + carry;
      carry = (ret.data[i] < data[i]) || (carry && (ret.data[i] == data[i]));
    }
    return ret;
  }

  template<int w>
  SInt<width + w> operator*(SInt<w>& right) {
    SInt<width + w> ret(0);
    SInt<width> _left = sign() ? SInt<width>(0) - *this : *this;
    SInt<w> _right = right.sign() ? SInt<width>(0) - right : right;
    uint64_t carry = 0;
    for (int i = 0; i < dataNum; i++) {
      uint64_t carry = 0;
      for (int j = 0; j < right.dataNum; j++) {
        uint64_t mul_ll = LOW32(_left.data[i]) * LOW32(_right.data[i]);
        uint64_t mul_lh = LOW32(_left.data[i]) * HIGH32(_right.data[i]);
        uint64_t mul_hl = HIGH32(_left.data[i]) * LOW32(_right.data[i]);
        uint64_t mul_hh = HIGH32(_left.data[i]) * HIGH32(_right.data[i]);
        uint64_t low = LOW32(mul_ll) + LOW32(ret.data[i+j]) + LOW32(carry);
        uint64_t mid = HIGH32(ret.data[i+j]) + HIGH32(carry)+ LOW32(mul_lh) + LOW32(mul_hl) + HIGH32(mul_ll) + HIGH32(low);
        carry = mul_hh + HIGH32(mul_lh) + HIGH32(mul_hl) + HIGH32(mid);
        ret.data[i+j] = mid << 32 + LOW32(low);
      }
      if(carry) ret.data[i+right.dataNum] += carry;
    }
    zeroExtend();
    if(sign() ^ right.sign()) ret = SInt<width+w>(0) - ret;
    return ret;
  }

  template<int w>
  SInt<width> operator/(SInt<w>& right) {
    if(width <= 64) return SInt<width>((sint64_t)data[0] / (sint64_t)right.data[0]);
    else TODO();
    return right;
  }

  template<int w>
  SInt<width> operator%(SInt<w>& right) {
    if(width <= 64) return SInt<width>((sint64_t)data[0] % (sint64_t)right.data[0]);
    else TODO();
    return right;
  }
};

#endif
