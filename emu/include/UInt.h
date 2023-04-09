#ifndef UINT_H
#define UINT_H
#include <cstdint>
#include "common.h"

#define LOW32(x) ((uint64_t)((uint32_t)x))
#define HIGH32(x) (x >> 32)

template <int width, int dataNum = (width + 63)/ 64>
class UInt {
public:
  // int dataNum = (width + 63) / 64;
  int _dataNum = dataNum;
  int padBits = (width % 64 == 0) ? 0 : 64 - width % 64;
  uint64_t data[dataNum];
  UInt() {
    for(int i = 0; i < dataNum; i++) data[i] = 0;
  }
  UInt(int val) {
    data[0] = val;
    for(int i = 1; i < dataNum; i++) data[i] = 0;
  }
  UInt(uint64_t val) {
    data[0] = val;
    for(int i = 1; i < dataNum; i++) data[i] = 0;
  }
  UInt(std::vector<uint64_t>& vec) {
    for(int i = 0; i < MIN(dataNum, vec.size()); i++) data[i] = vec[i];
    for(int i = vec.size(); i < dataNum; i ++) data[i] = 0;
  }
  UInt(uint64_t* vec) {
    for(int i = 0; i < dataNum; i++) data[i] = vec[i];
    zeroExtend();
  }

  void zeroExtend() {
    data[dataNum - 1] = (data[dataNum - 1] << padBits) >> padBits;
  }

  template<int w>
  UInt<width + 1> operator+(UInt<w>& right) {
    UInt<width + 1> ret;
    int carry = 0;
    for (int i = 0; i < dataNum; i++) {
      ret.data[i] = data[i] + right.data[i] + carry;
      carry = (ret.data[i] < data[i]) || (carry && (ret.data[i] == data[i]));
    }
    return ret;
  }

  template<int w>
  UInt<width + 1> operator-(UInt<w>& right) {
    UInt<width + 1>ret;
    int carry = 1;
    for(int i = 0; i < dataNum; i++) {
      ret.data[i] = data[i] + (~right.data[i]) + carry;
      carry = (ret.data[i] < data[i]) || (carry && (ret.data[i] == data[i]));
    }
    return ret;
  }

  template<int w>
  UInt<width + w> operator*(UInt<w>& right) {
    UInt<width + w> ret(0);
    uint64_t carry = 0;
    for (int i = 0; i < dataNum; i++) {
      uint64_t carry = 0;
      for (int j = 0; j < right._dataNum; j++) {
        uint64_t mul_ll = LOW32(data[i]) * LOW32(right.data[i]);
        uint64_t mul_lh = LOW32(data[i]) * HIGH32(right.data[i]);
        uint64_t mul_hl = HIGH32(data[i]) * LOW32(right.data[i]);
        uint64_t mul_hh = HIGH32(data[i]) * HIGH32(right.data[i]);
        uint64_t low = LOW32(mul_ll) + LOW32(ret.data[i+j]) + LOW32(carry);
        uint64_t mid = HIGH32(ret.data[i+j]) + HIGH32(carry)+ LOW32(mul_lh) + LOW32(mul_hl) + HIGH32(mul_ll) + HIGH32(low);
        carry = mul_hh + HIGH32(mul_lh) + HIGH32(mul_hl) + HIGH32(mid);
        ret.data[i+j] = mid << 32 + LOW32(low);
      }
      if(carry) ret.data[i+right._dataNum] += carry;
    }
  }

  template<int w>
  UInt<width> operator/(UInt<w>& right) {
    if(width <= 64) return UInt<width>(data[0] / right.data[0]);
    else TODO();
    return right;
  }

  template<int w>
  UInt<width> operator%(UInt<w>& right) {
    if(width <= 64) return UInt<width>(data[0] % right.data[0]);
    else TODO();
    return right;
  }

  template<int w>
  UInt<1> operator==(UInt<w>& right) {
    int leftIdx = dataNum - 1;
    int rightIdx = right._dataNum - 1;
    while(leftIdx < rightIdx){
      if(!right.data[rightIdx -- ]) return UInt<1>(false);
    }
    while(leftIdx > rightIdx) {
      if(!data[leftIdx --]) return UInt<1>(false);
    }
    while(leftIdx >= 0) {
      if(data[leftIdx --] != right.data[rightIdx --]) return UInt<1>(false);
    }
    return UInt<1>(true);
  }

  UInt<1> operator!=(UInt<width>& right) {
    return UInt<1>(1) - (*this == right);
  }

  template<int w>
  UInt<1> operator>(UInt<width>& right) {
    int leftIdx = dataNum - 1;
    int rightIdx = right._dataNum - 1;
    while(leftIdx < rightIdx){
      if(right.data[rightIdx -- ]) return UInt<1>(false);
    }
    while(leftIdx > rightIdx) {
      if(data[leftIdx --]) return UInt<1>(true);
    }
    while(leftIdx >= 0) {
      if(data[leftIdx] > right.data[rightIdx]) return UInt<1>(true);
      else if(data[leftIdx--] < right.data[rightIdx--]) return UInt<1>(false);
    }
    return UInt<1>(false);
  }

  template<int w>
  UInt<width-w> tail() {
    return UInt <width-w>(data);
  }

  // operator bool() {
  //   return data[0] != 0;
  // }
  operator uint64_t() {
    return data[0];
  }

};

#endif