/*
 determine whether two nodes refer to the same value, keep only one copy of it
*/

#include "common.h"
#include <stack>
#include <map>

/* TODO: array alias */
/* TODO: A = B[idx] */
ENode* Node::isAlias() {
  if (type != NODE_OTHERS) return nullptr;
  if (isArray()) {
    /* alias array A to array B iff
      A <= B (if idx = dim[0], A.arrayval[idx] = B, otherwise A.arrayval[idx] = null ) (Done)
      A[i] <= B[i] for every i  (WIP)
    */
    if (prev.size() == 1 && assignTree.size() == 1 && assignTree[0]->getlval()->getChildNum() == 0 && assignTree[0]->getRoot()->getNode()
        && assignTree[0]->getRoot()->getNode()->type == NODE_OTHERS) {
      return assignTree[0]->getRoot();
    }
    return nullptr;
  }
  if (assignTree.size() != 1) return nullptr;
  if (!assignTree[0]->getRoot()->getNode()) return nullptr;
  if (prev.size() != 1) return nullptr;
  return assignTree[0]->getRoot();
}

ENode* ENode::mergeSubTree(ENode* newSubTree) {
  ENode* ret = newSubTree->dup();
  for (ENode* childENode : child) ret->addChild(childENode->dup());
  return ret;
}

void ExpTree::replace(std::map<Node*, ENode*>& aliasMap) {
  std::stack<ENode*> s;
  Node* node = getRoot()->getNode();
  if(aliasMap.find(node) != aliasMap.end()) {
    int width = getRoot()->width;
    setRoot(getRoot()->mergeSubTree(aliasMap[node]));
    getRoot()->width = width;
  }
  s.push(getRoot());

  if (getlval()) s.push(getlval());

  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (size_t i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
      s.push(top->getChild(i));
      Node* replaceNode = top->getChild(i)->getNode();
      if (!replaceNode) continue;
      if (aliasMap.find(replaceNode) != aliasMap.end()) {
        ENode* newChild = top->getChild(i)->mergeSubTree(aliasMap[replaceNode]);
        newChild->width = top->getChild(i)->width;
        top->setChild(i, newChild);
      }
    }
  }
}
/*
A -> |         | -> E
     | C  -> D |
B -> |         | -> F
           remove   
*/
void graph::aliasAnalysis() {
  /* counters */
  size_t totalNodes = 0;
  size_t aliasNum = 0;
  size_t totalSuper = sortedSuper.size();
  std::map<Node*, ENode*> aliasMap;
  for (SuperNode* super : sortedSuper) {
    totalNodes += super->member.size();
    for (Node* member : super->member) {
      if (member->status != VALID_NODE) continue;
      ENode* enode = member->isAlias(); // may be a[0]
      if (!enode) continue;
      aliasNum ++;
      Node* aliasNode = enode->getNode();
      ENode* aliasENode = enode;
      if (aliasMap.find(aliasNode) != aliasMap.end()) {
        aliasENode = enode->mergeSubTree(aliasMap[aliasNode]);
      }
      member->status = DEAD_NODE;
      aliasMap[member] = aliasENode;
    }
  }
  /* update assignTree */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == DEAD_NODE) continue;
      for (ExpTree* tree : member->assignTree) tree->replace(aliasMap);
      if (member->resetTree) member->resetTree->replace(aliasMap);
    }
  }

  removeNodesNoConnect(DEAD_NODE);
  reconnectAll();
  printf("[aliasAnalysis] remove %ld alias (%ld -> %ld)\n", aliasNum, totalNodes, totalNodes - aliasNum);
  printf("[aliasAnalysis] remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}
