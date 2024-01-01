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

  while(!s.empty()) {
    SuperNode* top = s.top();
    s.pop();
    sortedSuper.push_back(top);
    for (SuperNode* next : top->next) {
      if (times.find(next) == times.end()) times[next] = 0;
      times[next] ++;
      if (times[next] == (int)next->prev.size()) s.push(next);
    }
  }
}

