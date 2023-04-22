
#include "graph.h"
#include "common.h"

void topoSort(graph* g) {
  int idx = 0;
  std::vector<Node*> s(g->sources);
  s.insert(s.end(), g->input.begin(), g->input.end());
  s.insert(s.end(), g->constant.begin(), g->constant.end());
  // for (Node* node : g->sources) {
  //   node->timestamp = time ++;
  // }
  while(!s.empty()) {
    Node* top = s.back();
    top->id = idx ++;
    g->sorted.push_back(top);
    s.pop_back();
    for(Node* next : top->next) {
      Assert(next->inEdge > 0, "Invalid inEdge %d for node(%d)\n", next->inEdge, next->id);
      next->inEdge --;
      if(!next->inEdge && next->type != NODE_REG_DST) s.push_back(next);
    }
  }
  for(Node* node: g->sources) {
    node->regNext->id = idx ++;
    g->sorted.push_back(node->regNext);
  }
  
}