/*
  Remove deadNodes. A node is a deadNode if it has no successor or all successors are also deadNodes.
  must be called after topo-sort
*/
#include "common.h"
#include <stack>

static std::set<Node*> nodesInUpdateTree;

void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes) {
  std::stack<ENode*> s;
  s.push(enode);
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    Node* prevNode = top->getNode();
    if (prevNode) allNodes.insert(prevNode);
    for (size_t i = 0; i < top->getChildNum(); i ++) {
      if (top->getChild(i)) s.push(top->getChild(i));
    }
  }
}

void ExpTree::getRelyNodes(std::set<Node*>& allNodes) {
  getENodeRelyNodes(getRoot(), allNodes);
  for (ENode* child : getlval()->child) {
    getENodeRelyNodes(child, allNodes);
  }
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
  std::set<Node*> visited;
  std::stack<Node*> s;
  auto add = [&visited, &s](Node* node) {
    if (visited.find(node) == visited.end()) {
      s.push(node);
      visited.insert(node);
    }
  };
  for (Node* outNode : output) add(outNode);
  for (Node* special : specialNodes) add(special);
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->type == NODE_EXT) add(member);
    }
  }
  while (!s.empty()) {
    Node* top = s.top();
    s.pop();
    for (Node* prev : top->prev) {
      add(prev);
    }
    if (top->type == NODE_REG_SRC) {
      add(top->getDst());
      std::set<Node*> resetNodes;
      if (top->resetTree) top->resetTree->getRelyNodes(resetNodes);
      for (Node* node : resetNodes) add(node);
    } else if (top->type == NODE_READER) {
      Node* memory = top->parent;
      for (Node* port : memory->member) {
        if (port->type == NODE_WRITER) add(port);
        if (port->type == NODE_READWRITER) add(port);
      }
    } else if (top->type == NODE_EXT_OUT) {
      Node* ext = top->parent;
      for (Node* member : ext->member) add(member);
    }
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->type == NODE_INP || node->type == NODE_OUT) continue;
      if (visited.find(node) == visited.end()) {
        node->status = DEAD_NODE;
      }
    }
  }

  /* counters */
  size_t totalNodes = countNodes();
  size_t totalSuper = sortedSuper.size();

  removeNodes(DEAD_NODE);
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );

  for (size_t i = 0; i < memory.size(); i ++) {
    Node* mem = memory[i];
    mem->member.erase(
      std::remove_if(mem->member.begin(), mem->member.end(), [](const Node* n) {return n->status == DEAD_NODE; }),
      mem->member.end()
    );
    if (mem->member.size() == 0) {
      mem->status = DEAD_NODE;
      memory.erase(memory.begin() + i);
      i --;
    }
  }
  reconnectAll();

  size_t optimizedNodes = countNodes();
  size_t optimizedSuper = sortedSuper.size();

  printf("[removeDeadNodes] remove %ld deadNodes (%ld -> %ld)\n", totalNodes - optimizedNodes, totalNodes, optimizedNodes);
  printf("[removeDeadNodes] remove %ld superNodes (%ld -> %ld)\n", totalSuper - optimizedSuper, totalSuper, optimizedSuper);

}
