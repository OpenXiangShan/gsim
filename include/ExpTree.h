/*
  classes of expression tree and nodes in the tree
*/
#ifndef EXPTREE_H
#define EXPTREE_H

class Node;
class valInfo;
class NodeComponent;

enum OPType {
  OP_EMPTY,
  OP_MUX,
/* 2expr */
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_REM,
  OP_LT,
  OP_LEQ,
  OP_GT,
  OP_GEQ,
  OP_EQ,
  OP_NEQ,
  OP_DSHL,
  OP_DSHR,
  OP_AND,
  OP_OR,
  OP_XOR,
  OP_CAT,
/* 1expr */
  OP_ASUINT,
  OP_ASSINT,
  OP_ASCLOCK,
  OP_ASASYNCRESET,
  OP_CVT,
  OP_NEG,
  OP_NOT,
  OP_ANDR,
  OP_ORR,
  OP_XORR,
/* 1expr1int */
  OP_PAD,
  OP_SHL,
  OP_SHR,
  OP_HEAD,
  OP_TAIL,
/* 1expr2int */
  OP_BITS,
  OP_BITS_NOSHIFT, // used for bit operations
/* index */
  OP_INDEX_INT,
  OP_INDEX,
/* when, may be replaced by mux */
  OP_WHEN,
  OP_STMT,
/* special */
  OP_PRINTF,
  OP_ASSERT,
/* leaf non-node enode */
  OP_INT,
/* for arrays */
  OP_GROUP,
/* special nodes for memory */
  OP_READ_MEM,
  OP_WRITE_MEM,
  OP_INFER_MEM,
/* special nodes for invalid node */
  OP_INVALID,
  OP_RESET,
/* width processing */
  OP_SEXT,
};

