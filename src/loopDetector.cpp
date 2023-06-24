#include "common.h"
#include <stack>
#include <graph.h>
#include <Node.h>

#define NOT_VISIT 0
#define EXPANDED 1
#define VISITED 2

void loopDetector(graph* g) {
  std::stack<Node*> s;

  for (size_t i = 0; i < g->constant.size(); i++) {
    s.push(g->constant[i]);
    g->constant[i]->visited = NOT_VISIT;
  }

  for (size_t i = 0; i < g->sources.size(); i++) {
    s.push(g->sources[i]);
    g->sources[i]->visited = NOT_VISIT;
  }

  for (size_t i = 0; i < g->input.size(); i++) {
    s.push(g->input[i]);
    g->input[i]->visited = NOT_VISIT;
  }
  while(!s.empty()) {
    Node* top = s.top();
    if(top->visited == EXPANDED) {
      top->visited = VISITED;
      s.pop();
    } else if(top->visited == NOT_VISIT) {
      top->visited = EXPANDED;
      for(Node* n : top->next) {
        if(n->visited == EXPANDED) {
          std::cout << "detect Loop: \n";
          std::cout << n->name << std::endl;
          while(!s.empty()) {
            Node* info = s.top(); s.pop();
            if(info->visited == EXPANDED) std::cout << info->name  << std::endl;
            if(info == n) return;
          }
        } else if (n->visited == NOT_VISIT) {
          s.push(n);
        }
      }

    } else {
      s.pop();
    }
  }
  std::cout << "NO Loop!\n";
}