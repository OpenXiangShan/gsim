/**
 * @file Node.h
 * @brief Declaration of Node classes.
 */

#ifndef NODE_H
#define NODE_H

#include "common.h"

enum {
  NODE_REG_SRC,
  NODE_REG_DST,
  NODE_ACTIVE,
  NODE_INP,
  NODE_OUT,
  NODE_MEMORY,
  NODE_READER,
  NODE_WRITER,
  NODE_READWRITER,
  NODE_MEMBER,
  NODE_L1_RDATA,
  NODE_TMP,
  NODE_LOCAL, // local node in when statment
  NODE_OTHERS,
  NODE_ARRAY_MEMBER,
};

enum { VALID_NODE, DEAD_NODE, CONSTANT_NODE };
enum { INDEX_INT, INDEX_NODE };

class PNode;
class Node;
class AggrType;

class Index {
public:
  int type;
  int idx;
  Node* nidx;
};

class Operand {
public:
  std::vector<Index> index;
  Node* node;
};

class AggrMember {
public:
  bool isFlip;
  Node* n;
};

class _AggrType {
public:
  int width = 0;
  bool sign = false;
  bool isFlip = false;
  std::string name;
  AggrType* aggrType;
};

class AggrType {
public:
  std::vector<_AggrType> aggr;
};


class ExprValue {
public:
  std::vector<PNode*> ops;
  std::vector<Operand*> operands;
  std::string consVal;
};

class Node {
 public:
  /**
   * @brief Default constructor.
   */
  Node() {
    type    = NODE_OTHERS;
    visited = 0;
    inEdge  = 0;
  }

  /**
   * @brief Constructor with a type parameter.
   *
   * @param _type The type of the node.
   */
  Node(int _type) {
    type    = _type;
    visited = 0;
    inEdge  = 0;
  }

  // update in AST2Graph
  /**
   * @brief update in AST2Graph; concat the module name in order (member in structure / temp
   * variable)
   */
  std::string name;  // concat the module name in order (member in structure / temp variable)
  int id = -1;
  int clusId = -1;
  int type;
  int width = 0;
  bool sign = false;
  int val;
  int status = VALID_NODE;
  int visited;
  int scratch;
  bool regSplit = false;
  bool arraySplit = false;
  int whenDepth = 0;
  AggrType* aggrType = NULL;
  std::vector<int> dimension;
  int entryNum = 1;
  std::vector<Node*> next;
  std::vector<Node*> prev;
  std::vector<Index> index;
  std::vector<ExprValue*> imps;
  std::vector<ExprValue*> whenStack;
  std::vector<Node*> clusNodes;
  std::vector<Node*> aggrMember;    // can not combine into aggrType, as many nodes may share a single aggrType
  std::vector<std::string> consArray;
  ExprValue* workingVal = NULL;
  ExprValue* value = NULL;
  ExprValue* resetVal = NULL;  // for regs
  ExprValue* resetCond = NULL;
  int inEdge;  // for topo sort
  std::vector<Node*> member;
  Node* parent = NULL;
  int latency[2];
  Node* regNext;
  std::vector<std::string> insts;
  void set_id(int _id) {
    id     = _id;
    clusId = id;
  }
};

#endif
