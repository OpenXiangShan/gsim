#include <gmp.h>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <queue>
#include <set>
#include <stack>
#include <tuple>
#include <utility>
#include <vector>
#include "common.h"
#include "splitNode.h"

#define NODE_REF first
#define NODE_UPDATE second
/* refer & update segment */
static std::map<Node*, std::pair<Segments*, Segments*>> nodeSegments;

std::map <Node*, NodeComponent*> componentMap;
std::map <ENode*, NodeComponent*> componentEMap;
static std::vector<Node*> checkNodes;
static std::map<Node*, Node*> aliasMap;
static std::map<Node*, std::vector<std::pair<Node*, int>>> splittedNodesSeg;
static std::map<Node*, std::vector<Node*>> splittedNodesSet;
/* all nodes after splitting */
static std::set<Node*> allSplittedNodes;

static std::priority_queue<Node*, std::vector<Node*>, ordercmp> reInferQueue;
static std::set<Node*> uniqueReinfer;

ExpTree* dupSplittedTree(ExpTree* tree, Node* regold, Node* regnew);
ExpTree* dupTreeWithBits(ExpTree* tree, int hi, int lo);
void addReInfer(Node* node);
Node* getLeafNode(bool isArray, ENode* enode);
void getENodeRelyNodes(ENode* enode, std::set<Node*>& allNodes);

NodeComponent* spaceComp(int width) {
  NodeComponent* comp = new NodeComponent();
  comp->addElementAll(new NodeElement(ELE_SPACE, nullptr, width - 1, 0));
  return comp;
}
/* split node according to seg */
void createSplittedNode(Node* node, std::set<int>& cuts) {
  splittedNodesSeg[node] = std::vector<std::pair<Node*, int>>();
  splittedNodesSeg[node].resize(node->width);
  if (node->type == NODE_REG_SRC) {
    splittedNodesSeg[node->getDst()] = std::vector<std::pair<Node*, int>>();
    splittedNodesSeg[node->getDst()].resize(node->width);
  }
  int lo = 0;
  for (int hi : cuts) {
    Node* splittedNode = nullptr;
    /* allocate component & update cut */
    if (node->type == NODE_REG_SRC) {
      Node* newSrcNode = node->dup(node->type, node->name + format("$%d_%d", hi, lo));
      newSrcNode->width = hi - lo + 1;
      newSrcNode->super = new SuperNode(newSrcNode);
      Node* newDstNode = node->dup(node->getDst()->type, node->getDst()->name + format("$%d_%d", hi, lo));
      newDstNode->width = hi - lo + 1;
      newDstNode->super = new SuperNode(newDstNode);
      newSrcNode->bindReg(newDstNode);
      componentMap[newDstNode] = new NodeComponent();
      componentMap[newDstNode]->addElementAll(new NodeElement(ELE_SPACE));
      nodeSegments[newDstNode] = std::make_pair(new Segments(newDstNode->width), new Segments(newDstNode->width));
      componentMap[newSrcNode] = new NodeComponent();
      componentMap[newSrcNode]->addElementAll(new NodeElement(ELE_SPACE));
      nodeSegments[newSrcNode] = std::make_pair(new Segments(newSrcNode->width), new Segments(newSrcNode->width));
      if (node->updateTree) newSrcNode->updateTree = dupSplittedTree(node->updateTree, node, newSrcNode);
      if (node->resetTree) newSrcNode->resetTree = dupSplittedTree(node->resetTree, node, newSrcNode);
      splittedNode = newSrcNode;
    } else {
      splittedNode = node->dup(node->type, node->name + format("$%d_%d", hi, lo));
      splittedNode->width = hi - lo + 1;
      splittedNode->super = new SuperNode(splittedNode);
      componentMap[splittedNode] = new NodeComponent();
      componentMap[splittedNode]->addElementAll(new NodeElement(ELE_SPACE));
      nodeSegments[splittedNode] = std::make_pair(new Segments(splittedNode->width), new Segments(splittedNode->width));
    }

    for (int i = lo; i <= hi; i ++) splittedNodesSeg[node][i] = std::make_pair(splittedNode, i - lo);
    splittedNodesSet[node].push_back(splittedNode);
    allSplittedNodes.insert(splittedNode);
    for (ExpTree* tree: node->assignTree) {
      ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
      newTree->setlval(new ENode(splittedNode));
      splittedNode->assignTree.push_back(newTree);
    }
    if (node->type == NODE_REG_SRC) {
      for (int i = lo; i <= hi; i ++) splittedNodesSeg[node->getDst()][i] = std::make_pair(splittedNode->getDst(), i - lo);
      splittedNodesSet[node->getDst()].push_back(splittedNode->getDst());
      allSplittedNodes.insert(splittedNode->getDst());
      for (ExpTree* tree: node->getDst()->assignTree) {
        ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
        newTree->setlval(new ENode(splittedNode->getDst()));
        splittedNode->getDst()->assignTree.push_back(newTree);
      }
      splittedNode->getDst()->order = node->getDst()->order - 1;
      splittedNode->getDst()->next.insert(node->getDst());
    }
    splittedNode->order = node->order - 1;
    splittedNode->next.insert(node);
    lo = hi + 1;
  }
  for (Node* n : splittedNodesSet[node]) n->updateConnect();
  node->prev.clear();
  node->prev.insert(splittedNodesSet[node].begin(), splittedNodesSet[node].end());
  componentMap[node]->invalidateAll();
  std::reverse(splittedNodesSet[node].begin(), splittedNodesSet[node].end());

  if (node->type == NODE_REG_SRC) {
    for (Node* n : splittedNodesSet[node->getDst()]) n->updateConnect();
    node->getDst()->prev.clear();
    node->getDst()->prev.insert(splittedNodesSet[node->getDst()].begin(), splittedNodesSet[node->getDst()].end());
    std::reverse(splittedNodesSet[node->getDst()].begin(), splittedNodesSet[node->getDst()].end());
  }
}

