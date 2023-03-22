#include "common.h"
#include "Node.h"
#include "graph.h"

#define MAX_REG 40
#define MIN_REG 10
#define MAX_MID 40
#define MIN_MID 10

void randomGraph(graph* g) {
  g->name = "test";
  srand(time(NULL));
  int regNum = (rand() % (MAX_REG - MIN_REG)) + MIN_REG;
  for(int i = 0; i < regNum; i++) {
    Node* ptr = new Node();
    g->sources.push_back(ptr);
    ptr->name = std::string("REG_") + std::to_string(i);
    ptr->type = NODE_REG;
    ptr->inEdge = 0;
  }

  int midNum = (rand() % (MAX_MID - MIN_MID)) + MIN_MID;
  std::vector<Node*> midNodes;
  for(int i = 0; i < midNum; i++) {
    Node* ptr = new Node();
    midNodes.push_back(ptr);
    ptr->name = std::string("TMP") + std::to_string(i);
    int src1 = rand() % (regNum + i);
    int src2 = rand() % (regNum + i);
    if(src1 < regNum) {
      ptr->op += /*"top->" + */ g->sources[src1]->name;
      g->sources[src1]->next.push_back(ptr);
    } else {
      ptr->op += midNodes[src1 - regNum]->name;
      midNodes[src1 - regNum]->next.push_back(ptr);
    }
    ptr->op += "+";
    if(src2 < regNum) {
      ptr->op += /* "top->" + */ g->sources[src2]->name;
      g->sources[src2]->next.push_back(ptr);
    } else {
      ptr->op += midNodes[src2 - regNum]->name;
      midNodes[src2 - regNum]->next.push_back(ptr);
    }
    ptr->inEdge = 2;
  }

  std::vector<Node*> regNext;
  for(int i = 0; i < regNum; i++) {
    Node* ptr = new Node();
    regNext.push_back(ptr);
    ptr->name = g->sources[i]->name + "_next";
    int src1 = rand() % midNum;
    int src2 = rand() % midNum;
    ptr->op += midNodes[src1]->name + "+" + midNodes[src2]->name;
    midNodes[src1]->next.push_back(ptr);
    midNodes[src2]->next.push_back(ptr);
    ptr->inEdge = 2;
  }
}
