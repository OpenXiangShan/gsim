/*
  merge regsrc into regdst if possible
*/

#include "common.h"
#include <stack>
#include <tuple>
Node* getLeafNode(bool isArray, ENode* enode);

void ExpTree::replace(Node* oldNode, ENode* newENode) {
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, -1));
  if (getlval()) {
    for (size_t i = 0; i < getlval()->getChildNum(); i ++) s.push(std::make_tuple(getlval()->getChild(i), getlval(), i));
  }

  while(!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (top->getNode() && getLeafNode(true, top) == oldNode) {
      if (parent) parent->setChild(idx, newENode);
      else setRoot(newENode);
    }

    for (size_t i = 0; i < top->getChildNum(); i ++) {
      ENode* enode = top->getChild(i);
      if (!enode) continue;
      s.push(std::make_tuple(enode, top, i));
    }
  }
}

void ExpTree::removeSelfAssignMent(Node* node) {
  std::stack<ENode*> s;
  s.push(getRoot());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType == OP_WHEN) {
      for (size_t i = 1; i < top->getChildNum(); i ++) {
        if (top->child[i] && node == getLeafNode(true, top->child[i])) top->child[i] = nullptr;
      }
    }
    for (ENode* child : top->child) {
      if (child) s.push(child);
    }
  }
}

bool isNext(Node* node, Node* checkNode) {
  bool outSuperNext = checkNode->super->order > node->super->order;
  bool inSuperNext = (checkNode->super == node->super) && (checkNode->orderInSuper > node->orderInSuper);
  return outSuperNext || inSuperNext;
}

Node* laterNode(Node* node1, Node* node2) {
  if (!node1) return node2;
  if (!node2) return node1;
  if (isNext(node1, node2)) return node2;
  return node1;
}

void graph::orderAllNodes() {
  int order = 1;
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    sortedSuper[i]->order = i;
    for (size_t j = 0; j < sortedSuper[i]->member.size(); j ++) {
      sortedSuper[i]->member[j]->orderInSuper = j;
      sortedSuper[i]->member[j]->order = order ++;
    }
  }
}

void graph::mergeRegister() {
  orderAllNodes();
  int num = 0;
  int totalNum = 0;
  std::map<Node*, Node*> maxNode;
  std::map<Node*, bool> anyNextNodes;

  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    for (int j = (int)sortedSuper[i]->member.size() - 1; j >= 0; j --) {
      Node* node = sortedSuper[i]->member[j];
      Node* latestNode = nullptr;
      for (Node* next : node->next) {
        latestNode = laterNode(latestNode, next);
      }
      maxNode[node] = latestNode;
    }
  }
  for (Node* reg : regsrc) {
    if (reg->status != VALID_NODE) continue;
    totalNum ++;
    if (reg->isArray() || reg->getDst()->assignTree.size() != 1) continue;
    bool split = false;
    if (maxNode[reg] && isNext(reg->getDst(), maxNode[reg])) split = true;
    /* checking updateTree */
    std::stack<ENode*> s;
    s.push(reg->updateTree->getRoot());
    while (!s.empty() && !split) {
      ENode* top = s.top();
      s.pop();
      if (top->getNode()) {
        Node* topNode = top->getNode();
        if (topNode->isArray() && topNode->arraySplitted()) {
          ArrayMemberList* list = top->getArrayMember(topNode);
          for (Node* member : list->member) {
            if (isNext(reg->getDst(), member)) {
              split = true;
              break;
            }
          }
        } else {
          if (isNext(reg->getDst(), topNode)) split = true;
        }
        for (ENode* child : top->child) {
          if (child) s.push(child);
        }
      }
    }
    if (!split) { // treat dst as NODE_OTHER
      num ++;
      reg->regSplit = reg->getDst()->regSplit = false;
      reg->getDst()->name = reg->name;
    }
  }

  printf("[mergeRegister] merge %d registers (total %d)\n", num, totalNum);
}

void graph::constructRegs() {
  std::map<SuperNode*, SuperNode*> dstSuper2updateSuper;
  for (Node* node : regsrc) {
    if (node->status != VALID_NODE) continue;
    if (node->regSplit) {
      Node* nodeUpdate = node->dup(NODE_REG_UPDATE);
      node->regUpdate = nodeUpdate;
      nodeUpdate->regNext = node;
      nodeUpdate->assignTree.push_back(node->updateTree);
      SuperNode* dstSuper = node->getDst()->super;
      if (dstSuper2updateSuper.find(dstSuper) != dstSuper2updateSuper.end()) { // TODO: why use this
        nodeUpdate->super = dstSuper2updateSuper[dstSuper];
        dstSuper2updateSuper[dstSuper]->member.push_back(nodeUpdate);
      } else {
        nodeUpdate->super = new SuperNode(nodeUpdate);
        nodeUpdate->super->superType = SUPER_UPDATE_REG;
        nodeUpdate->super->threadId = node->super->threadId;
        sortedSuper.push_back(nodeUpdate->super);
        dstSuper2updateSuper[dstSuper] = nodeUpdate->super;
      }
      nodeUpdate->next.insert(node->next.begin(), node->next.end());
      nodeUpdate->isArrayMember = node->isArrayMember;
      nodeUpdate->updateConnect();
    } else {
      Assert(node->getDst()->assignTree.size() == 1, "invalid assignTree for node %s", node->name.c_str());
      node->updateTree->replace(node->getDst(), node->getDst()->assignTree.back()->getRoot());
      /* remove self-assignments */
      if (getLeafNode(true, node->updateTree->getRoot()) == node->getSrc()) node->getDst()->assignTree.clear();
      else {
        node->getDst()->assignTree[0] = node->updateTree;
        node->getDst()->assignTree[0]->removeSelfAssignMent(node);
      }
      node->next.erase(node->getDst());
      node->getDst()->updateConnect();
      node->getDst()->status = VALID_NODE;
      node->getDst()->computeInfo = nullptr;
    }
  }
}
