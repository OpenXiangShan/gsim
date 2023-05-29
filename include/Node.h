#ifndef NODE_H
#define NODE_H
#include "common.h"
enum {NODE_REG_SRC, NODE_REG_DST, NODE_ACTIVE, NODE_INP, NODE_OUT, \
    NODE_MEMORY, NODE_READER, NODE_WRITER, NODE_READWRITER, NODE_MEMBER, NODE_L1_RDATA, NODE_OTHERS};
enum {NO_OP, OP_ADD, OP_SUB, OP_MUL, OP_DIV};
enum {VALID_NODE, DEAD_NODE};
class Node {
public:
  Node() {
    type = NODE_OTHERS;
    visited = 0;
    inEdge = 0;
  }
  Node(int _type) {
    type = _type;
    visited = 0;
    inEdge = 0;
  }
  std::string name; // concat the module name in order (member in structure / temp variable)
  int id;
  int type;
  int width = 0;
  int sign = 0;
  std::vector<Node*> next;
  std::vector<Node*> prev;
  std::vector<std::string> insts;
  int inEdge; // for topo sort
  int val;
  Node* regNext;
  std::vector<Node*>member;
  int latency[2];
  int visited;
  int status = 0;
};
#endif