void addRefer(NodeComponent* dst, NodeComponent* comp) {
  for (size_t i = 0; i < comp->elements.size(); i ++)
    dst->elements[0]->referNodes.insert(comp->elements[i]->referNodes.begin(), comp->elements[i]->referNodes.end());
}

void addDirectRef(NodeComponent* dst, NodeComponent* comp) {
  for (size_t i = 0; i < comp->directElements.size(); i ++)
    dst->directElements[0]->referNodes.insert(comp->directElements[i]->referNodes.begin(), comp->directElements[i]->referNodes.end());
}

NodeComponent* mergeMux(NodeComponent* comp1, NodeComponent* comp2, int width) {
  NodeComponent* ret;
  if (comp1->elements.size() == 1 && comp1->elements[0]->eleType == ELE_INT) {
    ret = comp2->dup();
    if (comp1->width > comp2->width) ret->addfrontElement(new NodeElement(ELE_EMPTY, nullptr, comp1->width - comp2->width - 1, 0));
    ret->invalidateAll();
  } else if (comp2->elements.size() == 1 && comp2->elements[0]->eleType == ELE_INT) {
    ret = comp1->dup();
    if (comp2->width > comp1->width) ret->addfrontElement(new NodeElement(ELE_EMPTY, nullptr, comp2->width - comp1->width - 1, 0));
    ret->invalidateAll();
  } else if (comp1->assignSegEq(comp2)) {
    ret = comp1->dup();
    ret->invalidateAll();
    for (size_t i = 0; i < comp1->elements.size(); i ++) {
      ret->elements[i]->referNodes.insert(comp2->elements[i]->referNodes.begin(), comp2->elements[i]->referNodes.end());
    }
  } else {
    ret = spaceComp(width);
    addRefer(ret, comp1);
    addRefer(ret, comp2);
  }
  ret->invalidateDirectAsWhole();
  addDirectRef(ret, comp1);
  addDirectRef(ret, comp2);
  return ret;
}

bool compMergable(NodeElement* ele1, NodeElement* ele2) {
  if (ele1->eleType != ele2->eleType) return false;
  if (ele1->eleType == ELE_NODE) return ele1->node == ele2->node && ele1->lo == ele2->hi + 1;
  if (ele1->eleType == ELE_SPACE) return false;
  else return true;
}

NodeComponent* merge2(NodeComponent* comp1, NodeComponent* comp2) {
  NodeComponent* ret = new NodeComponent();
  ret->merge(comp1->dup());
  ret->merge(comp2->dup());
  return ret;
}

bool enodeNeedReplace(ENode* enode, NodeComponent* comp) {
  if (!comp->fullValid()) return false;
  if (comp->elements.size() == 1) {
    if (comp->elements[0]->eleType == ELE_INT) return true;
    // if (comp->elements[0]->hi - comp->elements[0]->lo + 1 == comp->elements[0]->node->width) return true;
  }
  std::set<Node*> relyNodes;
  getENodeRelyNodes(enode, relyNodes);
  bool anyDead = false;
  for (Node* rely : relyNodes) {
    if (rely->status == SPLITTED_NODE) anyDead = true;
  }
  if (anyDead) return true;
  return false;
  if (!anyDead) return false;
  for (NodeElement* element : comp->elements) {
    if (element->eleType == ELE_NODE) {
      /* TODO: add direct next*/
      if (allSplittedNodes.find(element->node) != allSplittedNodes.end()) return true;
    }
  }
  return false;
}

