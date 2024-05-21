#include "common.h"
#include <stack>

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

void countNodes(graph* g) {
  size_t totalBits = 0;
  size_t midNum = 0;
  size_t regNum = 0;
  for (SuperNode* super : g->sortedSuper) {
    for (Node* node : super->member) {
      if (node->type == NODE_REG_DST) continue; // avoid duplicated counting
      if (node->type == NODE_REG_SRC) {
        regNum += countNodeNum(node);
      } else {
        midNum += countNodeNum(node);
      }
      totalBits += countBits(node);
    }
  }
  printf("totalBits %ld midNum %ld regNum %ld total %ld\n", totalBits, midNum, regNum, midNum + regNum);
}

size_t countOpsInTree(ExpTree* tree) {
  size_t ret = 0;
  std::stack<ENode*> s;
  s.push(tree->getRoot());
  if (tree->getlval()) s.push(tree->getlval());
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->nodePtr || top->opType == OP_INT || top->opType == OP_INVALID || top->opType == OP_EMPTY) {
      // do nothing
    } else {
      ret ++;
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  return ret;
}
/* count midNodes in trees */
void countOps(graph* g) {
  size_t totalOps = 0;
  for (SuperNode* super : g->sortedSuper) {
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) totalOps += countOpsInTree(tree);
      for (ExpTree* tree : node->arrayVal) totalOps += countOpsInTree(tree);
    }
  }
  printf("totalOps %ld\n", totalOps);
}