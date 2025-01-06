/*
  sort superNodes in topological order
*/

#include "common.h"
#include <stack>
#include <map>

void graph::topoSort() {
  std::set<SuperNode*> dstNode;
  for (Node* reg : regsrc) {
    dstNode.insert(reg->getDst()->super);
  }
  std::map<SuperNode*, int>times;
  std::stack<SuperNode*> s;
  for (SuperNode* node : supersrc) s.push(node);
  /* next.size() == 0, place the registers at the end to benefit mergeRegisters */
  std::vector<SuperNode*> potentialRegs;
  std::set<SuperNode*> visited;
  while(!s.empty()) {
    SuperNode* top = s.top();
    s.pop();
    Assert(visited.find(top) == visited.end(), "superNode %d is already visited\n", top->id);
    visited.insert(top);
    sortedSuper.push_back(top);
#ifdef ORDERED_TOPO_SORT
    std::vector<SuperNode*> sortedNext;
    sortedNext.insert(sortedNext.end(), top->next.begin(), top->next.end());
    std::sort(sortedNext.begin(), sortedNext.end(), [](SuperNode* a, SuperNode* b) {return a->id < b->id;});
    for (SuperNode* next : sortedNext) {
#else
    for (SuperNode* next : top->next) {
#endif
      if (times.find(next) == times.end()) times[next] = 0;
      times[next] ++;
      if (times[next] == (int)next->prev.size()) {
        if (dstNode.find(next) != dstNode.end()) potentialRegs.push_back(next);
        else s.push(next);
      }
    }
  }
  /* insert registers */
  sortedSuper.insert(sortedSuper.end(), potentialRegs.begin(), potentialRegs.end());
  /* order sortedSuper */
  for (size_t i = 0; i < sortedSuper.size(); i ++) sortedSuper[i]->order = i + 1;
  int nodeOrder = 0;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      node->order = nodeOrder ++;
    }
  }
}

