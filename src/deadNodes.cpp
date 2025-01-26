/*
  Remove deadNodes. A node is a deadNode if it has no successor or all successors are also deadNodes.
  must be called after topo-sort
*/
#include "common.h"
#include <stack>

static std::set<Node*> nodesInUpdateTree;

static inline bool potentialDead(Node* node) {
  return (node->type == NODE_OTHERS || node->type == NODE_REG_SRC || node->type == NODE_READER) && nodesInUpdateTree.find(node) == nodesInUpdateTree.end();
}

static inline bool isSink(Node* node) {
  return node->type == NODE_REG_DST || node->type == NODE_SPECIAL || node->type == NODE_OUT ||
        node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER ||
        node->type == NODE_EXT_IN;
}

void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes) {
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
    for (size_t i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) s.push(top->getChild(i));
    }
  }
}

void ExpTree::getRelyNodes(std::set<Node*>& allNodes) {
  getENodeRelyNodes(getRoot(), allNodes);
}

bool anyOuterEdge(Node* node) {
  bool ret = false;
  for (Node* next : node->next) {
    if (next == node || (node->type == NODE_REG_SRC && next == node->getDst())
      || next->status == DEAD_NODE /* point to dead registers */) continue;
    ret = true;
  }
  return ret;
}

void graph::removeDeadNodes() {
  removeDeadReg();
  /* counters */
  size_t totalNodes = 0;
  size_t deadNum = 0;
  size_t totalSuper = sortedSuper.size();

  for (Node* reg : regsrc) {
    if (reg->resetTree) reg->resetTree->getRelyNodes(nodesInUpdateTree);
  }

  std::stack<Node*> s;
  std::set<Node*> visited;
  for (SuperNode* super : sortedSuper) {
    totalNodes += super->member.size();
    for (Node* member : super->member) {
      for (Node* next : member->next) {
        if (next->status == DEAD_NODE) member->next.erase(next);
      }
      if (!anyOuterEdge(member) && potentialDead(member)) {
        s.push(member);
      }
    }
  }
  while(!s.empty()) {
    deadNum ++;
    Node* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    if (top->type == NODE_REG_SRC) s.push(top->getDst());
    top->status = DEAD_NODE;
    for (Node* prev : top->prev) {
      /* remove node connection */
      Assert(prev->next.find(top) != prev->next.end(), "node %s is not in prev(%s)->next", top->name.c_str(), prev->name.c_str());
      prev->next.erase(top);
      if (!anyOuterEdge(prev) && potentialDead(prev)) {
        s.push(prev);
      }
    }
    top->prev.clear();
  }
  removeNodes(DEAD_NODE);
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );

  printf("[removeDeadNodes] remove %ld deadNodes (%ld -> %ld)\n", deadNum, totalNodes, totalNodes - deadNum);
  printf("[removeDeadNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}

void graph::removeDeadReg() {
  std::map<Node*, std::pair<int, Node*>> dstMap; /* regieters that first point to */
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    for (int j = sortedSuper[i]->member.size() - 1; j >= 0; j--) {
      Node* node = sortedSuper[i]->member[j];
      if (isSink(node)) dstMap[node] = std::make_pair(1, node);
      else {
        dstMap[node] = std::make_pair(0, nullptr);
        for (Node* next : node->next) {
          if (dstMap[next].first >= 2) {
            dstMap[node].first = 2;
            break;
          } else if (dstMap[next].first == 1) {
            if (dstMap[node].first == 1 && dstMap[node].second == dstMap[next].second) continue;
            else if (dstMap[node].first == 1) {
              dstMap[node].first = 2;
              break;
            } else {
              dstMap[node] = dstMap[next];
            }
          }

        }
      }
    }
  }
  std::stack<Node*> checkNodes;
  std::set<Node*> visited;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->type == NODE_REG_SRC && dstMap[node].first == 1 && dstMap[node].second == node->getDst()) {
        checkNodes.push(node);
        checkNodes.push(node->getDst());
      }
    }
  }
  while (!checkNodes.empty()) {
    Node* top = checkNodes.top();
    checkNodes.pop();
    top->status = DEAD_NODE;
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    if (top->type == NODE_REG_SRC) checkNodes.push(top->getDst());
    for (Node* prev : top->prev) {
      prev->next.erase(top);
      if (!prev->anyExtEdge() && potentialDead(prev)) checkNodes.push(prev);
    }
    top->prev.clear();
  }
  removeNodes(DEAD_NODE);
  reconnectAll();
}
