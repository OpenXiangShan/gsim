#include "common.h"
#include <stack>
#include <set>
#include <map>

static std::set<Node*> fullyVisited;
static std::set<Node*> partialVisited;

void partialDfs(std::set<Node*>&loop) {
  enum nodeState{NOT_VISIT = 0, EXPANDED, VISITED};
  std::map<Node*, nodeState> state;
  for (Node* node : partialVisited) {
    state[node] = NOT_VISIT;
  }
  std::stack<Node*>s;
  for (Node* node : partialVisited) {
    if (state[node] == VISITED) continue;
    s.push(node);
    while (!s.empty()) {
      Node* top = s.top();
      if (state[top] == NOT_VISIT) {
        state[top] = EXPANDED;
        for (Node* next : top->next) {
          if (partialVisited.find(next) == partialVisited.end()) continue;
          if (next == top) Panic();
          if (state[next] == NOT_VISIT) s.push(next);
          else if (state[next] == EXPANDED) {
            while(!s.empty()) {
              Node* nodeInLoop = s.top();
              s.pop();
              if (state[nodeInLoop] == EXPANDED) loop.insert(nodeInLoop);
              if (nodeInLoop == next) break;
            }
            return;
          }
        }
      } else if (state[top] == EXPANDED) {
        state[top] = VISITED;
        s.pop();
      }

    }
  }
}

ExpTree* dupTreeWithIdx(ExpTree* tree, std::vector<int>& index) {
  ENode* lvalue = tree->getlval()->dup();
  for (int idx : index) {
    ENode* idxNode = new ENode(OP_INDEX_INT);
    idxNode->addVal(idx);
    lvalue->addChild(idxNode);
  }
  ENode* rvalue = tree->getRoot()->dup();
  std::stack<ENode*> s;
  s.push(rvalue);
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    for (int i = 0; i < top->getChildNum(); i ++) {
      if (!top->getChild(i)) continue;
    }
    if (top->getNode()) {
      Assert(top->getNode()->isArray(), "%s is not array", top->getNode()->name.c_str());
      for (int idx : index) {
        ENode* idxNode = new ENode(OP_INDEX_INT);
        idxNode->addVal(idx);
        top->addChild(idxNode);
      }
    }
    if (top->opType == OP_WHEN || top->opType == OP_MUX) {
      if (top->getChild(1)) s.push(top->getChild(1));
      if (top->getChild(2)) s.push(top->getChild(2));
    } else if (top->opType == OP_RESET) {
      if (top->getChild(1)) s.push(top->getChild(1));
    }
  }
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  return ret;
}

bool point2self(Node* node) {
  std::set<Node*> nextNodes;
  std::stack<Node*> s;
  std::set<Node*> visited;
  for (Node* next : node->next) {
    s.push(next);
    nextNodes.insert(next);
    if (next == node) return true;
  }
  while(!s.empty()) {
    Node* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    for (Node* next : top->next) {
      if (next == node) return true;
      if (nextNodes.find(next) != nextNodes.end()) continue;
      s.push(next);
    }
  }
  return false;
}

Node* getSplitArray(graph* g) {
  /* array points to itself */
  for (Node* node : partialVisited) {
    if (node->isArray() && node->next.find(node) != node->next.end()) return node;
  }

  for (Node* node : partialVisited) {
    if (node->isArray() && point2self(node)) return node;
  }

  for (Node* node : g->halfConstantArray) {
    if (fullyVisited.find(node) != fullyVisited.end() || node->arraySplitted()) continue;
    if (point2self(node)) return node;
  }
  for (Node* node : partialVisited) {
    printf("current %s\n", node->name.c_str());
  }
  Panic();

}

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode) {
  std::stack<ENode*> s;
  s.push(newTree->getRoot());
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType == OP_STMT) { /* push the first child */
      for (ENode* childENode : top->child) {
        if (childENode) {
          s.push(childENode);
          break;
        }
      }
    } else {
      for (ENode* childENode : top->child) {
        if (childENode) s.push(childENode);
      }
    }
    if (top->opType == OP_WHEN) {
      if (!top->getChild(1)) top->setChild(1, oldNode);
      if (!top->getChild(2)) top->setChild(2, oldNode);
    }
  }
}

