#ifndef UINT_H
#define UINT_H
#include <sys/types.h>
#include <cstdint>
#include <cstring>
#define uint128_t __uint128_t
#define int128_t __int128_t

class uint512_t;
class uint1024_t;
class uint2048_t;
template<int _dataNum> class wide_t;

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)

template<int A, int B>
struct max_of {
    static constexpr auto value = A > B ? A : B;
};

template<int A, int B>
struct min_of {
    static constexpr auto value  = A < B ? A : B;
};

#define SUPPORT_U256 true
#if SUPPORT_U256
#define uint256_t unsigned _BitInt(256)
#define int256_t _BitInt(256)
#else
class int256_t;
class uint256_t {
public:
  uint128_t u128_1;
  uint128_t u128_0;
  uint256_t() {}
  uint256_t(int val) {
    u128_0 = val;
    u128_1 = 0;
  }
  uint256_t(uint32_t val) {
    u128_0 = val;
    u128_1 = 0;
  }
  uint256_t(uint64_t val) {
    u128_0 = val;
    u128_1 = 0;
  }
  uint256_t(uint128_t val) {
    u128_0 = val;
    u128_1 = 0;
  }
  void operator = (int data) {
    u128_0 = data;
    u128_1 = 0;
  }
  void operator = (uint32_t data) {
    u128_0 = data;
    u128_1 = 0;
  }
  void operator = (long data) {
    u128_0 = data;
    u128_1 = 0;
  }
  void operator = (uint64_t data) {
    u128_0 = data;
    u128_1 = 0;
  }
  void operator = (uint128_t data) {
    u128_0 = data;
    u128_1 = 0;
  }
  bool operator == (uint256_t b) {
    return u128_1 == b.u128_1 && u128_0 == b.u128_0;
  }
  bool operator != (int b) {
    return u128_1 != 0 || u128_0 != b;
  }
  bool operator != (uint256_t b) {
    return u128_1 != b.u128_1 || u128_0 != b.u128_0;
  }
  uint256_t operator << (int shiftNum) {
    uint256_t ret;
    if (shiftNum == 0) {
      ret.u128_0 = u128_0;
      ret.u128_1 = u128_1;
      return ret;
    }
    ret.u128_0 = shiftNum >= 128 ? 0 : (u128_0 << shiftNum);
    ret.u128_1 = shiftNum >= 256 ? 0 :
                  (shiftNum >= 128 ?
                    (u128_0 << (shiftNum - 128)) :
                    (u128_1 << shiftNum) | (u128_0 >> (128 - shiftNum)));
    return ret;
  }
  uint256_t operator >> (uint32_t shiftNum) {
    return *this >> (int)shiftNum;
  }
  uint256_t operator >> (int shiftNum) {
    uint256_t ret;
    if (shiftNum == 0) {
      ret.u128_0 = u128_0;
      ret.u128_1 = u128_1;
      return ret;
    }
    ret.u128_1 = shiftNum >= 128 ? 0 : (u128_1 >> shiftNum);
    ret.u128_0 = shiftNum >= 256 ? 0 :
                  (shiftNum >= 128 ?
                    (u128_1 >> (shiftNum - 128)) :
                    (u128_0 >> shiftNum) | (u128_1 << (128 - shiftNum)));
    return ret;
  }
  int operator & (int a) {
    return u128_0 & a;
  }
  uint32_t operator & (uint32_t a) {
    return u128_0 & a;
  }
  uint64_t operator & (uint64_t a) {
    return u128_0 & a;
  }
  uint64_t operator & (long a) {
    return u128_0 & a;
  }
  uint128_t operator & (uint128_t a) {
    return u128_0 & a;
  }
  uint256_t operator & (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 & a.u128_1;
    ret.u128_0 = u128_0 & a.u128_0;
    return ret;
  }
  void operator |= (uint256_t a) {
    u128_1 |= a.u128_1;
    u128_0 |= a.u128_0;
  }
  uint256_t operator | (uint8_t a) {
    uint256_t ret;
    ret.u128_1 = 0;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (int a) {
    uint256_t ret;
    ret.u128_1 = u128_1;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (uint32_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (uint64_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (uint128_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 | a.u128_1;
    ret.u128_0 = u128_0 | a.u128_0;
    return ret;
  }
  friend uint256_t operator | (uint128_t a, uint256_t b);
  friend uint256_t operator | (uint64_t a, uint256_t b);
  uint256_t operator ^ (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 ^ a.u128_1;
    ret.u128_0 = u128_0 ^ a.u128_0;
    return ret;
  }
  uint256_t operator ~() {
    uint256_t ret;
    ret.u128_1 = ~u128_1;
    ret.u128_0 = ~u128_0;
    return ret;
  }
  uint256_t operator +(uint256_t val) {
    uint256_t ret;
    ret.u128_0 = u128_0 + val.u128_0;
    ret.u128_1 = u128_1 + val.u128_1 + (ret.u128_0 < u128_0);
    return ret;
    
  }
  operator bool()      { return u128_0 || u128_1; }
  operator uint8_t()   { return (uint8_t)u128_0; }
  operator int()       { return (int)u128_0; }
  operator uint32_t()  { return (uint32_t)u128_0; }
  operator uint64_t()  { return (uint64_t)u128_0; }
  operator uint128_t() { return u128_0; }
  operator int256_t();
  uint128_t tail128(int n);
  uint256_t tail256(int n);
  uint128_t bits128(int hi, int lo);
  uint256_t bits256(int hi, int lo);
  uint256_t flip();
  void display() {
    printf("%lx %lx %lx %lx", (uint64_t)(u128_1 >> 64), (uint64_t)u128_1, (uint64_t)(u128_0 >> 64), (uint64_t)u128_0);
  }
  void displayn() {
    display();
    printf("\n");
  }
};

class int256_t {
public:
  int128_t s128_1;
  uint128_t u128_0;
  int256_t() {}
  int256_t(long val) {
    u128_0 = val;
    s128_1 = 0;
  }
  int256_t(uint64_t val) {
    u128_0 = val;
    s128_1 = 0;
  }
  int256_t(int128_t val) {
    u128_0 = val;
    s128_1 = val < 0 ? -1 : 0;
  }
  void operator = (uint128_t data) {
    u128_0 = data;
    s128_1 = 0;
  }
  void operator = (uint64_t data) {
    u128_0 = data;
    s128_1 = 0;
  }
  bool operator == (uint256_t b) {
    return s128_1 == b.u128_1 && u128_0 == b.u128_0;
  }
  bool operator != (uint256_t b) {
    return s128_1 != b.u128_1 || u128_0 != b.u128_0;
  }
  int256_t operator << (int shiftNum) {
    int256_t ret;
    if (shiftNum == 0) {
      ret.u128_0 = u128_0;
      ret.s128_1 = s128_1;
      return ret;
    }
    ret.u128_0 = shiftNum >= 128 ? (uint128_t)0 : (u128_0 << shiftNum);
    ret.s128_1 = shiftNum >= 256 ? (uint128_t)0 :
                  (shiftNum >= 128 ?
                    (u128_0 << (shiftNum - 128)) :
                    (s128_1 << shiftNum) | (u128_0 >> (128 - shiftNum)));
    return ret;
  }
  int256_t operator >> (uint32_t shiftNum) {
    return *this >> (int)shiftNum;
  }
  int256_t operator >> (int shiftNum) {
    int256_t ret;
    if (shiftNum == 0) {
      ret.u128_0 = u128_0;
      ret.s128_1 = s128_1;
      return ret;
    }
    ret.s128_1 = shiftNum >= 128 ? 0 : (s128_1 >> shiftNum);
    ret.u128_0 = shiftNum >= 256 ? 0 :
                  (shiftNum >= 128 ?
                    (s128_1 >> (shiftNum - 128)) :
                    (u128_0 >> shiftNum) | (s128_1 << (128 - shiftNum)));
    return ret;
  }
  uint64_t operator & (uint64_t a) {
    return u128_0 & a;
  }
  uint128_t operator & (uint128_t a) {
    return u128_0 & a;
  }
  uint256_t operator & (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = s128_1 & a.u128_1;
    ret.u128_0 = u128_0 & a.u128_0;
    return ret;
  }
  uint256_t operator | (uint128_t a) {
    uint256_t ret;
    ret.u128_1 = s128_1;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator | (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = s128_1 | a.u128_1;
    ret.u128_0 = u128_0 | a.u128_0;
    return ret;
  }
  uint256_t operator | (uint8_t a) {
    uint256_t ret;
    ret.u128_1 = 0;
    ret.u128_0 = u128_0 | a;
    return ret;
  }
  uint256_t operator ^ (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = s128_1 ^ a.u128_1;
    ret.u128_0 = u128_0 ^ a.u128_0;
    return ret;
  }
  int256_t operator * (int128_t a) {
    uint64_t result[6];
    uint64_t val1[4];
    uint64_t val2[2];
    uint128_t u0, u1;
    bool neg = false;
    if (s128_1 < 0) {
      u0 = ~u128_0 + 1;
      u1 = ~s128_1 + (u0 == 0);
      neg = !neg;
    } else {
      u0 = u128_0;
      u1 = s128_1;
    }
    if (a < 0) {
      a = -a;
      neg = !neg;
    }
    val1[0] = (uint64_t)u0;
    val1[1] = (uint64_t)(u0 >> 64);
    val1[2] = (uint64_t)u1;
    val1[3] = (uint64_t)(u1 >> 64);
    val2[0] = (uint64_t)a;
    val2[1] = (uint64_t)(a >> 64);

    for (int i = 0; i < 6; i++) {
      result[i] = 0;
    }

    for (int i = 0; i < 4; i ++) {
      for (int j = 0; j < 2; j ++) {
        uint128_t mulVal = (uint128_t)val1[i] * (uint128_t)val2[j];
        result[i + j] += (uint64_t)mulVal;
        result[i + j + 1] += (uint64_t)(mulVal >> 64);
      }
    }
    if (neg) {
      for (int i = 0; i < 6; i ++) result[i] = ~result[i];
      for (int i = 0; i < 6; i ++) {
        result[i] += 1;
        if (result[i] != 0) break;
      }
    }
    int256_t ret;
    ret.u128_0 = result[0] | ((uint128_t)result[1] << 64);
    ret.s128_1 = result[2] | ((uint128_t)result[3] << 64);
    return ret;
  }
  uint128_t tail128(int n);
  uint256_t tail256(int n);
  operator int() {
    return (int)u128_0;
  }
  operator uint64_t() {
    return (uint64_t)u128_0;
  }
  operator uint128_t() {
    return u128_0;
  }
  void display() {
    printf("%lx %lx %lx %lx", (uint64_t)(s128_1 >> 64), (uint64_t)s128_1, (uint64_t)(u128_0 >> 64), (uint64_t)u128_0);
  }
  void displayn() {
    display();
    printf("\n");
  }
};
#endif

class uint512_t {
public:
  uint256_t u256_1;
  uint256_t u256_0;
  uint512_t() {
    u256_0 = 0;
    u256_1 = 0;
  }
  uint512_t(int val) {
    u256_0 = val;
    u256_1 = 0;
  }
  uint512_t(uint32_t val) {
    u256_0 = val;
    u256_1 = 0;
  }
  uint512_t(uint64_t val) {
    u256_0 = val;
    u256_1 = 0;
  }
  uint512_t(long val) {
    u256_0 = val;
    u256_1 = 0;
  }
  uint512_t(uint128_t val) {
    u256_0 = val;
    u256_1 = 0;
  }
  uint512_t(uint256_t val) {
    u256_0 = val;
    u256_1 = 0;
  }
  void operator = (uint64_t data) {
    u256_0 = data;
    u256_1 = 0;
  }
  void operator = (uint512_t data) {
    u256_1 = data.u256_1;
    u256_0 = data.u256_0;
  }
  void operator = (uint1024_t data);
  template<int num> void operator = (wide_t<num> a) {
    u256_0 = (uint256_t)a.data[3] << 192 | (uint256_t)a.data[2] << 128 | (uint256_t)a.data[1] << 64 | (uint256_t)a.data[0];
    u256_1 = (uint256_t)a.data[7] << 192 | (uint256_t)a.data[6] << 128 | (uint256_t)a.data[5] << 64 | (uint256_t)a.data[4];
  }
  bool operator == (uint512_t b) {
    return u256_1 == b.u256_1 && u256_0 == b.u256_0;
  }
  bool operator != (uint512_t b) {
    return u256_1 != b.u256_1 || u256_0 != b.u256_0;
  }
  bool operator != (int b) {
    return u256_1 != 0 || u256_0 != b;;
  }
  uint512_t operator << (int shiftNum) {
    uint512_t ret;
#if SUPPORT_U256
    if (shiftNum == 0) {
      ret.u256_0 = u256_0;
      ret.u256_1 = u256_1;
      return ret;
    }
    ret.u256_0 = shiftNum >= 256 ? (uint256_t)0 : (u256_0 << shiftNum);
    ret.u256_1 = shiftNum >= 512 ? 0 :
                  (shiftNum >= 256 ?
                    (u256_0 << (shiftNum - 256)) :
                    (u256_1 << shiftNum) | (u256_0 >> (256 - shiftNum)));
#else
  ret.u256_0 = u256_0 << shiftNum;
  ret.u256_1 = (u256_1 << shiftNum) | (shiftNum >= 256 ? (u256_0 << (shiftNum - 256)) : (u256_0 >> (256 - shiftNum)));
#endif
    return ret;
  }
  uint512_t operator << (uint64_t shiftNum) {
    return *this << (int)shiftNum;
  }
  uint512_t operator >> (int shiftNum) {
    uint512_t ret;
    if (shiftNum == 0) {
      ret.u256_0 = u256_0;
      ret.u256_1 = u256_1;
      return ret;
    }
    ret.u256_1 = shiftNum >= 256 ? (uint256_t)0 : (u256_1 >> shiftNum);
    ret.u256_0 = shiftNum >= 512 ? (uint256_t)0 :
                  (shiftNum >= 256 ?
                    (u256_1 >> (shiftNum - 256)) :
                    (u256_0 >> shiftNum) | (u256_1 << (256 - shiftNum)));
    return ret;
  }
  uint512_t operator >> (uint32_t shiftNum) {
    return *this >> (int)shiftNum;
  }
  uint512_t operator & (uint512_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1 & a.u256_1;
    ret.u256_0 = u256_0 & a.u256_0;
    return ret;
  }
  uint64_t operator & (int a) {
    return (int)u256_0 & a;
  }
  uint64_t operator & (uint64_t a) {
    return (uint64_t)u256_0 & a;
  }
  uint64_t operator & (long a) {
    return (uint64_t)u256_0 & a;
  }
  uint128_t operator & (uint128_t a) {
    return u256_0 & a;
  }
  uint256_t operator & (uint256_t a) {
    return u256_0 & a;
  }
  uint512_t operator | (uint32_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1;
    ret.u256_0 = u256_0 | a;
    return ret;
  }
  uint512_t operator | (uint64_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1;
    ret.u256_0 = u256_0 | a;
    return ret;
  }
  uint512_t operator | (uint128_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1;
    ret.u256_0 = u256_0 | a;
    return ret;
  }
  uint512_t operator | (uint256_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1;
    ret.u256_0 = u256_0 | a;
    return ret;
  }
  uint512_t operator | (uint512_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1 | a.u256_1;
    ret.u256_0 = u256_0 | a.u256_0;
    return ret;
  }
  void operator |= (uint512_t a) {
    u256_1 |= a.u256_1;
    u256_0 |= a.u256_0;
  }
  uint512_t operator ^ (uint512_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1 ^ a.u256_1;
    ret.u256_0 = u256_0 ^ a.u256_0;
    return ret;
  }
  operator int () {
    return (int)u256_0;
  }
  operator uint64_t () {
    return (uint64_t)u256_0;
  }
  operator uint128_t () {
    return (uint128_t)u256_0;
  }
  operator uint256_t () {
    return u256_0;
  }
  uint128_t tail128(int n);
  uint256_t tail256(int n);
  uint512_t tail512(int n) {return tail(n);}
  uint512_t tail(int n);
  uint128_t bits128(int hi, int lo);
  uint256_t bits256(int hi, int lo);
  uint512_t flip();
  void display() {
    printf("%lx %lx %lx %lx %lx %lx %lx %lx",
     (uint64_t)(u256_1 >> 192), (uint64_t)(u256_1 >> 128), (uint64_t)(u256_1 >> 64), (uint64_t)u256_1, (uint64_t)(u256_0 >> 192), (uint64_t)(u256_0 >> 128), (uint64_t)(u256_0 >> 64), (uint64_t)u256_0);
  }
  void displayn() {
    display();
    printf("\n");
  }
};


class uint1024_t {
public:
  union{
    uint512_t u512_1;
  };
  union {
    uint512_t u512_0;
  };
  uint1024_t() {}
  uint1024_t(int val) {
    u512_0 = val;
    u512_1 = 0;
  }
  uint1024_t(uint64_t val) {
    u512_0 = val;
    u512_1 = 0;
  }
  uint1024_t(uint128_t val) {
    u512_0 = val;
    u512_1 = 0;
  }
  uint1024_t(uint512_t val) {
    u512_0 = val;
    u512_1 = 0;
  }
  uint1024_t(uint64_t* val) { // construct from array
    u512_0 = 0;
    u512_1 = 0;
    for (int i = 0; i < 8; i ++) u512_0 = u512_0 | (uint512_t)val[i] << (i * 64);
    for (int i = 0; i < 8; i ++) u512_1 = u512_1 | (uint512_t)val[i + 8] << (i * 64);
  }
  void operator = (uint64_t data) {
    u512_0 = data;
    u512_1 = 0;
  }
  void operator = (uint1024_t data) {
    u512_1 = data.u512_1;
    u512_0 = data.u512_0;
  }
  bool operator == (uint1024_t b) {
    return u512_1 == b.u512_1 && u512_0 == b.u512_0;
  }
  bool operator != (uint1024_t b) {
    return u512_1 != b.u512_1 || u512_0 != b.u512_0;
  }
  uint1024_t operator << (int shiftNum) {
    uint1024_t ret;
    if (shiftNum == 0) {
      ret.u512_0 = u512_0;
      ret.u512_1 = u512_1;
      return ret;
    }
    ret.u512_0 = u512_0 << shiftNum;
    ret.u512_1 = (u512_1 << shiftNum) | (shiftNum >= 512 ? (u512_0 << (shiftNum - 512)) : (u512_0 >> (512 - shiftNum)));
    return ret;
  }
  uint1024_t operator >> (int shiftNum) {
    uint1024_t ret;
    ret.u512_1 = shiftNum >= 512 ? (uint512_t)0 : (u512_1 >> shiftNum);
    ret.u512_0 = shiftNum >= 1024 ? (uint512_t)0 :
                  (shiftNum >= 512 ?
                    (u512_1 >> (shiftNum - 512)) :
                    (u512_0 >> shiftNum) | (u512_1 << (512 - shiftNum)));
    return ret;
  }
  int operator & (int a) {
    return u512_0.u256_0 & a;
  }
  uint64_t operator & (uint64_t a) {
    return u512_0.u256_0 & a;
  }
  uint512_t operator & (uint512_t a) {
    return u512_0 & a;
  }
  uint1024_t operator & (uint1024_t a) {
    uint1024_t ret;
    ret.u512_1 = u512_1 & a.u512_1;
    ret.u512_0 = u512_0 & a.u512_0;
    return ret;
  }
  uint1024_t operator | (uint512_t a) {
    uint1024_t ret;
    ret.u512_1 = u512_1;
    ret.u512_0 = u512_0 | a;
    return ret;
  }
  uint1024_t operator | (uint1024_t a) {
    uint1024_t ret;
    ret.u512_1 = u512_1 | a.u512_1;
    ret.u512_0 = u512_0 | a.u512_0;
    return ret;
  }
  uint1024_t operator ^ (uint1024_t a) {
    uint1024_t ret;
    ret.u512_1 = u512_1 ^ a.u512_1;
    ret.u512_0 = u512_0 ^ a.u512_0;
    return ret;
  }
  operator uint64_t () {
    return (uint64_t)u512_0.u256_0;
  }
  operator uint128_t () {
    return (uint128_t)u512_0.u256_0;
  }
  operator uint256_t() {
    return (uint256_t)u512_0.u256_0;
  }
  operator uint512_t() {
    return u512_0;
  }
  uint1024_t tail(int n);
  uint1024_t flip();
  void display() {
    u512_1.display();
    printf(" ");
    u512_0.display();
  }
  void displayn() {
    display();
    printf("\n");
  }
};

class uint2048_t {
public:
  union{
    uint1024_t u1024_1;
  };
  union {
    uint1024_t u1024_0;
  };
  uint2048_t() {}
  uint2048_t(int val) {
    u1024_0 = val;
    u1024_1 = 0;
  }
  uint2048_t(uint64_t val) {
    u1024_0 = val;
    u1024_1 = 0;
  }
  uint2048_t(uint256_t val) {
    u1024_0 = val;
    u1024_1 = 0;
  }
  uint2048_t(uint1024_t val) {
    u1024_0 = val;
    u1024_1 = 0;
  }
  uint2048_t(uint64_t* val) { // construct from array
    u1024_0 = 0;
    u1024_1 = 0;
    for (int i = 0; i < 16; i ++) u1024_0 = u1024_0 | (uint1024_t)val[i] << (i * 64);
    for (int i = 0; i < 16; i ++) u1024_1 = u1024_1 | (uint1024_t)val[i + 16] << (i * 64);
  }
  void operator = (uint64_t data) {
    u1024_0 = data;
    u1024_1 = 0;
  }
  void operator = (uint2048_t data) {
    u1024_1 = data.u1024_1;
    u1024_0 = data.u1024_0;
  }
  bool operator == (uint2048_t b) {
    return u1024_1 == b.u1024_1 && u1024_0 == b.u1024_0;
  }
  bool operator != (uint2048_t b) {
    return u1024_1 != b.u1024_1 || u1024_0 != b.u1024_0;
  }
  uint2048_t operator << (int shiftNum) {
    uint2048_t ret;
    if (shiftNum == 0) {
      ret.u1024_0 = u1024_0;
      ret.u1024_1 = u1024_1;
      return ret;
    }
    ret.u1024_0 = u1024_0 << shiftNum;
    ret.u1024_1 = (u1024_1 << shiftNum) | (shiftNum >= 1024 ? (u1024_0 << (shiftNum - 1024)) : (u1024_0 >> (1024 - shiftNum)));
    return ret;
  }
  uint2048_t operator >> (int shiftNum) {
    uint2048_t ret;
    ret.u1024_1 = shiftNum >= 1024 ? (uint1024_t)0 : (u1024_1 >> shiftNum);
    ret.u1024_0 = shiftNum >= 2048 ? (uint1024_t)0 :
                  (shiftNum >= 1024 ?
                    (u1024_1 >> (shiftNum - 1024)) :
                    (u1024_0 >> shiftNum) | (u1024_1 << (1024 - shiftNum)));
    return ret;
  }
  uint64_t operator & (int a) {
    return (uint64_t)u1024_0.u512_0.u256_0 & a;
  }
  uint64_t operator & (uint64_t a) {
    return (uint64_t)u1024_0.u512_0.u256_0 & a;
  }
  uint128_t operator & (uint128_t a) {
    return (uint128_t)u1024_0.u512_0.u256_0 & a;
  }
  uint2048_t operator & (uint2048_t a) {
    uint2048_t ret;
    ret.u1024_1 = u1024_1 & a.u1024_1;
    ret.u1024_0 = u1024_0 & a.u1024_0;
    return ret;
  }
  uint2048_t operator | (uint2048_t a) {
    uint2048_t ret;
    ret.u1024_1 = u1024_1 | a.u1024_1;
    ret.u1024_0 = u1024_0 | a.u1024_0;
    return ret;
  }
  uint2048_t operator ^ (uint2048_t a) {
    uint2048_t ret;
    ret.u1024_1 = u1024_1 ^ a.u1024_1;
    ret.u1024_0 = u1024_0 ^ a.u1024_0;
    return ret;
  }
  operator uint64_t () {
    return (uint64_t)u1024_0.u512_0.u256_0;
  }
  operator uint128_t () {
    return (uint128_t)u1024_0.u512_0.u256_0;
  }
  operator uint256_t () {
    return u1024_0.u512_0.u256_0;
  }
  operator uint512_t () {
    return u1024_0.u512_0;
  }
  operator uint1024_t () {
    return u1024_0;
  }
  uint2048_t tail(int n);
  uint2048_t flip();
  void display() {
    u1024_1.display();
    printf(" ");
    u1024_0.display();
  }
  void displayn() {
    display();
    printf("\n");
  }
};

template<int _dataNum> class wide_t {
public:
  uint64_t data[_dataNum]; // lo - hi
  int dataNum = _dataNum;
  wide_t() {
    memset(data, 0, sizeof(data));
  }
  wide_t(int val) {
    for (int i = 1; i < dataNum ; i++) data[i] = 0;
    data[0] = val;
  }
  wide_t(long val) {
    for (int i = 1; i < dataNum; i++) data[i] = 0;
    data[0] = val;
  }
  wide_t(uint64_t val) {
    for (int i = 1; i < dataNum; i++) data[i] = 0;
    data[0] = val;
  }
  wide_t(uint128_t val) {
    for (int i = 2; i < dataNum; i++) data[i] = 0;
    data[0] = val;
    data[1] = val >> 64;
  }
  wide_t(uint256_t val) {
    for (int i = 4; i < dataNum; i++) data[i] = 0;
    data[0] = val;
    data[1] = val >> 64;
    data[2] = val >> 128;
    data[3] = val >> 192;
  }
  wide_t(uint512_t val) {
    for (int i = 8; i < dataNum; i++) data[i] = 0; // TODO: optimize using pointer
    data[0] = val.u256_0;
    data[1] = val.u256_0 >> 64;
    data[2] = val.u256_0 >> 128;
    data[3] = val.u256_0 >> 192;
    data[4] = val.u256_1;
    data[5] = val.u256_1 >> 64;
    data[6] = val.u256_1 >> 128;
    data[7] = val.u256_1 >> 192;
  }

  wide_t(uint2048_t val) {
    for (int i = 32; i < dataNum; i++) data[i] = 0;
    for (int i = 0; i < 32; i ++) {
      data[i] = (uint64_t)(val >> (i * 64));
    }
  }
  wide_t<_dataNum> operator << (int shiftNum) {
    wide_t<_dataNum> ret;
    int full_shifts = shiftNum / 64;
    int bit_shift = shiftNum % 64;

    // fully shift
    memset(ret.data, 0, full_shifts * sizeof(uint64_t));
    memcpy(ret.data + full_shifts, data, (dataNum - full_shifts) * sizeof(uint64_t));

    // partial shift
    if (bit_shift > 0) {
      uint64_t carry = 0;
      for (int i = 0; i < dataNum; i ++) {
        uint64_t new_carry = ret.data[i] >> (64 - bit_shift);
        ret.data[i] = (ret.data[i] << bit_shift) | carry;
        carry = new_carry;
      }
    }
    return ret;
  }
  wide_t<_dataNum> operator << (uint64_t shiftNum) {
    return *this << (int)shiftNum;
  }
  wide_t<_dataNum> operator >> (int shiftNum) {
    wide_t<_dataNum> ret;
    int full_shifts = shiftNum / 64;
    int bit_shift = shiftNum % 64;

    // fully shift
    memcpy(ret.data, data + full_shifts, (dataNum - full_shifts) * sizeof(uint64_t));
    memset(ret.data + dataNum - full_shifts, 0, full_shifts * sizeof(uint64_t));

    // partial shift
    if (bit_shift > 0) {
      uint64_t carry = 0;
      for (int i = dataNum - 1; i >= 0; i --) {
        uint64_t new_carry = ret.data[i] << (64 - bit_shift);
        ret.data[i] = ret.data[i] >> bit_shift | carry;
        carry = new_carry;
      }
    }

    return ret;
  }
  template<int num1> wide_t<_dataNum> operator & (wide_t<num1> a) {
    wide_t<_dataNum> ret = 0;
    for (int i = 0; i < MIN(dataNum, num1); i++) {
      ret.data[i] = data[i] & a.data[i];
    }
    return ret;
  }
  uint64_t operator & (uint64_t a) {
    return data[0] & a;
  }
  uint64_t operator & (int a) {
    return data[0] & a;
  }
  uint512_t operator & (uint512_t a) {
    uint512_t ret;
    ret.u256_0 = (uint256_t)data[3] << 192 | (uint256_t)data[2] << 128 | (uint256_t)data[1] << 64 | (uint256_t)data[0];
    ret.u256_1 = (uint256_t)data[7] << 192 | (uint256_t)data[6] << 128 | (uint256_t)data[5] << 64 | (uint256_t)data[4];
    ret.u256_0 &= a.u256_0;
    ret.u256_1 &= a.u256_1;
    return ret;
  }
  template<int num1> wide_t<_dataNum> operator | (wide_t<num1> a) {
    wide_t<_dataNum> ret = *this;
    for (int i = 0; i < MIN(dataNum, num1); i++) {
      ret.data[i] = data[i] | a.data[i];
    }
    return ret;
  }
  wide_t<_dataNum> operator | (uint64_t a) {
    wide_t<_dataNum> ret = *this;
    ret.data[0] |= a;
    return ret;
  }
  wide_t<_dataNum> operator | (uint512_t a) {
    static_assert(_dataNum > 8, "width is not enough");
    wide_t<_dataNum> ret = *this;
    ret.data[0] = data[0] | (uint64_t)(a.u256_0); // TODO: optimize using pointer
    ret.data[1] = data[1] | (uint64_t)(a.u256_0 >> 64);
    ret.data[2] = data[2] | (uint64_t)(a.u256_0 >> 128);
    ret.data[3] = data[3] | (uint64_t)(a.u256_0 >> 192);
    ret.data[4] = data[4] | (uint64_t)(a.u256_1);
    ret.data[5] = data[5] | (uint64_t)(a.u256_1 >> 64);
    ret.data[6] = data[6] | (uint64_t)(a.u256_1 >> 128);
    ret.data[7] = data[7] | (uint64_t)(a.u256_1 >> 192);
    return ret;
  }
  template<int num1> wide_t<_dataNum> operator ^ (wide_t<num1> a) {
    wide_t<_dataNum> ret = *this;
    for (int i = 0; i < MIN(dataNum, num1); i++) {
      ret.data[i] = data[i] ^ a.data[i];
    }
    return ret;
  }
  template<int num> bool operator == (wide_t<num> a) {
    bool ret = true;
    for (int i = 0; i < dataNum; i++) {
      ret &= data[i] == a.data[i];
    }
    return ret;
  }
  bool operator == (int a) {
    bool ret = data[0] == a;
    for (int i = 1; i < dataNum; i++) {
      ret &= data[i] == 0;
    }
    return ret;
  }
  template<int num> bool operator != (wide_t<num> a) {
    bool ret = false;
    for (int i = 0; i < dataNum; i++) {
      ret |= data[i] != a.data[i];
    }
    return ret;
  }
  bool operator != (int a) {
    bool ret = data[0] != a;
    for (int i = 0; i < dataNum; i++) {
      ret |= data[i] != 0;
    }
    return ret;
  }
  operator int() {
    return data[0];
  }
  operator uint64_t() {
    return data[0];
  }
  operator uint128_t() {
    uint256_t ret = (uint128_t)data[1] << 64 | data[0];
    return ret;
  }
  operator uint256_t() {
    uint256_t ret = (uint256_t)data[3] << 192 | (uint256_t)data[2] << 128 | (uint256_t)data[1] << 64 | (uint256_t)data[0];
    return ret;
  }
  operator uint1024_t() {
    uint1024_t ret(data);
    return ret;
  }
  operator uint2048_t() {
    uint2048_t ret(data);
    return ret;
  }
  template<int num> operator wide_t<num>() {
    wide_t<num> ret;
    for (int i = 0; i < num; i++) {
      ret.data[i] = data[i];
    }
    return ret;
  }
  wide_t<_dataNum> tail(int n) {
    wide_t<_dataNum> ret;
    int fullNum = n / 64;
    int shiftNum = (64 - n % 64) % 64;
    for (int i = 0; i < fullNum; i++) {
      ret.data[i] = data[i];
    }
    if (shiftNum != 64) {
      ret.data[fullNum] = (data[fullNum] << shiftNum) >> shiftNum;
    }
    return ret;
  }
  wide_t<_dataNum> flip() {
    wide_t<_dataNum> ret;
    for (int i = 0; i < _dataNum; i++) {
      ret.data[i] = ~data[i];
    }
    return ret;
  }
  uint128_t tail128(int n) {
    uint128_t ret = (uint128_t)data[1] << 64 | data[0];
    int shiftNum = 128 - n;
    return ret << shiftNum >> shiftNum;
  }
  uint512_t tail512(int n) {
    uint512_t ret;
    ret.u256_0 = (uint256_t)data[3] << 192 | (uint256_t)data[2] << 128 | (uint256_t)data[1] << 64 | (uint256_t)data[0];
    ret.u256_1 = (uint256_t)data[7] << 192 | (uint256_t)data[6] << 128 | (uint256_t)data[5] << 64 | (uint256_t)data[4];
    // ret.u256_0 = *(uint256_t*)data[0];
    // ret.u256_1 = *(uint256_t*)data[4];
    int shiftNum = 512 - n;
    return ret << shiftNum >> shiftNum;
  }
  uint128_t bits128(int hi, int lo) {
    uint128_t ret = 0;
    int offset = 0;
    int width = hi - lo + 1;
    int idx = lo / 64;
    while (offset != width) {
      int range_lo = (lo + offset) % 64;
      int range_hi = MIN(63, width - offset + range_lo - 1);
      uint64_t mask = range_hi == 63 ? (uint64_t)0xffffffffffffffff : (((uint64_t)1 << (range_hi + 1)) - 1);
      /* clear high */
      uint64_t seg = (data[idx] & mask) >> range_lo;
      ret |= (uint128_t)seg << offset;
      int segWidth = range_hi - range_lo + 1;;
      offset += segWidth;
      idx ++;
    }
    return ret;
  }
  uint256_t bits256(int hi, int lo) {
    uint256_t ret = 0;
    int offset = 0;
    int width = hi - lo + 1;
    int idx = lo / 64;
    while (offset != width) {
      int range_lo = (lo + offset) % 64;
      int range_hi = MIN(63, width - offset + range_lo - 1);
      uint64_t mask = range_hi == 63 ? 0xffffffffffffffff : ((1 << (range_hi + 1)) - 1);
      /* clear high */
      uint64_t seg = (data[idx] & mask) >> range_lo;
      ret |= (uint256_t)seg << offset;
      int segWidth = range_hi - range_lo + 1;;
      offset += segWidth;
      idx ++;
    }
    return ret;
  }
  uint512_t __attribute__ ((noinline)) bits512(int hi, int lo) {
    uint512_t ret = 0;
    int offset = 0;
    int width = hi - lo + 1;
    int idx = lo / 64;
    while (offset != width) {
      int range_lo = (lo + offset) % 64;
      int range_hi = MIN(63, width - offset + range_lo - 1);
      uint64_t mask = range_hi == 63 ? 0xffffffffffffffff : ((1 << (range_hi + 1)) - 1);
      /* clear high */
      uint64_t seg = (data[idx] & mask) >> range_lo;
      ret |= (uint512_t)seg << offset;
      int segWidth = range_hi - range_lo + 1;;
      offset += segWidth;
      idx ++;
    }
    return ret;
  }
  void display() {
    for (int i = 0; i < dataNum; i ++) printf("%lx ", data[i]);
  }
  void displayn() {
    display();
    printf("\n");
  }
};

#endif