#include "common.h"
#include <stack>

static std::map<uint64_t, std::vector<Node*>> exprId;
static std::map<Node*, Node*> aliasMap;

uint64_t ENode::keyHash() {
  if (nodePtr) return nodePtr->id;
  else return opType * width;
}

uint64_t ExpTree::keyHash() {
  std::stack<ENode*> s;
  s.push(getRoot());
  uint64_t ret = 0;
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    ret += top->keyHash();
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  return ret;
}

uint64_t Node::keyHash() {
  uint64_t ret = 0;
  for (ExpTree* tree : assignTree) ret += tree->keyHash();
  for (ExpTree* tree : arrayVal) ret += tree->keyHash();
  return ret;
}

static bool checkENodeEq(ENode* enode1, ENode* enode2) {
  if (!enode1 && !enode2) return true;
  if (!enode1 || !enode2) return false;
  if (enode1->getNode() != enode2->getNode()) return false;
  if (enode1->opType != enode2->opType) return false;
  if (enode1->width != enode2->width || enode1->sign != enode2->sign) return false;
  if (enode1->child.size() != enode2->child.size()) return false;
  if (enode1->opType == OP_INT && enode1->strVal != enode2->strVal) return false;
  if (enode1->values.size() != enode2->values.size()) return false;
  for (size_t i = 0; i < enode1->values.size(); i ++) {
    if (enode1->values[i] != enode2->values[i]) return false;
  }
  // if (enode1->opType == OP_INDEX_INT && enode1->values[0] != enode2->values[0]) return false;
  return true;
}

static bool checkTreeEq(ExpTree* tree1, ExpTree* tree2) {
  std::stack<std::pair<ENode*, ENode*>> s;
  s.push(std::make_pair(tree1->getRoot(), tree2->getRoot()));
  while (!s.empty()) {
    ENode *top1, *top2;
    std::tie(top1, top2) = s.top();
    s.pop();
    bool enodeEq = checkENodeEq(top1, top2);
    if (!enodeEq) return false;
    if (!top1) continue;
    for (size_t i = 0; i < top1->child.size(); i ++) {
      s.push(std::make_pair(top1->child[i], top2->child[i]));
    }
  }
  return true;
}

static bool checkNodeEq (Node* node1, Node* node2) {
  if (node1->assignTree.size() != node2->assignTree.size()) return false;
  if (node1->arrayVal.size() != node2->arrayVal.size()) return false;
  for (size_t i = 0; i < node1->assignTree.size(); i ++) {
    if (!checkTreeEq(node1->assignTree[i], node2->assignTree[i])) return false;
  }
  for (size_t i = 0; i < node1->arrayVal.size(); i ++) {
    if (!checkTreeEq(node1->arrayVal[i], node2->arrayVal[i])) return false;
  }
  return true;
}

void ExpTree::replace(std::map<Node*, Node*>& aliasMap) {
  std::stack<ENode*> s;
  s.push(getRoot());
  if (getlval()) s.push(getlval());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->getNode() && aliasMap.find(top->getNode()) != aliasMap.end()) {
      top->nodePtr = aliasMap[top->getNode()];
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
}

/* TODO: check common regs */
void graph::commonExpr() {
  for (SuperNode* super : sortedSuper) {
    if (super->superType != SUPER_VALID) continue;
    for (Node* node : super->member) {
      if (node->type != NODE_OTHERS || node->isArray() || node->isArrayMember) continue;
      if (node->prev.size() == 0) continue;
      // if (node->next.size() == 1) continue;
      uint64_t key = node->keyHash();
      if (exprId.find(key) == exprId.end()) {
        exprId[key] = std::vector<Node*>();
      }
      exprId[key].push_back(node);
    }
  }
  
  for (auto iter : exprId) {
    if (iter.second.size() <= 1) continue;
    std::vector<Node*> uniqueNodes;
    /* pair-wise checking */
    for (Node* node : iter.second) {
      if (uniqueNodes.size() == 0) {
        uniqueNodes.push_back(node);
        continue;
      }
      bool isUnique = true;
      for (Node* prevNode : uniqueNodes) {
        if (checkNodeEq(node, prevNode)) {
          aliasMap[node] = prevNode;
          node->status = DEAD_NODE;
          // printf("common %s to %s\n", node->name.c_str(), prevNode->name.c_str());
          isUnique = false;
          break;
        }
      }
      if (isUnique) {
        uniqueNodes.push_back(node);
      }
    }
    if (uniqueNodes.size() > 1) {
      printf("different trees with same hash(size=%ld):\n", uniqueNodes.size());
      for (Node* node : uniqueNodes) {
        node->display();
      }
    }
  }
/* update assignTrees */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == DEAD_NODE) continue;
      for (ExpTree* tree : member->arrayVal) {
        if (tree) tree->replace(aliasMap);
      }
      for (ExpTree* tree : member->assignTree) tree->replace(aliasMap);
      if (member->updateTree) member->updateTree->replace(aliasMap);
    }
  }
/* apdate arrayMember */
  for (Node* array: splittedArray) {
    printf("checking %s\n", array->name.c_str());
    for (size_t i = 0; i < array->arrayMember.size(); i ++) {
      if (array->arrayMember[i] && aliasMap.find(array->arrayMember[i]) != aliasMap.end()) {
        array->arrayMember[i] = aliasMap[array->arrayMember[i]];
      }
    }
  }

/* update connection */
  removeNodesNoConnect(DEAD_NODE);
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
  removeNodes(DEAD_NODE);

  printf("[commonExpr] remove %ld nodes (-> %ld)\n", aliasMap.size(), countNodes());

}

