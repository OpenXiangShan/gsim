#ifndef STMTTREE_H
#define STMTTREE_H
/* stmt trees indicate how to generate statements in a supernode */
#include "common.h"
class ENode;
class ExpTree;

class StmtNode {
public:
  bool isENode = false;
  ENode* enode = nullptr;
  ExpTree* tree = nullptr;
  std::vector<StmtNode*> child;
  OPType type = OP_EMPTY;
  StmtNode(OPType _type) : type(_type) {}
  StmtNode(ENode* _enode) : isENode(true), enode(_enode), type(OP_STMT_NODE) {}
  StmtNode(ExpTree* _tree) : isENode(false), tree(_tree), type(OP_STMT_NODE) {}
  size_t getChildNum() { return child.size(); }
  size_t checkChildIdx(size_t idx) {
    Assert(getChildNum() > idx, "idx %ld is out of bound [0, %ld)", idx, getChildNum());
    return idx;
  }
  StmtNode* getChild(size_t idx) { return child[checkChildIdx(idx)]; }
  void setChild(size_t idx, StmtNode* node) { child[checkChildIdx(idx)] = node; }
  void addChild(StmtNode* node) { child.push_back(node); }
  void generateWhenTree(ENode* lvalue, ENode* enode);
};

class StmtTree {
public:
  StmtNode* root;
  void mergeExpTree(ExpTree* tree);
  void display();
};

#endif