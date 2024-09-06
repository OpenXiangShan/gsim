#include <cstdint>
#define uint128_t __uint128_t
#define int128_t __int128_t

class uint512_t;
class uint1024_t;
class uint2048_t;

#define SUPPORT_U256 true
#if SUPPORT_U256
#define uint256_t unsigned _BitInt(256)
#define int256_t _BitInt(256)
#else 
class uint256_t {
public:
  union {
    uint128_t u128_1;
    struct {
      uint64_t u64_2;
      uint64_t u64_3;
    };
  };
  union {
    uint128_t u128_0;
    struct {
      uint64_t u64_0;
      uint64_t u64_1;
    };
  };
  uint256_t() {}
  uint256_t(uint64_t val) {
    u128_0 = val;
    u128_1 = 0;
  }
  void operator = (uint64_t data) {
    // uint256_t ret;
    u128_0 = data;
    u128_1 = 0;
    // return ret;
  }
  bool operator == (uint256_t b) {
    return u128_1 == b.u128_1 && u128_0 == b.u128_0;
  }
  bool operator != (uint256_t b) {
    return u128_1 != b.u128_1 || u128_0 != b.u128_0;
  }
  uint256_t operator << (int shiftNum) {
    uint256_t ret;
    ret.u128_0 = shiftNum >= 128 ? 0 : (u128_0 << shiftNum);
    ret.u128_1 = shiftNum >= 256 ? 0 :
                  (shiftNum >= 128 ?
                    (u128_0 << (shiftNum - 128)) :
                    (u128_1 << shiftNum) | (u128_0 >> (128 - shiftNum)));
    return ret;
  }
  uint256_t operator >> (int shiftNum) {
    uint256_t ret;
    ret.u128_1 = shiftNum >= 128 ? 0 : (u128_1 >> shiftNum);
    ret.u128_0 = shiftNum >= 256 ? 0 :
                  (shiftNum >= 128 ?
                    (u128_1 >> (shiftNum - 128)) :
                    (u128_0 >> shiftNum) | (u128_1 << (128 - shiftNum)));
    return ret;
  }
  uint256_t operator & (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 & a.u128_1;
    ret.u128_0 = u128_0 & a.u128_0;
    return ret;
  }
  uint64_t operator & (uint64_t a) {
    return u128_0 & a;
  }
  uint256_t operator | (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 | a.u128_1;
    ret.u128_0 = u128_0 | a.u128_0;
    return ret;
  }
  uint256_t operator ^ (uint256_t a) {
    uint256_t ret;
    ret.u128_1 = u128_1 ^ a.u128_1;
    ret.u128_0 = u128_0 ^ a.u128_0;
    return ret;
  }
};
#endif

class uint512_t {
public:
  union{
    uint256_t u256_1;
  };
  union {
    uint256_t u256_0;
  };
  uint512_t() {}
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
    // uint512_t ret;
    u256_0 = data;
    u256_1 = 0;
    // return ret;
  }
  void operator = (uint512_t data) {
    // uint512_t ret;
    u256_1 = data.u256_1;
    u256_0 = data.u256_0;
    // return ret;
  }
  void operator = (uint1024_t data);
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
    if (shiftNum == 0) {
      ret.u256_0 = u256_0;
      ret.u256_1 = u256_1;
      return ret;
    }
#if SUPPORT_U256
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
  uint512_t operator >> (int shiftNum) {
    uint512_t ret;
    ret.u256_1 = shiftNum >= 256 ? 0 : (u256_1 >> shiftNum);
    ret.u256_0 = shiftNum >= 512 ? 0 :
                  (shiftNum >= 256 ?
                    (u256_1 >> (shiftNum - 256)) :
                    (u256_0 >> shiftNum) | (u256_1 << (256 - shiftNum)));
    return ret;
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
  uint512_t operator ^ (uint512_t a) {
    uint512_t ret;
    ret.u256_1 = u256_1 ^ a.u256_1;
    ret.u256_0 = u256_0 ^ a.u256_0;
    return ret;
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
  operator uint512_t() {
    return u512_0;
  }
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