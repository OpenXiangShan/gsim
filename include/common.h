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

#include "debug.h"

#include "Node.h"
#include "PNode.h"

#define LENGTH(a) (sizeof(a) / sizeof(a[0]))

#define MAX(a, b) ((a >= b) ? a : b)
#define MIN(a, b) ((a >= b) ? b : a)
#define ABS(a) (a >= 0 ? a : -a)

#define nodeType(node)                     \
  std::string(node->width <= 8 ? "uint8_t" : \
            ((node->width <= 16 ? "uint16_t" : \
            ((node->width <= 32 ? "uint32_t" : "uint64_t")))))

#define widthUType(width) \
  std::string(width <= 8 ? "uint8_t" : \
            ((width <= 16 ? "uint16_t" : \
            ((width <= 32 ? "uint32_t" : "uint64_t")))))

#define widthSType(width) \
  std::string(width <= 8 ? "int8_t" : \
            ((width <= 16 ? "int16_t" : \
            ((width <= 32 ? "int32_t" : "int64_t")))))

#define widthBits(width) \
        (width <= 8 ? 8 : \
        ((width <= 16 ? 16 : \
        ((width <= 32 ? 32 : 64)))))

#define UCast(width) (std::string("(") + widthUType(width) + ")")

#define MAP(c, f) c(f)

#define NOT_VISIT 0
#define EXPANDED 1
#define VISITED 2

// #define TIME_COUNT

#ifdef TIME_COUNT
#define MUX_COUNT(...) __VA_ARGS__
#else
#define MUX_COUNT(...)
#endif

#endif