NodeComponent* ENode::inferComponent(Node* n) {
  if (nodePtr) {
    for (ENode* childENode : child) childENode->inferComponent(n);
    Node* node = getLeafNode(true, this);
    if (node->isArray() || node->sign) {
      NodeComponent* ret = spaceComp(node->width);
      componentEMap[this] = ret;
      return ret;
    }
    Assert(componentMap.find(node) != componentMap.end(), "%s is not visited when infer %s %p", node->name.c_str(), n->name.c_str(), this);
    NodeComponent* ret = componentMap[node]->getbits(width-1, 0);
    if (!ret->fullValid()) {
      ret = new NodeComponent();
      ret->addElement(new NodeElement(ELE_NODE, node, width - 1, 0));
    }
    /* update directElement */
    ret->directElements.clear();
    if (splittedNodesSet.find(node) != splittedNodesSet.end()) {
      for (Node* splittedNode : splittedNodesSet[node]) {
        ret->addDirectElement(new NodeElement(ELE_NODE, splittedNode, splittedNode->width - 1, 0));
      }
    } else
      ret->addDirectElement(new NodeElement(ELE_NODE, node, width - 1, 0));
    componentEMap[this] = ret;
    return ret;
  }
  std::vector<NodeComponent*> childComp;
  for (ENode* childENode : child) {
    if (childENode) {
      NodeComponent* comp = childENode->inferComponent(n);
      childComp.push_back(comp);
    } else {
      childComp.push_back(nullptr);
    }
  }

  NodeComponent* ret = nullptr;
  switch (opType) {
    case OP_ADD: case OP_SUB: case OP_MUL: case OP_DIV: case OP_REM:
    case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
    case OP_DSHL: case OP_DSHR:
    case OP_ASUINT: case OP_ASSINT: case OP_CVT: case OP_NEG:
    case OP_ANDR: case OP_ORR: case OP_XORR: case OP_INDEX:
      ret = spaceComp(width);
      for (NodeComponent* comp : childComp) {
        addRefer(ret, comp);
        addDirectRef(ret, comp);
      }
      /* set all refer segments to arith */
      ret->setReferArith();
      break;
    case OP_AND: case OP_OR: case OP_XOR:
      ret = mergeMux(childComp[0], childComp[1], width);
      ret->setReferLogi();
    break;
    case OP_NOT:
      ret = childComp[0]->dup();
      ret->invalidateAll();
      ret->setReferLogi();
      break;
    case OP_BITS:
      ret = childComp[0]->getbits(values[0], values[1]);
      break;
    case OP_CAT: 
      ret = merge2(childComp[0], childComp[1]);
      break;
    case OP_INT: {
      std::string str;
      int base;
      std::tie(base, str) = firStrBase(strVal);
      ret = new NodeComponent();
      ret->addElementAll(new NodeElement(str, base, width-1, 0));
      break;
    }
    case OP_HEAD:
      ret = childComp[0]->getbits(childComp[0]->width - 1, values[0]);
      break;
    case OP_TAIL:
      ret = childComp[0]->getbits(values[0]-1, 0);
      break;
    case OP_SHL:
      ret = childComp[0]->dup();
      ret->addElementAll(new NodeElement("0", 16, values[0] - 1, 0));
      break;
    case OP_SHR:
      ret = childComp[0]->getbits(childComp[0]->width - 1, values[0]);
      break;
    case OP_PAD:
      ret = childComp[0]->getbits(values[0]-1, 0);
      break;
    case OP_WHEN:
      if (!getChild(1) && !getChild(2)) break;
      if (!getChild(1)) {
        ret = childComp[2]->dup();
        ret->invalidateAll();
        break;
      } else if (!getChild(2)) {
        ret = childComp[1]->dup();
        ret->invalidateAll();
        break;
      }
      /* otherwise continue... */
    case OP_MUX:
      ret = mergeMux(childComp[1], childComp[2], width);
      break;
    default:
      ret = spaceComp(width);
      for (NodeComponent* comp : childComp) {
        addRefer(ret, comp);
        addDirectRef(ret, comp);
      }
      break;
  }
  if (!ret) ret = spaceComp(width);
  componentEMap[this] = ret;
  return ret;
}

void ExpTree::clearComponent() {
  std::stack<ENode*> s;
  s.push(getRoot());
  if (getlval() && getlval()->child.size() != 0) {
    for (ENode* child : getlval()->child) s.push(child);
  }
  if (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    componentEMap.erase(top);
    for (ENode* child : top->child) {
      if (child) s.push(child);
    }
  }
}