class ENode {
private:
  static int counter;
  valInfo* instsMux(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAdd(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsSub(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsMul(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDIv(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsRem(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsLt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsLeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsGt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsGeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsEq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNeq(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDshl(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsDshr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAnd(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsOr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsXor(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsCat(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsUInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsSInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsClock(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAsAsyncReset(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsCvt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNeg(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsNot(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsAndr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsOrr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsXorr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsPad(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsShl(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsShr(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsHead(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsTail(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsBits(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsBitsNoShift(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsWhen(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsStmt(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsIndexInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsIndex(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsInt(Node* n, std::string lvalue, bool isRoot);
  valInfo* instsReadMem(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsWriteMem(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsInvalid(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsReset(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsGroup(Node* node, std::string lvalue, bool isRoot);
  valInfo* instsPrintf();
  valInfo* instsAssert();
  /* used in constantNode */
  valInfo* consMux(bool isLvalue);
  valInfo* consAdd(bool isLvalue);
  valInfo* consSub(bool isLvalue);
  valInfo* consMul(bool isLvalue);
  valInfo* consDIv(bool isLvalue);
  valInfo* consRem(bool isLvalue);
  valInfo* consLt(bool isLvalue);
  valInfo* consLeq(bool isLvalue);
  valInfo* consGt(bool isLvalue);
  valInfo* consGeq(bool isLvalue);
  valInfo* consEq(bool isLvalue);
  valInfo* consNeq(bool isLvalue);
  valInfo* consDshl(bool isLvalue);
  valInfo* consDshr(bool isLvalue);
  valInfo* consAnd(bool isLvalue);
  valInfo* consOr(bool isLvalue);
  valInfo* consXor(bool isLvalue);
  valInfo* consCat(bool isLvalue);
  valInfo* consAsUInt(bool isLvalue);
  valInfo* consAsSInt(bool isLvalue);
  valInfo* consAsClock(bool isLvalue);
  valInfo* consAsAsyncReset(bool isLvalue);
  valInfo* consCvt(bool isLvalue);
  valInfo* consNeg(bool isLvalue);
  valInfo* consNot(bool isLvalue);
  valInfo* consAndr(bool isLvalue);
  valInfo* consOrr(bool isLvalue);
  valInfo* consXorr(bool isLvalue);
  valInfo* consPad(bool isLvalue);
  valInfo* consShl(bool isLvalue);
  valInfo* consShr(bool isLvalue);
  valInfo* consHead(bool isLvalue);
  valInfo* consTail(bool isLvalue);
  valInfo* consBits(bool isLvalue);
  valInfo* consBitsNoShift(bool isLvalue);
  valInfo* consWhen(Node* node, bool isLvalue);
  valInfo* consStmt(Node* node, bool isLvalue);
  valInfo* consGroup(Node* node, bool isLvalue);
  valInfo* consIndexInt(bool isLvalue);
  valInfo* consIndex(bool isLvalue);
  valInfo* consInt(bool isLvalue);
  valInfo* consReadMem(bool isLvalue);
  valInfo* consInvalid(bool isLvalue);
  valInfo* consReset(bool isLvalue);
  valInfo* consAssert();
  valInfo* consPrint();
  /* used in usedBits */

public:
  Node* nodePtr = nullptr;   // leafNodes: point to a real node; internals: nullptr
  std::vector<ENode*> child;
  OPType opType = OP_EMPTY;
  int width = -1;
  bool sign = false;
  bool isClock = false;
  ResetType reset = UNCERTAIN;
  int usedBit = -1;
  Node* memoryNode = nullptr;
  // bool islvalue = false;  // true for root and L_INDEX, otherwise false
  int id; // unique identifier
  std::vector<int> values;     // used in int_noinit/int_init leaf
  std::string strVal;
  valInfo* computeInfo = nullptr;
// potential: index
  ENode(OPType type = OP_EMPTY) {
    opType = type;
    id = counter ++;
  }
  ENode(Node* _node) {
    nodePtr = _node;
    id = counter ++;
  }
  void setNode(Node* node) {
    nodePtr = node;
  }
  void addChild(ENode* node) { // node can be empty (only for when)
    child.push_back(node);
  }
  int getChildNum() {
    return child.size();
  }
  void setChild(int idx, ENode* node) {
    Assert(getChildNum() > idx, "idx %d is out of bound [0, %d)", idx, getChildNum());
    child[idx] = node;
  }
  ENode* getChild(int idx) {
    Assert(getChildNum() > idx, "idx %d is out of bound [0, %d)", idx, getChildNum());
    return child[idx];
  }
  void addVal(int val) {
    values.push_back(val);
  }
  void setOP(OPType type) {
    opType = type;
  }
  Node* getNode() {
    return nodePtr;
  }
  void setWidth(int _width, bool _sign) {
    width = _width;
    sign = _sign;
  }
  void inferWidth();
  void usedBitWithFixRoot(int width);
  void clearWidth();
  valInfo* compute(Node* n, std::string lvalue, bool isRoot);
  valInfo* computeConstant(Node* node, bool isLvalue);
  void passWidthToChild();
  void updateWidth();
  std::pair<int, int> getIdx(Node* n);
  bool hasVarIdx(Node* node);
  Node* getConnectNode();
  ENode* mergeSubTree(ENode* newSubTree);
  ENode* dup();
  int getArrayIndex(Node* node);
  std::vector<int> getDim();
  clockVal* clockCompute();
  ResetType inferReset();
  ArrayMemberList* getArrayMember(Node* node);
  void display();
  uint64_t keyHash();
  NodeComponent* inferComponent(Node* node);
};

/* 
  ExpTree: The expression tree that describe the computing expression of a node
  ** The root represents the lvalue of expression (node that need to be updated)
  ** The internal node is a specific operations
  ** The leaf points to a real node

*/
class ExpTree {
  ENode* root;
  ENode* lvalue;
  void setOrAllocRoot(ENode* node) {
    if (node) root = node;
    else root = new ENode();
  }
public:
    ExpTree(ENode* root) {
      setOrAllocRoot(root);
      lvalue = nullptr;
    }
    ExpTree(ENode* node, ENode* _lvalue) {
      setOrAllocRoot(node);
      lvalue = _lvalue;
    }
    ExpTree(ENode* node, Node* lnode) {
      setOrAllocRoot(node);
      lvalue = new ENode(lnode);
    }
    ENode* getRoot() {
      return root;
    }
    void setRoot(ENode* _root) {
      root = _root;
    }
    ENode* getlval() {
      return lvalue;
    }
    void setlval(ENode* _lvalue) {
      lvalue = _lvalue;
    }
    void display();
    /* used in alias */
    void replace(std::map<Node*, ENode*>& aliasMap, bool isArray);
    /* used in mergeRegister */
    void replace(Node* oldNode, ENode* newENode);
    /* used in commonExpr */
    void replace(std::map<Node*, Node*>& aliasMap);
    /* used in splitNodes */
    void replaceAndUpdateWidth(Node* oldNode, Node* newNode);
    void replace(Node* oldNode, Node* newNode);
    void clearInfo();
    bool isInvalid() {
      Assert(getRoot(), "empty root");
      return getRoot()->opType == OP_INVALID;
    }
    bool isConstant();
    void removeConstant();
    void removeDummyDim(std::map<Node*, std::vector<int>>& arrayMap, std::set<ENode*>& visited);
    uint64_t keyHash();
    void removeSelfAssignMent(Node* node);
    void matchWidth(int width);
    void when2mux(int width);
    void updateWithNewWidth();
    void updateNewChild(ENode* parent, ENode* child, int idx);
    void treeOpt();
    void getRelyNodes(std::set<Node*>& allNodes);
    void updateWithSplittedNode();
    void clearComponent();
    void updateSplittedArray(Node* array, std::set<Node*>& allSplittedArray);
};

class ASTExpTree { // used in AST2Graph, support aggregate nodes
  ENode* expRoot = nullptr;
  // ASTENode* aggrRoot = nullptr;
  std::vector<std::pair<ENode*, bool>> aggrForest;
  // std::vector<std::string> name; // aggr member name, used for creating new nodes in visitNode
  AggrParentNode* anyParent = nullptr; // any nodes, used for creating new nodes in visitNode
  
  void validCheck() {
    Assert((expRoot && !anyParent) || (!expRoot && anyParent), "invalid ASTENode, expRoot %p aggrNum %ld", expRoot, aggrForest.size());
  }
  void requreNormal() {
    Assert(expRoot && !anyParent, "ASTENode is not a normal node");
  }

public:
  ASTExpTree(bool isAggr, int num = 0) {
    if (isAggr) {
      for (int i = 0; i < num; i ++) aggrForest.push_back(std::make_pair(new ENode(), false));
    } //aggrRoot = new ASTENode();
    else expRoot = new ENode();
  }
  void setOp(OPType op) {
    validCheck();
    if (expRoot) expRoot->setOP(op);
    else {
      for (auto entry : aggrForest) entry.first->setOP(op);
    }
  }
  void addVal(int _value) {
    requreNormal();
    expRoot->addVal(_value);
  }
  void setType(int _width, int _sign) {
    requreNormal();
    expRoot->width = _width;
    expRoot->sign = _sign;
  }
  bool isAggr() {
    validCheck();
    return anyParent;
  }
  int getAggrNum() {
    return aggrForest.size();
  }
  // void addName(std::string str) {
  //   name.push_back(str);
  // }
  // std::string getName(int idx) {
  //   return name[idx];
  // }
  void setAnyParent(AggrParentNode* parent) {
    anyParent = parent;
  }
  // add children in arguments into all trees
  void addChildTree(int num, ...) {
    validCheck();
    va_list valist;
    va_start(valist, num);
    for (int i = 0; i < num; i ++) {
      ASTExpTree* childTree = va_arg(valist, ASTExpTree*);
      if (isAggr()) {
        for (size_t i = 0; i < aggrForest.size(); i++) {
          aggrForest[i].first->addChild(childTree->getAggr(i));
        }
      }
      else expRoot->addChild(childTree->getExpRoot());
    }
    va_end(valist);
  }
  void addChildSameTree(ASTExpTree* child) {
    Assert(!child->isAggr(), "require normal");
    if (isAggr()) {
      for (size_t i = 0; i < aggrForest.size(); i++) aggrForest[i].first->addChild(child->getExpRoot()->dup());
    }
    else expRoot->addChild(child->getExpRoot());
  }
  void addChild(ENode* child) {
    if (isAggr()) {
      for (auto entry : aggrForest) entry.first->addChild(child->dup());
    } else {
      expRoot->addChild(child);
    }
  }
  void updateRoot(ENode* root) {
    requreNormal();
    root->addChild(expRoot);
    expRoot = root;
  }
  void setRoot(ENode* root) {
    requreNormal();
    expRoot = root;
  }
  ENode* getExpRoot() {
    requreNormal();
    return expRoot;
  }
  ENode* getAggr(int idx) {
    Assert((int)aggrForest.size() > idx, "idx %d is out of bound", idx);
    return aggrForest[idx].first;
  }
  bool getFlip(int idx) {
    Assert((int)aggrForest.size() > idx, "idx %d is out of bound", idx);
    return aggrForest[idx].second;
  }
  void setFlip(int idx, bool val) {
    Assert((int)aggrForest.size() > idx, "idx %d is out of bound", idx);
    aggrForest[idx].second = val;
  }
  // duplicated new ASTExpTree with the same name and new root
  ASTExpTree* dupEmpty() {
    ASTExpTree* dup = new ASTExpTree(isAggr(), getAggrNum());
    // for (int i = 0; i < getAggrNum(); i++) {
    //   dup->addName(name[i]);
    // }
    dup->anyParent = anyParent;
    return dup;
  }
  AggrParentNode* getParent() {
    return anyParent;
  }
  bool isInvalid() {
    return !isAggr() && expRoot->opType == OP_INVALID;
  }

};

class NodeList {
public:
  std::vector<Node*> nodes;
  
  void merge(NodeList* newList) {
    if (!newList) return;
    nodes.insert(nodes.end(), newList->nodes.begin(), newList->nodes.end());
  }
};

class ArrayMemberList {
public:
  std::vector<Node*> member;
  std::vector<int> idx;
  void add_member(Node* n, int _idx) {
    member.push_back(n);
    idx.push_back(_idx);
  }
};

#endif