#include "common.h"
#include <cstdio>
#include <stack>

static size_t arithOp = 0;
static size_t arith128 = 0;
static size_t bitOp = 0;
static size_t bit128 = 0;
static size_t muxOp = 0;
static size_t mux128 = 0;
static size_t logiOp = 0;
static size_t logi128 = 0;
static size_t convertOp = 0;
static size_t convert128 = 0;
static size_t cmpOp = 0;
static size_t cmp128 = 0;
static size_t shiftOp = 0;
static size_t shift128 = 0;
static size_t readmemOp = 0;
static size_t resetOp = 0;

size_t countBits(Node* node) {
  Assert(node->width >= 0, "%s width = %d\n", node->name.c_str(), node->width);
  size_t ret = node->width;
  for (int dim : node->dimension) ret *= dim;
  return ret;
}
size_t countNodeNum(Node* node) {
  size_t ret = 1;
  for (int dim : node->dimension) ret *= dim;
  return ret;
}

void countAllNodeNum(graph* g) {
  size_t totalBits = 0;
  size_t regBits = 0;
  size_t midNum = 0;
  size_t regNum = 0;
  size_t simpleRegNum = 0;
  for (SuperNode* super : g->sortedSuper) {
    for (Node* node : super->member) {
      if (node->type == NODE_REG_DST) continue; // avoid duplicated counting
      if (node->type == NODE_REG_SRC) {
        regNum += countNodeNum(node);
        regBits += countBits(node);
        simpleRegNum += 1;
      } else {
        midNum += countNodeNum(node);
      }
      totalBits += countBits(node);
    }
  }
  printf("totalBits %ld midNum %ld regNum %ld regBits %ld total %ld simpleRegNum %ld\n", totalBits, midNum, regNum, regBits, midNum + regNum, simpleRegNum);
}

size_t countOpsInTree(ExpTree* tree) {
  size_t ret = 0;
  std::stack<ENode*> s;
  s.push(tree->getRoot());
  if (tree->getlval()) s.push(tree->getlval());
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->nodePtr || top->opType == OP_INT || top->opType == OP_INVALID || top->opType == OP_EMPTY || top->opType == OP_INDEX_INT || top->opType == OP_INDEX) {
      // do nothing
    } else {
      ret ++;
      switch (top->opType) {
        case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_REM: case OP_NEG:
          arithOp ++;
          if (top->width > 128) arith128 ++;
          break;
        case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
          if (top->getChild(0)->width > 128) cmp128 ++;
          cmpOp ++;
          break;
        case OP_DSHL: case OP_DSHR:
          if (top->getChild(0)->width > 128 || top->width > 128) shift128 ++;
          shiftOp ++;
          break;
        case OP_AND: case OP_OR: case OP_XOR: case OP_NOT:
          if (top->width > 128) logi128 ++;
          logiOp ++;
          break;
        case OP_ASUINT: case OP_ASSINT: case OP_CVT:
        case OP_ASASYNCRESET: case OP_ASCLOCK:
        case OP_SEXT:
          if (top->getChild(0)->width > 128) convert128 ++;
          convertOp ++;
          break;
        case OP_PAD: case OP_SHL: case OP_SHR: case OP_HEAD:
        case OP_TAIL: case OP_BITS: case OP_BITS_NOSHIFT: case OP_CAT:
          if (top->getChild(0)->width > 128 || top->width > 128) bit128 ++;
          bitOp ++;
          break;
        case OP_MUX: case OP_WHEN:
          if (top->width > 128) mux128 ++;
          muxOp ++;
          break;
        case OP_READ_MEM:
          readmemOp ++;
          break;
        case OP_RESET:
          resetOp ++;
          break;
        case OP_ANDR: case OP_ORR: case OP_XORR:
        case OP_STMT:
        case OP_EXIT:
        case OP_PRINTF:
        case OP_ASSERT:
          break;
        default:
          fprintf(stderr, "invalid op %d\n", top->opType);
          Panic();
          break;
      }
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  return ret;
}

size_t countOpsInNode(Node* node) {
  size_t ret = 0;
  for (ExpTree* tree : node->assignTree) ret += countOpsInTree(tree);
  for (ExpTree* tree : node->arrayVal) ret += countOpsInTree(tree);
  return ret;
}

/* count midNodes in trees */
void countOps(graph* g) {
  size_t totalOps = 0;
  for (SuperNode* super : g->sortedSuper) {
    for (Node* node : super->member) {
      totalOps += countOpsInNode(node);
    }
  }
  printf("totalOps %ld (total128 %ld) arithOp %ld (arith128 %ld) bitOp %ld (bit128 %ld) muxOp %ld (mux128 %ld) logiOp %ld (logi128 %ld) convertOp %ld (convert128 %ld) cmpOp %ld (cmp128 %ld) shiftOp %ld (shift128 %ld) readMem %ld reset %ld \n",
    totalOps, arith128 + bit128 + mux128 + logi128 + convert128 + cmp128 + shift128, arithOp, arith128, bitOp, bit128, muxOp, mux128, logiOp, logi128, convertOp, convert128, cmpOp, cmp128, shiftOp, shift128, readmemOp, resetOp);
}

void graph::perfAnalysis() {
  countAllNodeNum(this);
  countOps(this);
}