void distributeTree(Node* node, ExpTree* tree) {
  ArrayMemberList* list = tree->getlval()->getArrayMember(node);
  if (list->member.size() == 1) {
    int idx = list->idx[0];
    if (node->arrayMember[idx]->assignTree.size() != 0 && node->arrayMember[idx]->assignTree.back()->getRoot()->opType != OP_WHEN) {
      ExpTree* oldTree = node->arrayMember[idx]->assignTree.back();
      fillEmptyWhen(tree, oldTree->getRoot());
      node->arrayMember[idx]->assignTree.back() = tree;
    } else {
      node->arrayMember[idx]->assignTree.push_back(tree);
    }
  } else {
    for (size_t i = 0; i < list->member.size(); i ++) {
      int idx = list->idx[i];
      /* compute index for all array operands in tree */
      std::vector<int> subIdx(node->dimension.size() - tree->getlval()->getChildNum());
      int tmp = idx;
      for (int i = node->dimension.size() - 1; i >= tree->getlval()->getChildNum(); i --) {
        subIdx[i - tree->getlval()->getChildNum()] = tmp % node->dimension[i];
        tmp /= node->dimension[i];
      }
      ExpTree* newTree = dupTreeWithIdx(tree, subIdx);
      /* duplicate tree with idx */
      if (node->arrayMember[idx]->assignTree.size() != 0 && node->arrayMember[idx]->assignTree.back()->getRoot()->opType != OP_WHEN) {
        ExpTree* oldTree = node->arrayMember[idx]->assignTree.back();
        fillEmptyWhen(newTree, oldTree->getRoot());
        node->arrayMember[idx]->assignTree.back() = newTree;
      } else {
        node->arrayMember[idx]->assignTree.push_back(newTree);
      }
    }
  }
}

void graph::splitArrayNode(Node* node) {
  splittedArray.insert(node);
  if (supersrc.find(node->super) != supersrc.end()) supersrc.erase(node->super);
  /* remove prev connection */
  for (Node* prev : node->prev) prev->next.erase(node);
  for (SuperNode* super : node->super->prev) super->next.erase(node->super);
  for (Node* next : node->next) next->prev.erase(node);
  for (SuperNode* super : node->super->next) super->prev.erase(node->super);
  node->super->prev.clear();
  node->super->next.clear();

  for (Node* memberInSuper : node->super->member) {
    if (memberInSuper != node)
      memberInSuper->constructSuperConnect();
  }
  /* create new node */
  int entryNum = node->arrayEntryNum();

  for (int i = 0; i < entryNum; i ++) {
    node->arrayMemberNode(i);
  }
  /* distribute arrayVal */
  for (ExpTree* tree : node->assignTree) distributeTree(node, tree);
  for (ExpTree* tree : node->arrayVal) distributeTree(node, tree);
  if (node->updateTree) {
    for (int idx = 0; idx < node->arrayEntryNum(); idx ++) {
      /* compute index for all array operands in tree */
      std::vector<int> subIdx(node->dimension.size());
      int tmp = idx;
      for (int i = node->dimension.size() - 1; i >= 0; i --) {
        subIdx[i] = tmp % node->dimension[i];
        tmp /= node->dimension[i];
      }
      ExpTree* newTree = dupTreeWithIdx(node->updateTree, subIdx);
      node->arrayMember[idx]->updateTree = newTree;
    }
  }
  /* construct connections */
  for (Node* member : node->arrayMember) {
    member->updateConnect();
  }
  for (Node* next : node->next) {
    if (next != node) {
      next->prev.clear();
      next->updateConnect();
    }
  }

  /* clear node connection */
  node->prev.clear();
  node->next.clear();

  for (Node* member : node->arrayMember) member->constructSuperConnect();
  for (Node* member : node->arrayMember) {
    if (member->super->prev.size() == 0) supersrc.insert(member->super);
  }
}

void graph::splitArray() {
  std::map<Node*, int> times;
  std::stack<Node*> s;
  int num = 0;

  for (SuperNode* super : supersrc) {
    for (Node* node : super->member) {
      if (node->prev.size() == 0) {
        s.push(node);
        fullyVisited.insert(node);
      }
    }
  }

  while(!s.empty()) {
    Node* top = s.top();
    s.pop();

    for (Node* next : top->next) {
      if (times.find(next) == times.end()) {
        times[next] = 0;
        partialVisited.insert(next);
      }
      times[next] ++;

      if (times[next] == (int)next->prev.size()) {
        s.push(next);
        Assert(partialVisited.find(next) != partialVisited.end(), "%s not found in partialVisited", next->name.c_str());
        partialVisited.erase(next);
        Assert(fullyVisited.find(next) == fullyVisited.end(), "%s is already in fullyVisited", next->name.c_str());
        fullyVisited.insert(next);
      }
    }

    while (s.size() == 0 && partialVisited.size() != 0) {
      /* split arrays in partialVisited until s.size() != 0 */
      Node* node = getSplitArray(this);
      Assert(!node->arraySplitted(), "%s is already splitted", node->name.c_str());
      // printf("split array %s\n", node->name.c_str());
      num ++;
      splitArrayNode(node);
      /* add into s and visitedSet */
      for (Node* member : node->arrayMember) {
        // if (!member->valTree) continue;
        times[member] = 0;
        for (Node* prev : member->prev) {
          if (fullyVisited.find(prev) != fullyVisited.end()) {
            times[member] ++;
          }
        }
        if (times[member] == (int)member->prev.size()) {
          s.push(member);
          fullyVisited.insert(member);
        } else {
          partialVisited.insert(member);
        }
      }
      /* erase array */
      partialVisited.erase(node);
    }
  }
  Assert(partialVisited.size() == 0, "partial is not empty!");
  printf("[splitArray] split %d arrays\n", num);
  splitOptionalArray();
}

