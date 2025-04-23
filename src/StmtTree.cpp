#include "common.h"
#include <stack>

bool checkENodeEq(ENode* enode1, ENode* enode2);
void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

bool checkCondENodeSame(ENode* enode1, ENode* enode2) {
  if (!enode1 && !enode2) return true;
  if (!enode1 || !enode2) return false;
  if (enode1->opType != enode2->opType) return false;
  if (enode1->width != enode2->width || enode1->sign != enode2->sign) return false;
  if (enode1->child.size() != enode2->child.size()) return false;
  if (enode1->opType == OP_INT && enode1->strVal != enode2->strVal) return false;
  if (enode1->values.size() != enode2->values.size()) return false;
  if ((!enode1->getNode() && enode2->getNode()) || (enode1->getNode() && !enode2->getNode())) return false;
  if (enode1->getNode() && enode2->getNode() && enode1->getNode() != enode2->getNode()) return false;
  for (size_t i = 0; i < enode1->values.size(); i ++) {
    if (enode1->values[i] != enode2->values[i]) return false;
  }
  for (size_t i = 0; i < enode1->child.size(); i ++) {
    if (!checkCondENodeSame(enode1->child[i], enode2->child[i])) return false;
  }
  return true;
}

bool checkCondEq(StmtNode* stmtNode, ENode* enode) {
  if ((enode->opType == OP_WHEN || enode->opType == OP_RESET) && stmtNode->type == OP_STMT_WHEN) {
    if (stmtNode->child[0]->isENode && checkCondENodeSame(enode->getChild(0), stmtNode->getChild(0)->enode)) {
      return true;
    }
  }
  return false;
}

void addDepthPath(std::vector<int>& path, int depth, int nodeIdx) {
  if (path.size() <= depth) path.push_back(nodeIdx);
  else if(nodeIdx > path[depth]){
    path[depth] = nodeIdx;
    path.resize(depth + 1);
  }
}

void maxPath(std::vector<int>& dst, std::vector<int>& path1, std::vector<int>& path2) {
  int isFirst = false;
  for (size_t i = 0; i < path1.size(); i ++) {
    if (i >= path2.size() || path1[i] > path2[i]) {
      isFirst = true;
      break;
    } else if (path1[i] < path2[i]) {
      isFirst = false;
      break;
    }
  }
  if (isFirst) dst = path1;
  else dst = path2;
}

/* generate whens according to enode, and add stmtNode into child */
void StmtNode::generateWhenTree(ENode* lvalue, ENode* root, std::vector<int>& subPath, Node* belong) {
  Assert(root->opType == OP_WHEN || root->opType == OP_RESET, "require when");
  StmtNode* stmt = nullptr;
  std::stack<std::tuple<StmtNode*, ENode*, int>> s;
  s.push(std::make_tuple(stmt, root, 1));
  addDepthPath(subPath, 0, getChildNum());
  while (!s.empty()) {
    StmtNode* parent;
    ENode* enode;
    int depth;
    std::tie(parent, enode, depth) = s.top();
    s.pop();
    if (enode->opType == OP_WHEN || enode->opType == OP_RESET) {
      StmtNode* stmtNode = new StmtNode(OP_STMT_WHEN);
      stmtNode->addChild(new StmtNode(enode->getChild(0)->dup(), belong));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      stmtNode->addChild(new StmtNode(OP_STMT_SEQ));
      if (enode->getChild(1)) {
        s.push(std::make_tuple(stmtNode->getChild(1), enode->getChild(1), depth + 1));
        addDepthPath(subPath, depth, stmtNode->getChild(1)->getChildNum());
      }
      if (enode->getChildNum() >= 3 && enode->getChild(2)) {
        s.push(std::make_tuple(stmtNode->getChild(2), enode->getChild(2), depth + 1));
        addDepthPath(subPath, depth, stmtNode->getChild(2)->getChildNum());
      }
      if (parent) parent->addChild(stmtNode);
      else stmt = stmtNode;
    } else {
      parent->addChild(new StmtNode(new ExpTree(enode->dup(), lvalue->dup()), belong));
      addDepthPath(subPath, depth, parent->child.size() - 1);
    }
  }
  addChild(stmt);
}

