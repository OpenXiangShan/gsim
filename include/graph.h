#ifndef GRAPH_H
#define GRAPH_H
#include "common.h"
#include "Node.h"
#include "debug.h"
class graph {
public:
  std::vector<Node*>input;
  std::vector<Node*>output;
  std::vector<Node*>sources;
  std::vector<Node*>sorted;
  std::vector<Node*>active;
  std::vector<Node*>constant;
  std::vector<Node*>memory;
  std::string name;
  int maxTmp = 0;
};
#endif