void genReferSegment(Node* node, NodeComponent* comp) {
  for (NodeElement* element : comp->directElements) {
    if (element->eleType == ELE_SPACE) {
      for (auto iter : element->referNodes) {
        Node* referNode = referNode(iter);
        if (node == referNode) continue;
        int hi = referHi(iter), lo = referLo(iter);
        // printf("refer %s [%d, %d] from %s\n", referNode->name.c_str(), hi, lo, node->name.c_str());
        nodeSegments[referNode].first->addRange(hi, lo, referLevel(iter));
      }
    } else if (element->eleType == ELE_NODE) {
      Node* referNode = element->node;
      if (node == referNode) continue;
      // printf("refer %s [%d, %d] from %s\n", referNode->name.c_str(), element->hi, element->lo, node->name.c_str());
      nodeSegments[referNode].first->addRange(element->hi, element->lo, OPL_BITS);
    }
  }
}

void eraseReferSegment(Node* node, NodeComponent* comp) {
  for (NodeElement* element : comp->directElements) {
    if (element->eleType == ELE_SPACE) {
      for (auto iter : element->referNodes) {
        Node* referNode = referNode(iter);
        if (node == referNode) continue;
        int hi = referHi(iter), lo = referLo(iter);
        nodeSegments[referNode].first->eraseRange(hi, lo, referLevel(iter));
      }
    } else if (element->eleType == ELE_NODE) {
      Node* referNode = element->node;
      if (node == referNode) continue;
      nodeSegments[referNode].first->eraseRange(element->hi, element->lo, OPL_BITS);
    }
  }
}

NodeComponent* Node::reInferComponent() {
  NodeComponent* oldComp = componentMap[this];
  /* eliminate effects on segments */
  eraseReferSegment(this, oldComp);
  /* erase old component */
  componentMap.erase(this);
  for (ExpTree* tree : arrayVal) tree->clearComponent();
  for (ExpTree* tree : assignTree) tree->clearComponent();
  NodeComponent* newComp = inferComponent();
  if (!oldComp->assignAllEq(newComp)) {
    for (Node* nextNode : next) addReInfer(nextNode);
  }
  componentMap[this] = newComp;
  nodeSegments[this].second = new Segments(newComp);
  // printf("reinfer node %s (%p) eq %d\n", name.c_str(), this, oldComp->assignAllEq(newComp));
  // display();
  // newComp->display();
  genReferSegment(this, newComp);
  return newComp;
}

void reInferAll(bool record, std::set<Node*>& reinferNodes) {
  while(!reInferQueue.empty()) {
    Node* node = reInferQueue.top();
    reInferQueue.pop();
    uniqueReinfer.erase(node);
    // if (splittedNodesSeg.find(node) != splittedNodesSeg.end()) {
    //   for (Node* nextNode : splittedNodesSet[node]) addReInfer(nextNode);
      // continue;
    // }
    if (splittedNodesSeg.find(node) == splittedNodesSeg.end() && record) reinferNodes.insert(node);
    node->reInferComponent();
  }
}

void reInferAll() {
  std::set<Node*> tmp;
  reInferAll(false, tmp);
}

void addReInfer(Node* node) {
  if (uniqueReinfer.find(node) == uniqueReinfer.end()) {
    uniqueReinfer.insert(node);
    reInferQueue.push(node);
  }
}

NodeComponent* Node::inferComponent() {
  if (splittedNodesSet.find(this) != splittedNodesSet.end()) {
    NodeComponent* ret = new NodeComponent();
    for (int i = splittedNodesSet[this].size() - 1; i >= 0; i --) {
      NodeComponent* comp = componentMap[splittedNodesSet[this][i]];
      if (comp->fullValid()) ret->merge(comp->dup());
      else ret->addElement(new NodeElement(ELE_NODE, splittedNodesSet[this][i], splittedNodesSet[this][i]->width - 1, 0));
    }
    return ret;
  }
  if (assignTree.size() != 1 || isArray() || sign || width == 0) {
    NodeComponent* ret = spaceComp(width);
    for (ExpTree* tree : assignTree) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this);
      addRefer(ret, comp);
      addDirectRef(ret, comp);
    }
    for (ExpTree* tree : arrayVal) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this);
      addRefer(ret, comp);
      addDirectRef(ret, comp);
      if (tree->getlval() && tree->getlval()->child.size() != 0) {
        for (ENode* child : tree->getlval()->child) {
          NodeComponent* indexComp = child->inferComponent(this);
          addRefer(ret, indexComp);
          addDirectRef(ret, comp);
        }
      }
    }
    return ret;
  }
  NodeComponent* ret = assignTree[0]->getRoot()->inferComponent(this);
  ret->mergeNeighbor();
  return ret;
}

