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
  }// For a Scalar node to be a 'my_alias = value' form
  if (assignTree.size() != 1) return nullptr; // It has only one assignTree
  if (!assignTree[0]->getRoot()->getNode()) return nullptr; // It's root is already a leaf ENode (not a compute)
  if (prev.size() != 1) return nullptr; // It only rely on one node (no cond, nor dyn array)
  return assignTree[0]->getRoot(); // Then 'my_alias' is a alias of 'value' so return 'value'
}

ENode* ENode::mergeSubTree(ENode* newSubTree) {
  // We can't just change this->nodePtr: for y=a[0] and z=add(y,1)
  // changing E(y)->nodePtr with a would yield E(a) instead of E(a[0])// so we must clone newSubTree first
  ENode* ret = newSubTree->dup();
  // For b=a; c=b[x]; this is E(b[x]), newSubTree is E(a), and [x] is in the child of 'this', addChild so ret is a[x]
  // This function also handle b=a[0] c=b[x], ret will be E(a[0][x])
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
      Node* replaceNode = top->getChild(i)->getNode();
      if (replaceNode && aliasMap.find(replaceNode) != aliasMap.end()) {
        ENode* newChild = top->getChild(i)->mergeSubTree(aliasMap[replaceNode]);
        newChild->width = top->getChild(i)->width;
        top->setChild(i, newChild);
      }
      s.push(top->getChild(i)); // This ENode or newChild may contains alias
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
      Node* aliasNode = enode->getNode(); // the Node being aliased
      ENode* aliasENode = enode; // the Enode containing the Node being aliased
      if (aliasMap.find(aliasNode) != aliasMap.end()) { // Is such Node already an alias?
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
