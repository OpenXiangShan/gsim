/*
  Remove deadNodes. A node is a deadNode if it has no successor or all successors are also deadNodes.
  must be called after topo-sort
*/
#include "common.h"
#include <stack>

static std::set<Node*> nodesInUpdateTree;

static inline bool potentialDead(Node* node) {
  return (node->type == NODE_OTHERS || node->type == NODE_REG_SRC || (node->type == NODE_MEM_MEMBER && node->parent->type == NODE_READER)) && nodesInUpdateTree.find(node) == nodesInUpdateTree.end();
}

void getRepyNodes(ENode* enode, std::set<Node*>& allNodes) {
  std::stack<ENode*> s;
  s.push(enode);
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    Node* prevNode = top->getNode();
    if (prevNode) {
      if (prevNode->isArray() && prevNode->arraySplitted()) {
        ArrayMemberList* list = top->getArrayMember(prevNode);
        for (Node* arrayMember : list->member) {
          allNodes.insert(arrayMember);
        }
      } else {
        allNodes.insert(prevNode);
      }
    }
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) s.push(top->getChild(i));
    }
  }
}

void ExpTree::getRelyNodes(std::set<Node*>& allNodes) {
  getRepyNodes(getRoot(), allNodes);
}


void graph::removeDeadNodes() {
  /* counters */
  size_t totalNodes = 0;
  size_t deadNum = 0;
  size_t totalSuper = sortedSuper.size();

  for (Node* reg : regsrc) {
    if (reg->resetTree) reg->resetTree->getRelyNodes(nodesInUpdateTree);
  }

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
  removeNodes(DEAD_NODE);
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );

  printf("[removeDeadNodes] remove %ld deadNodes (%ld -> %ld)\n", deadNum, totalNodes, totalNodes - deadNum);
  printf("[removeDeadNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}