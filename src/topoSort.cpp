
#include "graph.h"
#include "common.h"

void topoSort(graph* g) {
  int idx = 0;
  std::vector<Node*> s(g->sources);
  s.insert(s.end(), g->input.begin(), g->input.end());
  s.insert(s.end(), g->constant.begin(), g->constant.end());
  s.insert(s.end(), g->memRdata1.begin(), g->memRdata1.end());

  while(!s.empty()) {
    Node* top = s.back();
    top->id = idx ++;
    g->sorted.push_back(top);
    s.pop_back();
    for(Node* next : top->next) {
      Assert(next->inEdge > 0, "Invalid inEdge %d for node(%s, %d)\n", next->inEdge, next->name.c_str(), next->id);
      next->inEdge --;
      if(!next->inEdge && (next->type != NODE_REG_DST && next->type != NODE_ACTIVE)) s.push_back(next);
    }
  }
  for(Node* node: g->sources) {
    node->regNext->id = idx ++;
    g->sorted.push_back(node->regNext);
  }
  
}