void setComponent(Node* node, NodeComponent* comp) {
  if (comp->fullValid()) {
    for (NodeElement* element : comp->elements) {
      element->referNodes.clear();
      if (element->eleType == ELE_NODE) element->referNodes.insert(std::make_tuple(element->node, element->hi, element->lo, OPL_BITS));
    }
    for (NodeElement* element : comp->directElements) {
      element->referNodes.clear();
      if (element->eleType == ELE_NODE) element->referNodes.insert(std::make_tuple(element->node, element->hi, element->lo, OPL_BITS));
    }
  }
  componentMap[node] = comp;
/* update node segment */
  if (nodeSegments.find(node) == nodeSegments.end()) nodeSegments[node] = std::make_pair(new Segments(node->width), new Segments(node->width));
/* update segment*/
  nodeSegments[node].second->construct(comp);
}

void ExpTree::replaceAndUpdateWidth(Node* oldNode, Node* newNode) {
  std::stack<ENode*> s;
  if (getRoot()) s.push(getRoot());
  if (getlval()) s.push(getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (!top->nodePtr && top->opType != OP_INT) top->width = -1; // clear width
    if (top->getNode() && getLeafNode(true, top) == oldNode) {
      top->nodePtr = newNode;
      top->child.clear();
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  getRoot()->inferWidth();
  getRoot()->usedBitWithFixRoot(newNode->width);
}

ExpTree* dupSplittedTree(ExpTree* tree, Node* regold, Node* regnew) {
  ENode* lvalue = tree->getlval()->dup();
  ENode* rvalue = tree->getRoot()->dup();
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  ret->replaceAndUpdateWidth(regold, regnew);
  ret->replaceAndUpdateWidth(regold->getDst(), regnew->getDst());
  ret->getRoot()->clearWidth();
  ret->getRoot()->inferWidth();
  ret->getRoot()->usedBitWithFixRoot(regnew->width);
  return ret;
}

ExpTree* dupTreeWithBits(ExpTree* tree, int _hi, int _lo) {
  ENode* lvalue = tree->getlval()->dup();
  ENode* rvalue = tree->getRoot()->dup();
  std::stack<std::tuple<ENode*, ENode*, int, int, int>> s;
  s.push(std::make_tuple(rvalue, nullptr, -1, _hi, _lo));
  while(!s.empty()) {
    ENode* top, *parent;
    int idx, hi, lo;
    std::tie(top, parent, idx, hi, lo) = s.top();
    s.pop();
    if (top->nodePtr || top->opType == OP_INT) {
      ENode* bits = new ENode(OP_BITS);
      bits->addChild(top);
      bits->addVal(hi);
      bits->addVal(lo);
      bits->width = hi - lo + 1;
      if (parent) parent->child[idx] = bits;
      else rvalue = bits;
    } else {
      switch(top->opType) {
        case OP_WHEN: case OP_MUX:
          if (top->getChild(1)) s.push(std::make_tuple(top->getChild(1), top, 1, hi, lo));
          if (top->getChild(2)) s.push(std::make_tuple(top->getChild(2), top, 2, hi, lo));
          break;
        case OP_RESET:
          if (top->getChild(1)) s.push(std::make_tuple(top->getChild(1), top, 1, hi, lo));
          break;
        case OP_CAT:
          if (lo >= top->getChild(1)->width) {
            if (parent) parent->child[idx] = top->getChild(0);
            else rvalue = top->getChild(0);
            s.push(std::make_tuple(top->getChild(0), parent, idx, hi - top->getChild(1)->width, lo - top->getChild(1)->width));
          } else if (hi < top->getChild(1)->width) {
            if (parent) parent->child[idx] = top->getChild(1);
            else rvalue = top->getChild(1);
            s.push(std::make_tuple(top->getChild(1), parent, idx, hi, lo));
          } else {
            s.push(std::make_tuple(top->getChild(0), top, 0, hi - top->getChild(1)->width, 0));
            s.push(std::make_tuple(top->getChild(1), top, 1, top->getChild(1)->width - 1, lo));
          }
          break;
        case OP_PAD:
          Assert(hi < top->width, "invalid bits %p [%d, %d]", top, hi, lo);
          if (lo >= top->getChild(0)->width) {
            Assert(!top->sign, "invalid sign");
            top->opType = OP_INT;
            top->strVal = "0";
            top->values.clear();
            top->width = hi - lo + 1;
            top->child.clear();
          } else if (hi < top->getChild(0)->width) {
            if (parent) parent->child[idx] = top->getChild(0);
            else rvalue = top->getChild(0);
            s.push(std::make_tuple(top->getChild(0), parent, idx, hi, lo));
          } else {
            top->values[0] = hi - lo + 1;
            s.push(std::make_tuple(top->getChild(0), top, 0, top->getChild(0)->width - 1, lo));
          }
          break;
        case OP_SHL:
          if (top->values[0] > hi) { // constant zero
            top->opType = OP_INT;
            top->strVal = "0";
            top->values.clear();
            top->width = hi - lo + 1;
            top->child.clear();
          } else if (top->values[0] <= lo) {
            if (parent) parent->child[idx] = top->getChild(0);
            else rvalue = top->getChild(0);
            s.push(std::make_tuple(top->getChild(0), parent, 0, hi - top->values[0], lo - top->values[0]));
          } else {
            top->values[0] = top->values[0] - lo;
            s.push(std::make_tuple(top->getChild(0), top, 0, hi - top->values[0], 0));
          }
          break;
        case OP_BITS:
          Assert(hi < top->width, "invalid bits %p [%d, %d]", top, hi, lo);
          top->values[0] = top->values[1] + hi;
          top->values[1] = top->values[1] + lo;
          top->width = hi - lo + 1;
          break;
        case OP_AND:
        case OP_OR:
        case OP_NOT:
        case OP_XOR:
          for (size_t i = 0; i < top->child.size(); i ++) {
            if (top->getChild(i)) s.push(std::make_tuple(top->getChild(i), top, i, hi, lo));
          }
          break;
        case OP_ADD: {
          ENode* bits = new ENode(OP_BITS);
          bits->addVal(hi);
          bits->addVal(lo);
          bits->width = hi - lo + 1;
          bits->addChild(top);
          if (parent) parent->child[idx] = bits;
          else rvalue = bits;
          s.push(std::make_tuple(top->getChild(0), top, 0, hi, 0));
          s.push(std::make_tuple(top->getChild(1), top, 1, hi, 0));
          break;
        }
        case OP_DSHR: case OP_DSHL: case OP_SUB: {
          ENode* bits = new ENode(OP_BITS);
          bits->addVal(hi);
          bits->addVal(lo);
          bits->width = hi - lo + 1;
          bits->addChild(top);
          if (parent) parent->child[idx] = bits;
          else rvalue = bits;
          break;
        }
        case OP_INVALID:
          top->width = hi - lo + 1;
          break;
        case OP_SHR: case OP_HEAD: {
          top->opType = OP_BITS;
          top->width = hi - lo + 1;
          int shiftNum = top->values[0];
          top->values.clear();
          top->addVal(shiftNum + hi);
          top->addVal(shiftNum + lo);
          break;
        }
        default:
          Assert(0, "invalid opType %d", top->opType);
          for (size_t i = 0; i < top->child.size(); i ++) {
            if (top->getChild(i)) s.push(std::make_tuple(top->getChild(i), top, i, hi, lo));
          }
      }
    }
  }
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  ret->getRoot()->clearWidth();
  ret->getRoot()->inferWidth();
  ret->getRoot()->usedBitWithFixRoot(_hi - _lo + 1);
  return ret;
}

ENode* constructRootFromComponent(NodeComponent* comp) {
  int hiBit = comp->width - 1;
  ENode* newRoot = nullptr;
  for (NodeElement* element : comp->directElements) {
    ENode* enode;
    if (element->eleType == ELE_NODE) {
      enode = new ENode(element->node);
      enode->width = element->node->width;
      int validHi = enode->width - 1;
      int validLo = 0;
      if (hiBit > element->hi) {
        validHi += hiBit - element->hi;
        validLo += hiBit - element->hi;
        ENode* lshift = new ENode(OP_SHL);
        lshift->addVal(hiBit - element->hi);
        lshift->addChild(enode);
        lshift->width = validHi + 1;
        enode = lshift;
      } else if (hiBit < element->hi) {
        validHi -= element->hi - hiBit;
        validLo = 0;
        ENode* rshift = new ENode(OP_SHR);
        rshift->addVal(element->hi - hiBit);
        rshift->addChild(enode);
        rshift->width = validHi + 1;
        enode = rshift;
      }

      if ((validHi - validLo) > (element->hi - element->lo) || validHi > hiBit) {
        ENode* bits = new ENode(OP_BITS_NOSHIFT);
        bits->addChild(enode);
        bits->addVal(hiBit);
        bits->addVal(hiBit - (element->hi - element->lo));
        bits->width = hiBit + 1;
        enode = bits;
      }
    } else {
      mpz_t realVal;
      mpz_init(realVal);
      if (hiBit > element->hi) mpz_mul_2exp(realVal, element->val, hiBit - element->hi);
      else if (hiBit < element->hi) mpz_tdiv_q_2exp(realVal, element->val, element->hi - hiBit);
      else mpz_set(realVal, element->val);

      enode = new ENode(OP_INT);
      enode->strVal = mpz_get_str(nullptr, 10, realVal);
      enode->width = hiBit + 1;

    }
    if (newRoot) {
      ENode* orNode = new ENode(OP_OR);
      orNode->addChild(newRoot);
      orNode->addChild(enode);
      orNode->width = MAX(newRoot->width, enode->width);
      newRoot = orNode;
    } else newRoot = enode;
    hiBit -= element->hi - element->lo + 1;
  }
  return newRoot;
}

void ExpTree::updateWithSplittedNode() {
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, -1));
  if (getlval() && getlval()->child.size() != 0) {
    for (size_t i = 0; i < getlval()->child.size(); i ++) s.push(std::make_tuple(getlval()->getChild(i), getlval(), i));
  }
  while (!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (componentEMap.find(top) == componentEMap.end()) continue;
    // printf("enode %p\n", top);
    // componentEMap[top]->display();
    componentEMap[top]->mergeNeighbor();
    if (enodeNeedReplace(top, componentEMap[top])) {
      ENode* replaceENode = nullptr;

      replaceENode = constructRootFromComponent(componentEMap[top]);
      if (parent) {
        parent->setChild(idx, replaceENode);
      } else setRoot(replaceENode);
    } else {
      for (size_t i = 0; i < top->child.size(); i ++) {
        if (top->child[i]) s.push(std::make_tuple(top->child[i], top, i));
      }
    }
  }
}

