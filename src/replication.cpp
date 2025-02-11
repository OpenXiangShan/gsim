/* remove nodes by replication */
#include "common.h"
#include <cstdio>
#include <stack>

/* non-negative if node is replicated and -1 if node is not */
static std::map<Node*, int> opNum;
Node* getLeafNode(bool isArray, ENode* enode);
void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

void ExpTree::replace(Node* oldNode, Node* newNode) {
  std::stack<ENode*> s;
  if (getRoot()) s.push(getRoot());
  if (getlval()) s.push(getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->getNode() && getLeafNode(true, top) == oldNode) {
      top->nodePtr = newNode;
      top->child.clear();
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
}

int Node::repOpCount() {
  if (isArray() || type != NODE_OTHERS || assignTree.size() > 1) return -1;
  if (status == CONSTANT_NODE) return 0;
  int ret = 0;
  std::stack<ENode*> s;
  for (ExpTree* tree : assignTree) s.push(tree->getRoot());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->nodePtr) {
      Node* leafNode = getLeafNode(true, top);
      if (leafNode->isArray()) {
        for (ENode* child : top->child) {
          if (child->width > BASIC_WIDTH) return -1;
          else ret ++;
        }
      }
      if (opNum[leafNode] >= 0) ret += opNum[leafNode];
      continue;
    }
    else if (top->opType == OP_INT) continue;
    else if (top->opType == OP_WHEN) return -1;
    if (top->width > BASIC_WIDTH) return -1;
    ret ++;
    for (ENode* child : top->child) {
      if (child) s.push(child);
    }
  }
  return ret;
}

bool graph::replicationOpt() {
  size_t optimizeNum = 0;
  size_t oldNum = countNodes();
  size_t oldSuper = sortedSuper.size();
  std::set<Node*> mustNodes;
  for (SuperNode* super : sortedSuper) { // TODO: support op in index
    for (Node* member : super->member) {
      std::set<Node*> rely;
      for (ExpTree* tree : member->arrayVal) {
        getENodeRelyNodes(tree->getlval(), rely);
      }
      rely.erase(member);
      mustNodes.insert(rely.begin(), rely.end());
      for (Node* next : member->next) {
        if (next->isArray()) mustNodes.insert(member);
      }
    }
  }
  std::vector<Node*> repNodes;
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      int op = member->repOpCount();
      int threadHold = super->member.size() == 1 ? 3 : 0;
      if (mustNodes.find(member) != mustNodes.end() || op < 0 || op * (int)member->next.size() >= threadHold || !member->anyExtEdge()) {
        opNum[member] = -1; // mark node is valid
      } else {
        opNum[member] = op; // mark node is replicated
        repNodes.push_back(member);
        optimizeNum ++;
      }
    }
  }
  /* remove replication nodes and update connections */
  for (int i = repNodes.size() - 1; i >= 0; i --) {
    Node* node = repNodes[i];
    std::map<SuperNode*, std::vector<Node*>> nextSuper;
    bool remainNode = false;
    for (Node* next : node->next) {
      if (nextSuper.find(next->super) == nextSuper.end()) nextSuper[next->super] = std::vector<Node*>();
      nextSuper[next->super].push_back(next);
      if (next->super == node->super) remainNode = true;
    }
    if (!remainNode) node->status = REPLICATION_NODE;
    int repIdx = 1;
    for (auto iter : nextSuper) {
      SuperNode* super = iter.first;
      if (super == node->super) continue;
      std::string dupName = format("%s$DUP_%d", node->name.c_str(), repIdx ++);
      Node* repNode = node->dup(node->type, dupName);
      for (Node* prev : node->prev) prev->next.insert(repNode);
      repNode->assignTree.push_back(new ExpTree(node->assignTree[0]->getRoot()->dup(), new ENode(repNode)));
      super->member.insert(super->member.begin(), repNode);
      repNode->super = super;
      for (Node* next : iter.second) {
        for (ExpTree* tree : next->assignTree) tree->replace(node, repNode);
        for (ExpTree* tree : next->arrayVal) tree->replace(node, repNode);
      }
    }
  }
  removeNodesNoConnect(REPLICATION_NODE);
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->updateConnect();
    }
  }
  removeNodes(REPLICATION_NODE);
  printf("[replication] remove %ld nodes (%ld -> %ld)\n", optimizeNum, oldNum, countNodes());
  printf("[replication] remove %ld superNodes (%ld -> %ld)\n", oldSuper - sortedSuper.size(), oldSuper, sortedSuper.size());
  return (optimizeNum > 0) || (oldSuper - sortedSuper.size() > 0);
}
