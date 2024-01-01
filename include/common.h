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
#include <gmp.h>
#include <cstdarg>

// #define EMU_LOG

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)
#define ABS(a) (a >= 0 ? a : -a)

#define nodeType(node)                     \
  std::string(node->width <= 8 ? "uint8_t" : \
            ((node->width <= 16 ? "uint16_t" : \
            ((node->width <= 32 ? "uint32_t" : \
            (node->width <= 64 ? "uint64_t" : "uint128_t"))))))

#define Cast(width, sign)                     \
  ("(" + (sign ? widthSType(width) : widthUType(width)) + ")")

#define widthType(width, sign)                     \
  (sign ? widthSType(width) : widthUType(width))

#define widthUType(width) \
  std::string(width <= 8 ? "uint8_t" : \
            ((width <= 16 ? "uint16_t" : \
            ((width <= 32 ? "uint32_t" : \
            (width <= 64 ? "uint64_t" : "uint128_t"))))))

#define widthSType(width) \
  std::string(width <= 8 ? "int8_t" : \
            ((width <= 16 ? "int16_t" : \
            ((width <= 32 ? "int32_t" : \
            (width <= 64 ? "int64_t" : "int128_t"))))))

#define widthBits(width) \
        (width <= 8 ? 8 : \
        ((width <= 16 ? 16 : \
        ((width <= 32 ? 32 : \
        (width <= 64 ? 64 : 128))))))

#define BASIC_WIDTH 128
#define BASIC_TYPE __uint128_t
#define uint128_t __uint128_t

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

class TypeInfo;
class PNode;
class Node;
class AggrParentNode;
class ExpTree;
class SuperNode;
class valInfo;

#include "debug.h"
#include "Node.h"
#include "PNode.h"
#include "ExpTree.h"
#include "graph.h"
#include "valInfo.h"

#include "util.h"

#endif
