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

/* replace ENode points to oldnode to newSubTree*/
void ExpTree::replace(Node* oldNode, ENode* newSubTree) {
  Assert(!getlval() || getlval()->getNode() != oldNode, "invalid lvalue");

  std::stack<ENode*> s;

  if(getRoot()->getNode() == oldNode) {
    setRoot(getRoot()->mergeSubTree(newSubTree));
  } else {
    s.push(getRoot());
  }
  if (getlval()) s.push(getlval());

  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
      if (top->getChild(i)->getNode() == oldNode) {
        ENode* newChild = top->getChild(i)->mergeSubTree(newSubTree);
        newChild->width = top->getChild(i)->width;
        top->setChild(i, newChild);
      } else {
        s.push(top->getChild(i));
      }
    }
  }
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

void ExpTree::replaceUpdateTree(std::map<Node*, ENode*>& aliasMap) {
  std::stack<ENode*> s;

  if(aliasMap.find(getRoot()->getNode()) != aliasMap.end()) {
    setRoot(getRoot()->mergeSubTree(aliasMap[getRoot()->getNode()]));
  } else {
    s.push(getRoot());
  }
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
      std::set<Node*> leafNodes;
      Node* origin = enode->getLeafNode(leafNodes);

      Assert(member->prev.size() == 1 && *(member->prev.begin()) == origin, "%s (prev %ld) prev %s != %s",
              member->name.c_str(), member->prev.size(), (*(member->prev.begin()))->name.c_str(), origin->name.c_str());

      /* update connection */
      for (Node* leaf : leafNodes) {
        leaf->next.insert(member->next.begin(), member->next.end());
        leaf->next.erase(member);

      }
      for (Node* next : member->next) {
        next->prev.erase(member);
        next->prev.insert(leafNodes.begin(), leafNodes.end());
      }
      /* update valTree */
      for (Node* next : member->next) {
        for (ExpTree* tree : next->arrayVal) {
          if (tree)
            tree->replace(member, enode);
        }
        if (next->valTree) next->valTree->replace(member, enode);
        if ((next->type == NODE_REG_DST || next->type == NODE_REG_SRC) && next->getSrc()->updateTree) next->getSrc()->updateTree->replace(member, enode);
      }
      member->status = DEAD_NODE;
      aliasMap[member] = enode;
    }
  }
  for (Node* reg : regsrc) {
    if (reg->updateTree) {
      reg->updateTree->replaceUpdateTree(aliasMap);
    }
  }

  updateSuper();

  printf("remove %ld alias (%ld -> %ld)\n", aliasNum, totalNodes, totalNodes - aliasNum);
  printf("remove %ld superNodes (%ld -> %ld)\n", totalSuper - sortedSuper.size(), totalSuper, sortedSuper.size());

}