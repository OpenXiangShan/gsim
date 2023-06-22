#include <stdio.h>
#include <stack>
#include <tuple>
#include "common.h"
#include "Node.h"
#include "graph.h"

enum {DEAD=1, VALID};

#define N(i) g->sorted[i]

void removeDeadNodes(graph* g) { // after topo sort
  std::vector<int>info(g->sorted.size(), 0);
  int deadNum = 0;
  for(int i = g->sorted.size()-1; i >= 0; i--) {
    if(N(i)->type == NODE_OTHERS && (N(i)->next.size() == 0 || N(i)->next.size() == info[i])) { // deadNode
      for(Node* n: N(i)->prev) {
        info[n->id] ++;
      }
      deadNum ++;
      N(i)->status = DEAD_NODE;
    }
  }

  std::cout << "find " << deadNum << " deadNodes( " << g->sorted.size() << " )\n";
}