void StmtTree::mergeExpTree(ExpTree* tree, std::vector<int>& prevPath, std::vector<int>& nodePath, Node* belong) {
  std::vector<int> path;
  maxPath(path, prevPath, nodePath);
  std::stack<std::tuple<StmtNode*, ENode*, int, bool>> s;
  s.push(std::make_tuple(root, tree->getRoot(), 1, true));
  addDepthPath(nodePath, 0, 0);
  while (!s.empty()) {
    StmtNode* stmtNode;
    ENode* enode;
    int depth;
    bool anyLimit;
    std::tie(stmtNode, enode, depth, anyLimit) = s.top();
    s.pop();

    if (anyLimit && depth >= path.size()) anyLimit = false;
    if (checkCondEq(stmtNode, enode)) { // directly merge when
      if (enode->getChild(1)) {
        s.push(std::make_tuple(stmtNode->getChild(1), enode->getChild(1), depth + 1, anyLimit));
        addDepthPath(nodePath, depth, stmtNode->getChild(1)->getChildNum());
      }
      if (enode->getChildNum() >= 3 && enode->getChild(2)) {
        s.push(std::make_tuple(stmtNode->getChild(2), enode->getChild(2), depth + 1, anyLimit));
        addDepthPath(nodePath, depth, stmtNode->getChild(2)->getChildNum());
      }
    } else {
      if (enode->opType == OP_WHEN || enode->opType == OP_RESET) {
        if (stmtNode->type == OP_STMT_SEQ) {
          int match = -1;
          for (size_t i = 0; i < stmtNode->child.size(); i ++) {
            if (stmtNode->getChild(i)->type == OP_STMT_WHEN &&
                checkCondEq(stmtNode->getChild(i), enode) &&
                (!anyLimit || path[depth] <= i)) {
              match = i;
              bool nextLimit = (!anyLimit || path[depth] < i) ? false : anyLimit;
              s.push(std::make_tuple(stmtNode->getChild(i), enode, depth + 1, nextLimit));
              break;
            }
          }
          if (match >= 0) {
            addDepthPath(nodePath, depth, match);
          } else {
            std::vector<int> subPath;
            stmtNode->generateWhenTree(tree->getlval(), enode, subPath, belong);
            for (size_t i = 0; i < subPath.size(); i ++) {
              addDepthPath(nodePath, depth + i, subPath[i]);
            }
          }
        } else {
          TODO();
        }
      } else {
        Node* node = tree->getlval()->getNode();
        Assert(stmtNode->type == OP_STMT_SEQ, "stmtNode %d is not seq", stmtNode->type);
        if (!node->isArray() && enode->opType != OP_INVALID) {
          for (int i = 0; i < stmtNode->getChildNum(); i ++) {
            StmtNode* child = stmtNode->getChild(i);
            if (child->type == OP_STMT_NODE) {
              Assert(!child->isENode, "invalid stmtNode %d", child->type);
              Node* childNode = child->tree->getlval()->getNode();
              Assert(childNode, "invalid node");
              if (childNode == node) {
                stmtNode->eraseChild(i);
              }
            }
          }
        }
        stmtNode->addChild(new StmtNode(new ExpTree(enode->dup(), tree->getlval()->dup()), belong));
        addDepthPath(nodePath, depth, stmtNode->child.size() - 1);
      }
    }
  }
}

void StmtTree::addSeq(ExpTree* tree, Node* belong) {
  Assert(root->type == OP_STMT_SEQ, "root is not seq");
  StmtNode* stmtNode = new StmtNode(tree->dup(), belong);
  root->addChild(stmtNode);
}

void StmtTree::mergeStmtTree(StmtTree* tree) {
  Assert(root->type == OP_STMT_SEQ && tree->root->type == OP_STMT_SEQ, "root is not seq");
  for (StmtNode* stmtNode : tree->root->child) {
    root->addChild(stmtNode);
  }
}

void prevOrderPath(Node* node, std::vector<int>& prevPath, std::map<Node*, std::vector<int>>& allPath) { // the max path of previous nodes that in the same superNode
  if (node->depPrev.size() == 0) return;
  for (Node* prev : node->depPrev) {
    if (prev->super != node->super) continue;
    Assert(allPath.find(prev) != allPath.end(), "path of %s not exits", prev->name.c_str());
    /* merge prevPath */
    for (size_t i = 0; i < allPath[prev].size(); i ++) {
      if (i >= prevPath.size()) prevPath.push_back(allPath[prev][i]);
      else if (prevPath[i] <= allPath[prev][i]) prevPath[i] = allPath[prev][i];
      else break;
    }
  }
}

