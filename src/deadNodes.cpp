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
    if (N(i)->dimension.size() != 0 || N(i)->type == NODE_ARRAY_MEMBER) continue; // TODO
    Assert(N(i)->id >= 0 && N(i)->id < (int)g->sorted.size(), "%s id %d\n", N(i)->name.c_str(), N(i)->id);
    bool isDeadNode = (N(i)->type == NODE_OTHERS && (N(i)->next.size() == 0 || N(i)->next.size() == info[N(i)->id])) ||
                      (N(i)->type == NODE_REG_SRC && (N(i)->next.size() == 0 || N(i)->next.size() == info[N(i)->id]));

    if (isDeadNode == true) {  // deadNode
      Node* node = N(i)->type == NODE_REG_SRC ? N(i)->regNext : N(i);
      for (Node* n : node->prev) {
        if (n->id < 0) {
          Assert(n->type == NODE_ARRAY_MEMBER, "%s %d type %d\n", n->name.c_str(), n->id, n->type);
          continue;
        }
        Assert(n->id >= 0 && n->id < (int)g->sorted.size(), "%s(%d %d) -> %s\n", n->name.c_str(), n->id, n->clusId, N(i)->name.c_str());
        info[n->id]++;
      }
      deadNum++;
      N(i)->status = DEAD_NODE;
      node->status = DEAD_NODE;
      // std::cout << "deadNode: " << N(i)->name << " " << N(i)->id<< std::endl;
    }
  }

  std::cout << "find " << deadNum << " deadNodes( " << g->sorted.size() << " )\n";
}
