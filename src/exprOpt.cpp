#include "common.h"
#include <stack>

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
  for (ExpTree* tree : assignTree) {
    std::tie(beg, end) = tree->getlval()->getIdx(this);
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
  for (ExpTree* tree : assignTree) fillArrayInvalid(tree);
  for (ExpTree* tree : arrayVal) fillArrayInvalid(tree);
}

bool checkENodeEq(ENode* enode1, ENode* enode2);
bool subTreeEq(ENode* enode1, ENode* enode2) {
  if (!checkENodeEq(enode1, enode2)) return false;
  for (size_t i = 0; i < enode1->getChildNum(); i ++) {
    if (!subTreeEq(enode1->getChild(i), enode2->getChild(i))) return false;
  }
  return true;
}

void ExpTree::treeOpt() {
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, -1));

  while(!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (top->opType == OP_WHEN || top->opType == OP_MUX) {
      if (subTreeEq(top->getChild(1), top->getChild(2))) {
        if (parent) {
          parent->setChild(idx, top->getChild(1));
        } else {
          setRoot(top->getChild(1));
        }
        top = top->getChild(1);
      }
    }
    for (size_t i = 0; i < top->child.size(); i ++) {
      if (top->child[i]) s.push(std::make_tuple(top->child[i], top, i));
    }
  }
}

void graph::exprOpt() {
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) tree->treeOpt();
      for (ExpTree* tree : node->arrayVal) tree->treeOpt();
      if (node->resetVal) node->resetVal->treeOpt();
      if (node->updateTree) node->updateTree->treeOpt();
    }
  }

  reconnectAll();
}