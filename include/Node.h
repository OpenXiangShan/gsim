#ifndef NODE_H
#define NODE_H
#include "common.h"
enum {NODE_REG_SRC, NODE_REG_DST, NODE_OTHERS};
enum {NO_OP, OP_ADD, OP_SUB, OP_MUL, OP_DIV};
class Node {
public:
  Node() {
    defined = 0;
    type = NODE_OTHERS;
  }
  std::string name; // concat the module name in order (member in structure / temp variable)
  int id;   // unused
  int type; // unused register ? mid
  int width;
  std::vector<Node*> next;
  std::string op;  // replace the variable names with new names
  // std::vector<Node*> prev; // keep the order in sources file
  // std::vector<std::string> op;
  int inEdge; // for topo sort
  int defined;
  int initVal;
  Node* regNext;
};
#endif