/**
 * @file common.h
 * @brief common headers and macros
 */

#ifndef COMMON_H
#define COMMON_H

#include <sstream>
#include <string>
#include <iostream>
#include <vector>
#include <fstream>
#include <stdlib.h>
#include <cstring>
#include <cstdint>
#include <string.h>
#include <algorithm>
#include <set>
#include <map>
#include <gmp.h>
#include <cstdarg>

#define NR_THREAD 10
#define ORDERED_TOPO_SORT
// #define PERF

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define _STR(a) # a
#define STR(a)  _STR(a)
#define _CONCAT(a, b) a ## b
#define CONCAT(a, b)  _CONCAT(a, b)

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)
#define ABS(a) (a >= 0 ? a : -a)
#define ROUNDUP(a, sz) ((((uintptr_t)a) + (sz) - 1) & ~((sz) - 1))

#define nodeType(node) widthUType(node->width)

#define Cast(width, sign)                     \
  ("(" + (sign ? widthSType(width) : widthUType(width)) + ")")

#define widthType(width, sign)                     \
  (sign ? widthSType(width) : widthUType(width))

#define widthUType(width) \
  std::string(width <= 8 ? "uint8_t" : \
            (width <= 16 ? "uint16_t" : \
            (width <= 32 ? "uint32_t" : \
            (width <= 64 ? "uint64_t" : format("unsigned _BitInt(%d)", ROUNDUP(width, 64))))))

#define widthSType(width) \
  std::string(width <= 8 ? "int8_t" : \
            (width <= 16 ? "int16_t" : \
            (width <= 32 ? "int32_t" : \
            (width <= 64 ? "int64_t" : format("_BitInt(%d)", ROUNDUP(width, 64))))))

#define widthBits(width) \
        (width <= 8 ? 8 : \
        (width <= 16 ? 16 : \
        (width <= 32 ? 32 : \
        (width <= 64 ? 64 : ROUNDUP(width, 64)))))

#define BASIC_WIDTH 256
#define MAX_UINT_WIDTH 2048
#define BASIC_TYPE __uint128_t
#define uint128_t __uint128_t
#define MAX_U8 0xff
#define MAX_U16 0xffff
#define MAX_U32 0xffffffff
#define MAX_U64 0xffffffffffffffff

#define UCast(width) (std::string("(") + widthUType(width) + ")")

#define MAP(c, f) c(f)

// #define TIME_COUNT

#ifdef TIME_COUNT
#define MUX_COUNT(...) __VA_ARGS__
#else
#define MUX_COUNT(...)
#endif

#define MUX_DEF(macro, ...) \
  do { \
    if (macro) { \
      __VA_ARGS__ \
    } \
  } while(0)

#define MUX_NDEF(macro, ...) \
  do { \
    if (!macro) { \
      __VA_ARGS__ \
    } \
  } while(0)


#define ACTIVATE_ALL 0xff

#ifndef CLOCK_GATE_NAME
#define CLOCK_GATE_NAME "ClockGate"
#endif

class TypeInfo;
class PNode;
class Node;
class AggrParentNode;
class ENode;
class ExpTree;
class SuperNode;
class valInfo;
class clockVal;
class ArrayMemberList;

enum ResetType { UNCERTAIN, ASYRESET, UINTRESET, ZERO_RESET };

std::string arrayMemberName(Node* node, std::string suffix);
#define newBasic(node) (node->isArrayMember ? arrayMemberName(node, "new") : (node->name + "$new"))
#define newName(node) newBasic(node)
#define oldName(node) (node->name + "$old$" + std::to_string(node->id))
#define ASSIGN_LABLE std::string("ASSIGN$$$LABEL")
#define ASSIGN_INDI(node) (node->name + "$$UPDATE")
#define ASSIGN_BEG(node) (node->name + std::to_string(node->id) + "$$BEG ")
#define ASSIGN_END(node) (node->name + std::to_string(node->id) + "$$END ")

#include "opFuncs.h"
#include "debug.h"
#include "Node.h"
#include "PNode.h"
#include "ExpTree.h"
#include "StmtTree.h"
#include "graph.h"
#include "util.h"
#include "valInfo.h"
#include "perf.h"
#include "config.h"

#define TIMER_START(name) struct timeval CONCAT(__timer_, name) = getTime();
#define TIMER_END(name) do { \
  struct timeval t = getTime(); \
  char buf[256]; \
  snprintf(buf, sizeof(buf) - 1, "Timer.%s()." STR(name), __func__); \
  showTime(buf, CONCAT(__timer_, name), t); \
} while (0)

struct ordercmp {
  bool operator()(Node* n1, Node* n2) {
    return n1->order > n2->order;
  }
};

#endif
