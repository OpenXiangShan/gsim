/*
  Remove deadNodes. A node is a deadNode if it has no successor or all successors are also deadNodes.
  must be called after topo-sort
*/
#include "common.h"
#include <stack>

static inline bool potentialDead(Node* node) {
  return node->type == NODE_OTHERS || node->type == NODE_REG_SRC;
}

void graph::removeDeadNodes() {
  /* counters */
  size_t totalNodes = 0;
  size_t deadNum = 0;
  size_t totalSuper = sortedSuper.size();

  std::stack<Node*> s;
  for (SuperNode* super : sortedSuper) {
    totalNodes += super->member.size();
    for (Node* member : super->member) {
      if (member->next.size() == 0 && potentialDead(member)) s.push(member);
    }
  }
  while(!s.empty()) {
    deadNum ++;
    Node* top = s.top();
    s.pop();
    if (top->type == NODE_REG_SRC) s.push(top->getDst());
    top->status = DEAD_NODE;
    for (Node* prev : top->prev) {
      /* remove node connection */
      Assert(prev->next.find(top) != prev->next.end(), "next %s is not in prev(%s)->next", top->name.c_str(), prev->name.c_str());
      prev->next.erase(top);
      if (prev->next.size() == 0 && potentialDead(prev)) {
        s.push(prev);
      }
    }
  }
  updateSuper();

  printf("remove %ld deadNodes (%ld -> %ld)\n", deadNum, totalNodes, totalNodes - deadNum);
  printf("remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}