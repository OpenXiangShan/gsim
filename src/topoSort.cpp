
#include "graph.h"
#include "common.h"

void topoSort(graph* g) {
  int time = 1;
  std::vector<Node*> s(g->sources);
  s.insert(s.end(), g->input.begin(), g->input.end());
  // for (Node* node : g->sources) {
  //   node->timestamp = time ++;
  // }
  while(!s.empty()) {
    Node* top = s.back();
    g->sorted.push_back(top);
    s.pop_back();
    for(Node* next : top->next) {
      Assert(next->inEdge > 0, "Invalid inEdge %d for node(%d)\n", next->inEdge, next->id);
      next->inEdge --;
      if(!next->inEdge) s.push_back(next);
    }
  }
  
}