int splitDelta(Node* node, int interval) {
  if (interval >= BASIC_WIDTH) return 0;
  if (interval >= 64) return 2;
  if (interval >= 32) return 4;
  if (interval >= 16) return 8;
  if (interval >= 8) return 16;
  return 16;
}

void getCut(Node* node, std::set<int>& cuts, Segments* seg1, Segments* seg2) {
  /* merge boundCount */
  std::map<int, int> allCut(seg1->boundCount);
  for (auto iter : seg2->boundCount) {
    if (allCut.find(iter.first) == allCut.end()) allCut[iter.first] = 0;
    allCut[iter.first] += iter.second;
  }
  int lo = -1;
  for (auto iter : allCut) {
    int cut = iter.first;
    int concatNum = 0;
    if (seg1->concatCount.find(cut) != seg1->concatCount.end()) concatNum += seg1->concatCount[cut];
    if (seg2->concatCount.find(cut) != seg2->concatCount.end()) concatNum += seg2->concatCount[cut];
    if (allCut[cut] >= concatNum + splitDelta(node, cut - lo)) {
      cuts.insert(cut);
      lo = cut;
    }
  }
}

void graph::splitNodes() {
  int num = 0;
  /* initialize nodeSegments */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) nodeSegments[node] = std::make_pair(new Segments(node->width), new Segments(node->width));
  }