Node* Node::arrayMemberNode(int idx) {
  Assert(isArray(), "%s is not array", name.c_str());
  std::vector<int>index(dimension);
  int dividend = dimension.back();
  int divisor = idx;

  for (int i = (int)dimension.size() - 1; i >= 0; i --) {
    dividend = dimension[i];
    index[i] = divisor % dividend;
    divisor = divisor / dividend;
  }
  std::string memberName = name;
  size_t i;
  for (i = 0; i < index.size(); i ++) {
    memberName += "__" + std::to_string(index[i]);
  }

  Node* member = new Node(type);
  member->isArrayMember = true;
  member->name = memberName;
  member->arrayIdx = idx;
  arrayMember.push_back(member);
  member->arrayParent = this;
  member->clock = clock;
  member->reset = reset;
  member->setType(width, sign);
  member->constructSuperNode();

  for ( ; i < dimension.size(); i ++) member->dimension.push_back(dimension[i]);

  return member;
}

bool nextVarConnect(Node* node) {
  for (Node* next : node->next) {
    std::stack<std::pair<ENode*, bool>> s;
    for (ExpTree* tree : next->assignTree) s.push(std::make_pair(tree->getRoot(), false));
    for (ExpTree* tree : next->arrayVal) {
      s.push(std::make_pair(tree->getRoot(), false));
      s.push(std::make_pair(tree->getlval(), false));
    }
    while (!s.empty()) {
      ENode* top;
      bool anyMux;
      std::tie(top, anyMux) = s.top();
      s.pop();
      if (top->getNode() == node) {
        int beg, end;
        std::tie(beg, end) = top->getIdx(node);
        if (beg < 0) return true;
      }
      for (int i = 0; i < top->getChildNum(); i ++) {
        if (top->getChild(i)) s.push(std::make_pair(top->getChild(i), anyMux || (top->opType == OP_MUX)));
      }
    }
  }
  return false;
}
static std::map<Node*, bool> arraySplitMap;

void graph::checkNodeSplit(Node* node) {
  if (arraySplitMap.find(node) != arraySplitMap.end()) return;
  if (!node->isArray()) return;
  if ((node->type != NODE_OTHERS && node->type != NODE_REG_SRC) || node->arrayEntryNum() <= 1) {
    arraySplitMap[node] = false;
    return;
  }
  for (Node* prev : node->prev) checkNodeSplit(prev);
  bool prevSplitted = true;
  for (Node* prev : node->prev) {
    if (prev->isArray() && !arraySplitMap[prev]) prevSplitted = false;
  }
  Node* treeNode = node->type == NODE_REG_SRC ? node->getDst() : node;
  bool anyVarIdx = false;
  int beg, end;
  for (ExpTree* tree : treeNode->assignTree) {
    std::tie(beg, end) = tree->getlval()->getIdx(treeNode);
    if (beg < 0 || (beg != end && !prevSplitted)) anyVarIdx = true;
  }
  for (ExpTree* tree : treeNode->arrayVal) {
    std::tie(beg, end) = tree->getlval()->getIdx(treeNode);
    if (beg < 0 || (beg != end&& !prevSplitted)) anyVarIdx = true;
  }
  if (!anyVarIdx && !nextVarConnect(node)) {
    node->status = DEAD_NODE;
    if (node->type == NODE_REG_SRC) node->getDst()->status = DEAD_NODE;
    arraySplitMap[node] = true;
  } else {
    arraySplitMap[node] = false;
  }
}

/* splitted separately assigned, no variable index acceesing arrays */
void graph::splitOptionalArray() {
  int num = 0;
  for (Node* node : fullyVisited) {
    checkNodeSplit(node);
  }
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );
  for (auto iter : arraySplitMap) {
    if (iter.second) {
      splitArrayNode(iter.first);
      if (iter.first->type == NODE_REG_SRC) {
        splitArrayNode(iter.first->getDst());
        Assert(iter.first->arrayMember.size() == iter.first->getDst()->arrayMember.size(), "%p %s member not match", iter.first, iter.first->name.c_str());
        for (size_t i = 0; i < iter.first->arrayMember.size(); i ++) {
          regsrc.push_back(iter.first->arrayMember[i]);
          iter.first->arrayMember[i]->bindReg(iter.first->getDst()->arrayMember[i]);
        }
      }
      num ++;
    }
  }
  printf("[splitOptionalArray] split %d arrays\n", num);
}