/**
 * @file Node.h
 * @brief Declaration of Node classes.
 */

#ifndef NODE_H
#define NODE_H

#include "common.h"

enum NODE_TYPE {
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
  NODE_OTHERS
};

enum NODE_STATUS { VALID_NODE, DEAD_NODE, CONSTANT_NODE };

class PNode;

class Node {
 public:
  /**
   * @brief Default constructor.
   */
  Node() {
    type = NODE_OTHERS;
    visited = 0;
    inEdge = 0;
  }

  /**
   * @brief Constructor with a type parameter.
   *
   * @param _type The type of the node.
   */
  Node(int _type) {
    type = _type;
    visited = 0;
    inEdge = 0;
  }

  /**
   * @brief update in AST2Graph; concat the module name in order (member in structure / temp
   * variable)
   */
  std::string name;
  int id;
  int type;
  int width = 0;
  int sign = 0;
  int val;
  int status = VALID_NODE;
  int visited;
  std::vector<Node*> next;
  std::vector<Node*> prev;
  std::vector<Node*> operands;
  std::vector<PNode*> ops;
  int inEdge;  // for topo sort
  std::vector<Node*> member;
  int latency[2];
  Node* regNext;
  // update in constantNode
  std::string consVal;
  // update in instsGenerator
  std::vector<std::string> insts;
};

#endif