/* update nodeComponent & nodeSegments */
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      NodeComponent* comp = node->inferComponent();
      setComponent(node, comp);
      // printf("after infer %s\n", node->name.c_str());
      // node->display();
      // componentMap[node]->display();
      Assert(node->width == componentMap[node]->countWidth(), "%s width not match %d != %d", node->name.c_str(), node->width, comp->width);
    }
  }
  /* update refer & update segments for each node */
  std::set<Node*> validNodes;
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    for (int j = sortedSuper[i]->member.size() - 1; j >= 0; j --) {
      Node* node = sortedSuper[i]->member[j];
      NodeComponent* comp = componentMap[node];
      validNodes.insert(node);
      genReferSegment(node, comp);
    }
  }

  std::set<Node*> checkNodes(validNodes);
  std::set<Node*> arrayMember;
  for (Node* node : validNodes) {
    for (Node* next : node->next) {
      if (next->isArray()) arrayMember.insert(node);
    }
  }
  while (!checkNodes.empty()) {
    /* split common nodes */
    for (Node* node : checkNodes) {
      // printf("node %s(w = %d, type %d):\n", node->name.c_str(), node->width, node->type);
      Node* updateNode = node->type == NODE_REG_SRC ? node->getDst() : node;
      // for (auto cut : nodeSegments[node].first->boundCount) printf("[%d]=%d ", cut.first, cut.second);
      // for (auto cut : nodeSegments[updateNode].second->boundCount) printf("[%d]=%d ", cut.first, cut.second);
      // printf("\n-------\n");
      // for (auto cut : nodeSegments[node].first->concatCount) printf("[%d]=%d ", cut.first, cut.second);
      // for (auto cut : nodeSegments[updateNode].second->concatCount) printf("[%d]=%d ", cut.first, cut.second);
      // printf("\n-------\n");
      if ((node->type != NODE_OTHERS && node->type != NODE_REG_SRC) || node->width == 0 || node->sign || allSplittedNodes.find(node) != allSplittedNodes.end()) continue;
      if (arrayMember.find(node) != arrayMember.end()) continue;
      std::set<int>nodeCuts;
      getCut(node, nodeCuts, nodeSegments[node].first, nodeSegments[updateNode].second);
      nodeCuts.insert(node->width - 1);
      if (nodeCuts.size() <= 1) {
        Assert(nodeCuts.size() == 1 && *nodeCuts.begin() == node->width - 1, "invalid cut %ld %d %s", nodeCuts.size(), nodeCuts.size() == 0 ? -1 : *nodeCuts.begin(), node->name.c_str());
        continue;
      }

      // printf("splitNode %s %p (type %d, width %d)\n", node->name.c_str(), node, node->type, node->width);
      // for (int cut : nodeCuts) printf("%d ", cut);
      // printf("\n");
      createSplittedNode(node, nodeCuts);
      componentMap[node]->display();
      for (Node* n : splittedNodesSet[node]) {
        n->display();
        addReInfer(n);
      }
      node->status = SPLITTED_NODE;
      addReInfer(node);
      /* extra update for registers */
      if (node->type == NODE_REG_SRC) {
        if (supersrc.find(node->super) != supersrc.end()) {
          supersrc.erase(node->super);
        }
        for (Node* splittedNode : splittedNodesSet[node]) regsrc.push_back(splittedNode);
        addReInfer(node->getDst());
        for (Node* dst : splittedNodesSet[node->getDst()]) addReInfer(dst);
        node->getDst()->status = SPLITTED_NODE;
      }
    }
    checkNodes.clear();
    reInferAll(true, checkNodes);
  }

  /* update assignTree*/
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->width == 0) continue;
      for (ExpTree* tree : member->assignTree) tree->updateWithSplittedNode();
      for (ExpTree* tree : member->arrayVal) tree->updateWithSplittedNode();
    }
  }
  for (Node* node : allSplittedNodes) {
    for (ExpTree* tree : node->assignTree) tree->updateWithSplittedNode();
    for (ExpTree* tree : node->arrayVal) tree->updateWithSplittedNode();
  }

