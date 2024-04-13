/*
 determine whether two nodes refer to the same value, keep only one copy of it
*/

#include "common.h"
#include <stack>
#include <map>

/* TODO: array alias */
/* TODO: A = B[idx] */
ENode* Node::isAlias() {
  if (type != NODE_OTHERS && type != NODE_ARRAY_MEMBER) return nullptr;
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
  if (assignTree.size() != 1) return nullptr;
  if (!assignTree[0]->getRoot()->getNode()) return nullptr;
  if (prev.size() != 1) return nullptr;
  if (type == NODE_ARRAY_MEMBER && assignTree[0]->getRoot()->getNode()->isArray() && !assignTree[0]->getRoot()->getNode()->arraySplitted()) return nullptr;
  return assignTree[0]->getRoot();
}

ENode* ENode::mergeSubTree(ENode* newSubTree) {
  ENode* ret = newSubTree->dup();
  for (ENode* childENode : child) ret->addChild(childENode->dup());
  return ret;
}

Node* getLeafNode(bool isArray, ENode* enode) {
  if (!enode->getNode()) return nullptr;
  Node* node = enode->getNode();
  if (node->isArray() && node->arraySplitted()) {
    int beg, end;
    std::tie(beg, end) = enode->getIdx(node);
    if (beg < 0 || (isArray && enode->getChildNum() != node->dimension.size())) return node;
    else return node->getArrayMember(beg);
  }
  return node;
}

void ExpTree::replace(std::map<Node*, ENode*>& aliasMap, bool isArray) {
  std::stack<ENode*> s;
  Node* leafNode = getLeafNode(isArray, getRoot());
  if(aliasMap.find(leafNode) != aliasMap.end()) {
    if (leafNode == getRoot()->getNode())
      setRoot(getRoot()->mergeSubTree(aliasMap[leafNode]));
    else
      setRoot(aliasMap[leafNode]->dup());
  }
  s.push(getRoot());

  if (getlval()) s.push(getlval());

  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
      s.push(top->getChild(i));
      if (!top->getChild(i)->getNode()) continue;
      Node* replaceNode = getLeafNode(isArray, top->getChild(i));
      if (aliasMap.find(replaceNode) != aliasMap.end()) {
        ENode* newChild;
        if (replaceNode != top->getChild(i)->getNode()) { // splitted array member
          newChild = aliasMap[replaceNode]->dup();
        } else {
          newChild = top->getChild(i)->mergeSubTree(aliasMap[replaceNode]);
        }
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
      ENode* enode = member->isAlias();
      if (!enode) continue;
      aliasNum ++;
      Node* interNode = getLeafNode(member->isArray(), enode);//->getNode();
      ENode* aliasENode = enode;
      if (aliasMap.find(interNode) != aliasMap.end()) {
        if (interNode == enode->getNode()) aliasENode = enode->mergeSubTree(aliasMap[interNode]);
        else aliasENode = aliasMap[interNode]->dup();
      }
      if (member->type == NODE_ARRAY_MEMBER && aliasENode->getNode()->isArray() && !aliasENode->getNode()->arraySplitted()) {
        /* do not alias array member to */
      } else {
        member->status = DEAD_NODE;
        aliasMap[member] = aliasENode;
      }
    }
  }
  /* update valTree */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == DEAD_NODE) continue;
      for (ExpTree* tree : member->arrayVal) {
        if (tree) tree->replace(aliasMap, member->isArray());
      }
      for (ExpTree* tree : member->assignTree) tree->replace(aliasMap, member->isArray());
    }
  }

  for (Node* reg : regsrc) {
    if (reg->updateTree) {
      reg->updateTree->replace(aliasMap, reg->isArray());
    }
  }
  for (auto iter : aliasMap) {
    if(iter.first->type == NODE_ARRAY_MEMBER) {
      Node* parent = iter.first->arrayParent;
      parent->arrayMember[iter.first->arrayIdx] = getLeafNode(false, iter.second);
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