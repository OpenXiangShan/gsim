/**
 * @file util.cpp
 * @brief utils
 */

#include <string>
#include <map>
#include <execinfo.h>

#include "common.h"
#include "util.h"

/* convert firrtl constant to C++ constant 
   In the new FIRRTL spec, there are 'int' and 'rint'.*/
std::pair<int, std::string> firStrBase(std::string s) {
  if (s.length() <= 1) { return std::make_pair(10, s); }
  std::string ret;

  int idx  = 0;
  int base = -1;

  // Check if the constant is negative.
  if (s[0] == '-') {
    ret += '-';
    idx = 1;
  }
  if (s[idx] != '0') return std::make_pair(10, s);

  idx ++;

  // If there is no "0b", "0o", "0d", or "0h" prefix, treat it as a base-10 integer.
  if ((s[idx] != 'b') && (s[idx] != 'o') && (s[idx] != 'd') && (s[idx] != 'h')) {
    ret += s.substr(idx - 1);
    return std::make_pair(10, ret);
  }

  // Determine the base from the prefix.
  switch (s[idx]) {
    case 'b': base = 2; break;
    case 'o': base = 8; break;
    case 'h': base = 16; break;
    default: base = 10; break;
  }

  idx ++;
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

std::string bitMask(int width) {
  Assert(width > 0, "invalid width %d", width);
  if (width <= 64) {
    std::string ret = std::string(width/4, 'f');
    const char* headTable[] = {"", "1", "3", "7"};
    ret = headTable[width % 4] + ret;
    return "0x" + ret;
  } else {
    std::string type = widthUType(width);
    if (width % 64 == 0) { // in such case, (type)1 << width is undefined
      return std::format("(({})0 - 1)", type);
    } else {
      return std::format("((({})1 << {}) - 1)", type, width);
    }
  }
}

std::string shiftBits(unsigned int bits, ShiftDir dir){
  if(bits == 0)
    return "";
  return (dir == ShiftDir::Left? " << " : " >> ") + std::to_string(bits);
}
std::string shiftBits(std:: string bits, ShiftDir dir){
  if(bits == std::to_string(0) || bits == "0x0")
    return "";
  return (dir == ShiftDir::Left? " << " : " >> ") + bits;
}

void print_stacktrace() {
  int size = 16;
  void * array[16];
  int stack_num = backtrace(array, size);
  char ** stacktrace = backtrace_symbols(array, stack_num);
  for (int i = 0; i < stack_num; ++i) {
    fprintf(stderr, "%s\n", stacktrace[i]);
  }
  free(stacktrace);
}