/* update superNodes, replace reg by splitted regs */
  for (size_t i = 0; i < sortedSuper.size(); i ++) {
    for (Node* member : sortedSuper[i]->member) {
      if (member->type == NODE_REG_SRC || member->type == NODE_REG_DST) {
        Assert(sortedSuper[i]->member.size() == 1, "invalid merged SuperNode");
      }
      if ((member->type == NODE_REG_SRC || member->type == NODE_OTHERS) && splittedNodesSet.find(member) != splittedNodesSet.end()) {
        std::set<SuperNode*> replaceSuper;
        for (Node* node : splittedNodesSet[member]) {
          replaceSuper.insert(node->super);
        }
        sortedSuper.erase(sortedSuper.begin() + i);
        sortedSuper.insert(sortedSuper.begin() + i, replaceSuper.begin(), replaceSuper.end());
        i += replaceSuper.size() - 1;
      }
      if (member->type == NODE_REG_DST && splittedNodesSet.find(member->getSrc()) != splittedNodesSet.end()) {
        std::set<SuperNode*> replaceSuper;
        for (Node* node : splittedNodesSet[member->getSrc()]) {
          replaceSuper.insert(node->getDst()->super);
        }
        sortedSuper.erase(sortedSuper.begin() + i);
        sortedSuper.insert(sortedSuper.begin() + i, replaceSuper.begin(), replaceSuper.end());
        i += replaceSuper.size() - 1;
      }
    }
  }
  regsrc.erase(
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == SPLITTED_NODE; }),
        regsrc.end()
  );
  removeNodesNoConnect(SPLITTED_NODE);
/* update connection */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  reconnectAll();

  printf("[splitNode] update %d nodes (total %ld)\n", num, countNodes());
  printf("[splitNode] split %ld nodes (total %ld)\n", splittedNodesSeg.size(), countNodes());
}