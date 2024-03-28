/**
 * @file util.cpp
 * @brief utils
 */

#include <string>
#include <map>
#include <iostream>
#include "common.h"
/* convert firrtl constant to C++ constant */
std::pair<int, std::string> firStrBase(std::string s) {
  if (s.length() <= 1) { return std::make_pair(10, s); }

  std::string ret;

  int idx  = 1;
  int base = -1;

  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }

  if (s[0] == 'b') {
    base = 2;
  } else if (s[0] == 'o') {
    base = 8;
  } else if (s[0] == 'h') {
    base = 16;
  } else {
    base = 10;
    idx  = 0;
  }

  ret += s.substr(idx);

  return std::make_pair(base, ret);
}

std::string to_hex_string(BASIC_TYPE x) {
  if (x == 0) { return "0"; }

  std::string ret;

  while (x != 0) {
    int rem = x % 16;
    ret     = (char)(rem < 10 ? (rem + '0') : (rem - 10 + 'a')) + ret;
    x /= 16;
  }

  return ret;
}

static std::map<std::string, OPType> expr2Map = {
  {"add", OP_ADD},  {"sub", OP_SUB},  {"mul", OP_MUL},  {"div", OP_DIV},
  {"rem", OP_REM},  {"lt", OP_LT},  {"leq", OP_LEQ},  {"gt", OP_GT},
  {"geq", OP_GEQ},  {"eq", OP_EQ},  {"neq", OP_NEQ},  {"dshl", OP_DSHL},
  {"dshr", OP_DSHR},  {"and", OP_AND},  {"or", OP_OR},  {"xor", OP_XOR},
  {"cat", OP_CAT},
};

OPType str2op_expr2(std::string name) {
  Assert(expr2Map.find(name) != expr2Map.end(), "invalid 2expr op %s\n", name.c_str());
  return expr2Map[name];
}

static std::map<std::string, OPType> expr1Map = {
  {"asUInt", OP_ASUINT}, {"asSInt", OP_ASSINT}, {"asClock", OP_ASCLOCK}, {"asAsyncReset", OP_ASASYNCRESET},
  {"cvt", OP_CVT}, {"neg", OP_NEG}, {"not", OP_NOT}, {"andr", OP_ANDR},
  {"orr", OP_ORR}, {"xorr", OP_XORR},
};

OPType str2op_expr1(std::string name) {
  Assert(expr1Map.find(name) != expr1Map.end(), "invalid 1expr op %s\n", name.c_str());
  return expr1Map[name];
}

static std::map<std::string, OPType> expr1int1Map = {
  {"pad", OP_PAD}, {"shl", OP_SHL}, {"shr", OP_SHR}, {"head", OP_HEAD}, {"tail", OP_TAIL},
};

OPType str2op_expr1int1(std::string name) {
  Assert(expr1int1Map.find(name) != expr1int1Map.end(), "invalid 1expr op %s\n", name.c_str());
  return expr1int1Map[name];
}

int upperPower2(int x) {
  return x <= 1 ? x : (1 << (32 - __builtin_clz(x - 1)));
}

int upperLog2(int x) {
  if (x <= 1) return x;
  return (32 - __builtin_clz(x - 1));
}

static char buf[0x1000000];

std::string format(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  std::vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  std::string ret = buf;
  Assert(ret.length() < sizeof(buf) - 1, "require larger buf");
  return ret;
}