void getCommonPath(std::vector<int>& path1, ExpTree* referTree, std::vector<int>& path2, ExpTree* newTree) {
  int commonNum = 1;
  ENode* referRoot = referTree->getRoot();
  ENode* newRoot = newTree->getRoot();
  for (; commonNum < MIN(path1.size(), path2.size()) && referRoot->opType == OP_WHEN && newRoot->opType == OP_WHEN; commonNum ++) {
    if (path1[commonNum] != path2[commonNum]) break;
    if (!checkCondENodeSame(referRoot->getChild(0), newRoot->getChild(0))) break;
    referRoot = referRoot->getChild(path1[commonNum]);
    newRoot = newRoot->getChild(path2[commonNum]);
  }
  path1.resize(commonNum);
}

void getRelyPath(std::vector<int>&path, Node* node, ExpTree* tree) { // get the [path] in [tree] that refers [node]
  enum status {NOT_VISITED, EXPANDED, VISITED};
  std::map<ENode*, status> enodeStatus;
  std::set<Node*> lvalueNodes;
  getENodeRelyNodes(tree->getlval(), lvalueNodes);
  bool inLvalue = lvalueNodes.find(node) != lvalueNodes.end(); // the path leads to assignment
  std::stack<std::pair<ENode*, int>> s;
  std::vector<int> currentPath;
  bool isFirst = true;
  s.push(std::make_pair(tree->getRoot(), 0));
  while (!s.empty()) {
    ENode* top;
    int idx;
    std::tie(top, idx) = s.top();
    if (enodeStatus.find(top) == enodeStatus.end()) { // not visited
      currentPath.push_back(idx);
      /* compute common path of all referrence */
      if (top->getNode() == node || (inLvalue && (top->getNode() || top->opType == OP_INT))) {
        if (isFirst) {
          path = currentPath;
          isFirst = false;
        } else {
          getCommonPath(path, tree, currentPath, tree);
        }
        enodeStatus[top] = VISITED;
        s.pop();
        currentPath.pop_back();
        continue;
      }
      /* expand top */
      enodeStatus[top] = EXPANDED;
      for (int i = 0; i < top->getChildNum(); i ++) {
        Assert(enodeStatus.find(top->getChild(i)) == enodeStatus.end(), "already visited %p in %s\n", top->getChild(i), node->name.c_str());
        if (top->getChild(i)) s.push(std::make_pair(top->getChild(i), i));
      }
    } else if (enodeStatus[top] == EXPANDED) {
      enodeStatus[top] = VISITED;
      s.pop();
      currentPath.pop_back();
    } else {
      Panic();
    }
  }
}

bool isLastWhenCond(std::vector<int>&path, ExpTree* referTree) {
  if (path.back() != 0) return false;
  bool isLastWhenCond = false;
  ENode* checkENode = referTree->getRoot();
  for (size_t i = 1; i + 1 < path.size(); i ++) {
    checkENode = checkENode->getChild(path[i]);
  }
  if (checkENode->opType == OP_WHEN) isLastWhenCond = true;
  return isLastWhenCond;
}

void growTreeFromPath(ExpTree* oldTree, std::vector<int>&path, ExpTree* referTree) {
  ENode* referRoot = referTree->getRoot();
  if (referRoot->opType != OP_WHEN) return ;

  int endIdx = isLastWhenCond(path, referTree) ? (int)path.size() - 1 : path.size(); // the node may be when cond
  Assert(endIdx >= 0, "invalid path");
  if (endIdx <= 1) return;
  ENode* newRoot = nullptr;
  ENode* newParent = nullptr;
  for (int idx = 0; idx < endIdx; idx ++) {
    if (referRoot->opType != OP_WHEN || (idx + 1 == endIdx)) {
      newParent->setChild(path[idx], oldTree->getRoot());
      break;
    } else {
      ENode* when = new ENode(OP_WHEN);
      when->width = oldTree->getRoot()->width;
      when->sign = oldTree->getRoot()->sign;
      when->addChild(referRoot->getChild(0)->dup());
      when->addChild(nullptr);
      when->addChild(nullptr);
      if (newParent) newParent->setChild(path[idx], when);
      else newRoot = when;
      newParent = when;
    }
    if (idx + 1 < endIdx) referRoot = referRoot->getChild(path[idx + 1]);
  }

  oldTree->setRoot(newRoot);
}

