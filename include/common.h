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

// #define MEM_CHECK
// #define PERF
// #define EMU_LOG
// #define LOG_START 0
// #define LOG_END 0

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)
#define ABS(a) (a >= 0 ? a : -a)

#define nodeType(node) widthUType(node->width)

#define Cast(width, sign)                     \
  ("(" + (sign ? widthSType(width) : widthUType(width)) + ")")

#define widthType(width, sign)                     \
  (sign ? widthSType(width) : widthUType(width))

#define widthUType(width) \
  std::string(width <= 8 ? "uint8_t" : \
            (width <= 16 ? "uint16_t" : \
            (width <= 32 ? "uint32_t" : \
            (width <= 64 ? "uint64_t" : \
            (width <= 128 ? "uint128_t" : \
            (width <= 256 ? "uint256_t" : \
            (width <= 512 ? "uint512_t" : \
            (width <= 1024 ? "uint1024_t" : \
            (width <= 2048 ? "uint2048_t" : format("wide_t<%d>", (width + 63) / 64))))))))))

#define widthSType(width) \
  std::string(width <= 8 ? "int8_t" : \
            (width <= 16 ? "int16_t" : \
            (width <= 32 ? "int32_t" : \
            (width <= 64 ? "int64_t" : \
            (width <= 128 ? "int128_t" : \
            (width <= 256 ? "int256_t" : \
            (width <= 512 ? "int512_t" : \
            (width <= 1024 ? "int1024_t" : \
            (width <= 2048 ? "int2048_t" : "UNIMPLEMENTED")))))))))

#define widthBits(width) \
        (width <= 8 ? 8 : \
        (width <= 16 ? 16 : \
        (width <= 32 ? 32 : \
        (width <= 64 ? 64 : \
        (width <= 128 ? 128 : \
        (width <= 256 ? 256 : \
        (width <= 512 ? 512 : \
        (width <= 1024 ? 1024 : \
        (width <= 2048 ? 2048 : (width + 63) / 64 * 64)))))))))

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
#define ASSIGN_LABLE std::string("ASSIGN$$$LABEL")
#define ASSIGN_INDI(node) (node->name + "$$UPDATE")
#include "opFuncs.h"
#include "debug.h"
#include "Node.h"
#include "PNode.h"
#include "ExpTree.h"
#include "graph.h"
#include "util.h"
#include "valInfo.h"
#include "perf.h"


struct ordercmp {
  bool operator()(Node* n1, Node* n2) {
    return n1->order > n2->order;
  }
};

#endif
