#include "graph.h"
#include "common.h"
#include <set>
#include <map>

static std::map<PNode*, Node*> when2Master;
static std::set<Node*> allNodes;

Node* newSuperNode(graph* g, Node* node, int& idx) {
  Node* superNode = new Node(NODE_SUPER);
  superNode->name = "SUPER" + std::to_string(idx ++);
  superNode->setNodes.insert(node);
  superNode->setOrder.push_back(node);
  g->superNodes.push_back(superNode);
  node->master = superNode;
  return superNode;
}

static bool mergeCheck(std::map<Node*, std::set<Node*>>& prevNodes, Node* node, Node* superNode) {
  for (Node* prev : node->prev) {
    if (superNode->setNodes.find(prev) != superNode->setNodes.end()) continue;
    for (Node* member : superNode->setNodes) {
      if (prevNodes[prev].find(member) != prevNodes[prev].end()) return false;
    }
  }
  return true;
}

void updateSuperConnect(graph* g) {
  // update superNode prev & next
  for (Node* superNode : g->superNodes) {
    for (Node* member : superNode->setOrder) {
      for (Node* prev : member->prev) {
        Node* prevNode = prev->master ? prev->master : prev;
        if (superNode->prev.find(prevNode) == superNode->prev.end())
          superNode->inEdge ++;
        superNode->prev.insert(prevNode);
      }
      for (Node* next : member->next) {
        if (next->master == NULL) {
          superNode->next.insert(next);
        } else {
          superNode->next.insert(next->master);
        }
      }
    }
  }
}

void updateArrayMaster(graph* g) {
  for (Node* array : g->array) {
    if (array->arraySplit) continue;
    for (Node* member : array->member) member->master = array->master;
  }
}

void mergeWhen(graph* g) {
  int idx = 0;
  std::vector<Node*>s(g->sources);
  s.insert(s.end(), g->input.begin(), g->input.end());
  s.insert(s.end(), g->constant.begin(), g->constant.end());
  s.insert(s.end(), g->memRdata1.begin(), g->memRdata1.end());
  for (Node* array: g->array) {
    if (!array->arraySplit && array->prev.size() == 0) {
      if (std::find(s.begin(), s.end(), array) == s.end())
        s.push_back(array);
    }
  }

  allNodes.insert(s.begin(), s.end());
  for (Node* n : allNodes) n->scratch = n->inEdge;

  std::map<Node*, std::set<Node*>> prevNodes;

  while(!s.empty()) {
    Node* top = s.back();
    s.pop_back();
    if (g->node2WhenSet.find(top) == g->node2WhenSet.end() || g->node2WhenSet[top].size() != 1) {
      newSuperNode(g, top, idx);
    } else {
      PNode* when = *(g->node2WhenSet[top].begin());
      if (when2Master.find(when) == when2Master.end()) {
        when2Master[when] = newSuperNode(g, top, idx);
      } else if (mergeCheck(prevNodes, top, when2Master[when])){
        Node* master = when2Master[when];
        Assert (master->setNodes.find(top) == master->setNodes.end(), "%s is already visited\n", top->name.c_str());
        master->setNodes.insert(top);
        master->setOrder.push_back(top);
        top->master = master;
      } else {
        newSuperNode(g, top, idx);
      }
    }
    for (Node* next: top->next) {
      if (allNodes.find(next) == allNodes.end()){
        next->scratch = next->inEdge;
        allNodes.insert(next);
      }
      next->scratch --;
      if (!next->scratch && (next->type != NODE_REG_DST && next->type != NODE_ACTIVE)) s.push_back(next);
      if (next->type == NODE_REG_DST || next->type == NODE_ACTIVE) continue;
      prevNodes[next].insert(top);
      prevNodes[next].insert(prevNodes[top].begin(), prevNodes[top].end());
    }
  }

  for (Node* reg : g->sources) {
    Node* superNode = newSuperNode(g, reg->regNext, idx);
    superNode->type = NODE_SUPER_REG_DST;
  }

  for (Node* active : g->active) {
    Node* superNode = newSuperNode(g, active, idx);
    superNode->type = NODE_SUPER_ACTIVE;
  }

  updateSuperConnect(g);
  updateArrayMaster(g);
}

void removeInvalidSuperNodes(graph* g) {
  for (Node* superNode : g->superNodes) {
    superNode->setOrder.erase(superNode->setOrder.begin(), superNode->setOrder.end());
  }
  for (Node* node : g->sorted) {
    node->master->setOrder.push_back(node);
  }

  g->superNodes.erase(std::remove_if(g->superNodes.begin(), g->superNodes.end(), [](const Node* n) { return n->setOrder.size() == 0;}), g->superNodes.end());
  for (Node* superNode : g->superNodes) {
    for (auto iter = superNode->prev.begin(); iter != superNode->prev.end(); ) {
      if ((*iter)->setOrder.size() == 0) iter = superNode->prev.erase(iter);
      else iter ++;
    }
    for (auto iter = superNode->next.begin(); iter != superNode->next.end(); ) {
      if ((*iter)->setOrder.size() == 0) iter = superNode->next.erase(iter);
      else iter ++;
    }
  }
}