void growWhenPathFromNext(Node* node) {
  if (node->type != NODE_OTHERS) return;
  std::set<ExpTree*> usedTrees;
  std::vector<int> path;
  bool isFirst = true;
  ExpTree* referTree = nullptr;
  for (Node* next : node->next) {
    for (ExpTree* tree : next->assignTree) {
      std::vector<int> currentPath;
      getRelyPath(currentPath, node, tree);
      if (currentPath.size() != 0) {
        usedTrees.insert(tree);
        if (isFirst) {
          path = currentPath;
          isFirst = false;
          referTree = tree;
        } else getCommonPath(path, referTree, currentPath, tree);
      }
    }
  }
  if (path.size() <= 1) return;
  Assert(path[0] == 0, "the first must be root");

  for (ExpTree* tree : node->assignTree) {
    growTreeFromPath(tree, path, *usedTrees.begin());
  }
}

void SuperNode::reorderMember() {
  std::vector<Node*> newMember;
  std::map<Node*, int> nodePrev;
  for (Node* node : member) {
    nodePrev[node] = 0;
    for (Node* prev : node->depPrev) {
      if (prev->super == this) nodePrev[node] ++;
    }
  }
  std::stack<Node*> s;
  for (auto iter : nodePrev) {
    if (iter.second == 0) {
      newMember.push_back(iter.first);
      s.push(iter.first);
    }
  }
  while (!s.empty()) {
    Node* node = s.top();
    s.pop();
    for (Node* next : node->depNext) {
      if (next->super == this) {
        nodePrev[next] --;
        if (nodePrev[next] == 0) {
          newMember.push_back(next);
          s.push(next);
        }
      }
    }
  }
  Assert(newMember.size() == member.size(), "invalid size %ld %ld\n", newMember.size(), member.size());
  member = newMember;
}

void graph::generateStmtTree() {
  orderAllNodes();
  std::map<Node*, std::vector<int>> allPath; // order in seq
  /* add when path for nodes with next = 1 */
  for (int superIdx = sortedSuper.size() - 1; superIdx >= 0; superIdx --) {
    SuperNode* super = sortedSuper[superIdx];
    for (int nodeIdx = super->member.size() - 1; nodeIdx >= 0; nodeIdx --) {
      Node* member = super->member[nodeIdx];
      int depNextInSuper = 0;
      for (Node* next : member->depNext) {
        if (next->super == super) depNextInSuper ++;
      }
      if (depNextInSuper != 1 || member->next.size() != 1) continue; // TODO: change to "no loop in SuperNode"
      bool anyExtNext = false;
      for (Node* next : member->next) {
        if (next->super != super) {
          anyExtNext = true;
          break;
        }
      }
      if (anyExtNext) continue;
      growWhenPathFromNext(member);
      /* update connection */
      member->updateConnect();
    }
  }
  connectDep();
  /* reorder nodes in superNode */
  for (SuperNode* super : sortedSuper) {
    super->reorderMember();
  }

  for (SuperNode* super : sortedSuper) {
    super->stmtTree = new StmtTree();
    super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
    for (Node* node : super->member) {
      std::vector<int> prevPath;
      std::vector<int> nodePath;
      prevOrderPath(node, prevPath, allPath);
      for (ExpTree* tree : node->assignTree) {
        super->stmtTree->mergeExpTree(tree, prevPath, nodePath, node);
        /* update reg_dst when asyreset arrives */
        if (node->type == NODE_REG_SRC && node->reset == ASYRESET && node->regSplit && node->getDst()->status == VALID_NODE) {
          ENode* lvalue = tree->getlval()->dup();
          lvalue->nodePtr = node->getDst();
          ExpTree* dstTree = new ExpTree(tree->getRoot(), lvalue);
          super->stmtTree->mergeExpTree(dstTree, prevPath, nodePath, nullptr);
        }
      }
      allPath[node] = nodePath;
    }
  }
  for (SuperNode* super : uintReset) {
    super->stmtTree = new StmtTree();
    super->stmtTree->root = new StmtNode(OP_STMT_SEQ);
    for (Node* node : super->member) {
      for (ExpTree* tree : node->assignTree) {
        std::vector<int> emptyPath;
        super->stmtTree->mergeExpTree(tree, emptyPath, emptyPath, node);
      }
    }
  }
}