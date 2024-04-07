/*
 determine whether two nodes refer to the same value, keep only one copy of it
*/

#include "common.h"
#include <stack>
#include <map>

/* TODO: array alias */
/* TODO: A = B[idx] */
ENode* Node::isAlias() {
  if (isArray()) {
    /* alias array A to array B iff
      A <= B (if idx = dim[0], A.arrayval[idx] = B, otherwise A.arrayval[idx] = null ) (Done)
      A[i] <= B[i] for every i  (WIP)
    */
    if (prev.size() == 1 && arrayVal.size() == 1 && arrayVal[0]->getlval()->getChildNum() == 0 && arrayVal[0]->getRoot()->getNode()
        && arrayVal[0]->getRoot()->getNode()->type == NODE_OTHERS) {
      return arrayVal[0]->getRoot();
    }
    return nullptr;
  }
  if (type != NODE_OTHERS) return nullptr;
  if (!valTree) return nullptr;
  if (!valTree->getRoot()->getNode()) return nullptr;
  if (prev.size() != 1) return nullptr;
  return valTree->getRoot();
}

ENode* ENode::mergeSubTree(ENode* newSubTree) {
  ENode* ret = newSubTree->dup();
  for (ENode* childENode : child) ret->addChild(childENode->dup());
  return ret;
}

/* TODO: array with index */
Node* ENode::getLeafNode(std::set<Node*>& s) {
  if (!getNode()) return nullptr;

  for (ENode* childENode : child) childENode->getLeafNode(s);

  Node* node = getNode();
  if (node->isArray() && node->arraySplitted()) {
    int idx = getArrayIndex(node);
    if (s.size() != 0) TODO(); // splitted array accessed by variable index
    s.insert(node->arrayMember[idx]);
    return node->arrayMember[idx];
  } else s.insert(node);
  return node;
}

void ExpTree::replace(std::map<Node*, ENode*>& aliasMap) {
  std::stack<ENode*> s;

  if(aliasMap.find(getRoot()->getNode()) != aliasMap.end()) {
    setRoot(getRoot()->mergeSubTree(aliasMap[getRoot()->getNode()]));
  }
  s.push(getRoot());

  if (getlval()) s.push(getlval());

  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
      if (aliasMap.find(top->getChild(i)->getNode()) != aliasMap.end()) {
        ENode* newChild = top->getChild(i)->mergeSubTree(aliasMap[top->getChild(i)->getNode()]);
        newChild->width = top->getChild(i)->width;
        top->setChild(i, newChild);
      }
      s.push(top->getChild(i));
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
      ENode* enode = member->isAlias();
      if (!enode) continue;
      aliasNum ++;

      member->status = DEAD_NODE;
      if (aliasMap.find(enode->getNode()) != aliasMap.end()) aliasMap[member] = enode->mergeSubTree(aliasMap[enode->getNode()]);
      else aliasMap[member] = enode;
    }
  }
  /* update valTree */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      for (ExpTree* tree : member->arrayVal) {
        if (tree) tree->replace(aliasMap);
      }
      if (member->valTree) member->valTree->replace(aliasMap);
    }
  }

  for (Node* reg : regsrc) {
    if (reg->updateTree) {
      reg->updateTree->replace(aliasMap);
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }
  removeNodesNoConnect(DEAD_NODE);
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->updateConnect();
    }
  }
  updateSuper();

  printf("remove %ld alias (%ld -> %ld)\n", aliasNum, totalNodes, totalNodes - aliasNum);
  printf("remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}