/*
  sort superNodes in topological order
*/

#include "common.h"
#include <stack>
#include <map>

void graph::topoSort() {
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
    for (SuperNode* next : top->next) {
      if (times.find(next) == times.end()) times[next] = 0;
      times[next] ++;
      if (times[next] == (int)next->prev.size()) {
        if (next->next.size() == 0) potentialRegs.push_back(next);
        else s.push(next);
      }
    }
  }
  /* insert registers */
  sortedSuper.insert(sortedSuper.end(), potentialRegs.begin(), potentialRegs.end());
  /* order sortedSuper */
  for (size_t i = 0; i < sortedSuper.size(); i ++) sortedSuper[i]->order = i + 1;
}

