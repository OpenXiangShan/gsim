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
  Node* belong = nullptr;
  std::vector<StmtNode*> child;
  OPType type = OP_EMPTY;
  valInfo* computeInfo = nullptr;
  StmtNode(OPType _type) : type(_type) {}
  StmtNode(ENode* _enode, Node* _belong) : isENode(true), enode(_enode), belong(_belong), type(OP_STMT_NODE) {}
  StmtNode(ExpTree* _tree, Node* _belong) : isENode(false), tree(_tree), belong(_belong), type(OP_STMT_NODE) {}
  size_t getChildNum() { return child.size(); }
  size_t checkChildIdx(size_t idx) {
    Assert(getChildNum() > idx, "idx %ld is out of bound [0, %ld)", idx, getChildNum());
    return idx;
  }
  StmtNode* getChild(size_t idx) { return child[checkChildIdx(idx)]; }
  void setChild(size_t idx, StmtNode* node) { child[checkChildIdx(idx)] = node; }
  void eraseChild(size_t idx) { child.erase(child.begin() + checkChildIdx(idx)); }
  void addChild(StmtNode* node) { child.push_back(node); }
  void generateWhenTree(ENode* lvalue, ENode* enode, std::vector<int>& subPath, Node* belong);
  void compute(std::vector<InstInfo>& insts, std::set<InstInfo> assign_insts[] = nullptr);
};

class StmtTree {
public:
  StmtNode* root;
  void mergeExpTree(ExpTree* tree, std::vector<int>& prevPath, std::vector<int>& nodePath, Node* belong);
  void display();
  void addSeq(ExpTree* tree, Node* belong);
  void mergeStmtTree(StmtTree* tree);
  void compute(std::vector<InstInfo>& insts);
};

#endif