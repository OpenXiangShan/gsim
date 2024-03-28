#include "common.h"

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

void Node::fillArrayInvalid(ExpTree* tree) {
  int beg, end;
  std::tie(beg, end) = tree->getlval()->getIdx(this);
    bool canBeInvalid = true;
    for (int i = beg; i <= end; i ++) {
      if (invalidIdx.find(i) == invalidIdx.end()) canBeInvalid = false;
    }
    if (canBeInvalid) fillEmptyWhen(tree, new ENode(OP_INVALID));
}

void Node::invalidArrayOptimize() {
  if (invalidIdx.size() == 0) return;
  if (!isArray()) return;
  if (type != NODE_OTHERS) return;
  /* can compute in AST2Graph and pass using member variable */
  std::set<int> allIdx;
  int beg, end;
  if (valTree) {
    std::tie(beg, end) = valTree->getlval()->getIdx(this);
    if (beg < 0) return;  // varialble index
    for (int i = beg; i <= end; i ++) {
      if (allIdx.find(i) != allIdx.end()) return; // multiple assignment
      allIdx.insert(i);
    }
  }
  for (ExpTree* tree : arrayVal) {
    std::tie(beg, end) = tree->getlval()->getIdx(this);
    if (beg < 0) return;  // varialble index
    for (int i = beg; i <= end; i ++) {
      if (allIdx.find(i) != allIdx.end()) return; // multiple assignment
      allIdx.insert(i);
    }
  }
  if (valTree) fillArrayInvalid(valTree);
  for (ExpTree* tree : arrayVal) fillArrayInvalid(tree);    
}