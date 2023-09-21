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
#include <queue>

enum { DEAD = 1, VALID };

#define N(i) g->sorted[i]

void removeDeadNodes(graph* g) {  // after topo sort
  std::vector<size_t> info(g->sorted.size(), 0);
  int deadNum = 0;

  std::queue<Node*>s;
  for (size_t i = 0; i < g->sorted.size(); i ++) {
    if (N(i)->dimension.size() != 0 || N(i)->type == NODE_ARRAY_MEMBER) continue; // TODO
    Assert(N(i)->id >= 0 && N(i)->id < (int)g->sorted.size(), "%s id %d\n", N(i)->name.c_str(), N(i)->id);
    if ((N(i)->type == NODE_OTHERS || (N(i)->type == NODE_REG_DST && !N(i)->regNext->regSplit)) &&
                    N(i)->next.size() == 0)
        s.push(N(i));
  }
  while (!s.empty()) {
    Node* top = s.front();
    s.pop();
    top->status = DEAD_NODE;
    for (Node* prev : top->prev) {
      if (prev->id < 0 || prev->status != VALID_NODE) continue;
      if (prev->type == NODE_OTHERS || (prev->type == NODE_REG_DST && !prev->regNext->regSplit)) {
        info[prev->id ] ++;
        if (prev->next.size() == info[prev->id]) s.push(prev);
      }
    }
  }

  std::cout << "find " << deadNum << " deadNodes( " << g->sorted.size() << " )\n";

#if 0
  for (Node* superNode : g->superNodes) {
    std::cout << superNode->name << " " << superNode->id << ": ";
    for (Node* member : superNode->setOrder) std::cout << member->name <<  "(" << member->id << ", " << (member->type == NODE_INVALID ? "invalid " : "valid ") << member->prev.size()<< " " << member->next.size() << ") ";
    std::cout << "\nprev: " << std::endl;
    for (Node* prevSuper : superNode->prev) std::cout << "  " <<prevSuper->name << " " << prevSuper->id << std::endl;
    std::cout << "next: " << std::endl;
    for (Node* nextSuper : superNode->next) std::cout << "  " <<nextSuper->name << " " << nextSuper->id << std::endl;
  }
#endif
}
