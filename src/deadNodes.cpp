/**
 * @file deadNodes.cpp
 * @brief 从计算图g中删除死节点.
 * 死节点是指没有输出, 或者所有的输出都是死节点的节点. 删除死节点可以简化计算图,
 * 减少计算时间和内存开销.
 */

#include <stdio.h>
#include <stack>
#include <tuple>

#include "common.h"
#include "Node.h"
#include "graph.h"

enum { DEAD = 1, VALID };

#define N(i) g->sorted[i]

void removeDeadNodes(graph* g) {  // after topo sort
  std::vector<size_t> info(g->sorted.size(), 0);

  int deadNum = 0;

  for (int i = g->sorted.size() - 1; i >= 0; i--) {
    bool isDeadNode =
        N(i)->type == NODE_OTHERS && (N(i)->next.size() == 0 || N(i)->next.size() == info[i]);

    if (isDeadNode == true) {  // deadNode

      for (Node* n : N(i)->prev) {
        info[n->id]++;
      }

      deadNum++;

      N(i)->status = DEAD_NODE;
    }
  }

  int idx = 0;
  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (N(i)->status != DEAD_NODE) N(idx++) = N(i);
  }

  std::cout << "find " << deadNum << " deadNodes( " << g->sorted.size() << " )\n";

  g->sorted.erase(g->sorted.begin() + idx, g->sorted.end());
}
