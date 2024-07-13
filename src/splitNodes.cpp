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
static std::map<Node*, std::vector<std::pair<Node*, int>>> splittedNodes;
static std::map<Node*, std::set<Node*>> splittedNodesSet;

static std::priority_queue<Node*, std::vector<Node*>, ordercmp> reInferQueue;
static std::set<Node*> uniqueReinfer;

static std::map<Node*, int> reinferCount;

ExpTree* dupSplittedTree(ExpTree* tree, Node* regold, Node* regnew);
ExpTree* dupTreeWithBits(ExpTree* tree, int hi, int lo);
void addReInfer(Node* node);

NodeComponent* spaceComp(int width) {
  NodeComponent* comp = new NodeComponent();
  comp->addElement(new NodeElement(ELE_SPACE, nullptr, width - 1, 0));
  return comp;
}

void createSplittedNode(Node* node, Segments* seg) {
  splittedNodes[node] = std::vector<std::pair<Node*, int>>();
  splittedNodes[node].resize(node->width);
  int lo = 0;
  NodeComponent* comp = new NodeComponent();
  std::vector<NodeElement*> elements;
  for (int hi : seg->cuts) {
    Node* splittedNode = nullptr;
    if (node->type == NODE_REG_SRC) {
      Node* newSrcNode = node->dup(node->type, node->name + format("$%d_%d", hi, lo));
      newSrcNode->width = hi - lo + 1;
      newSrcNode->super = new SuperNode(newSrcNode);
      Node* newDstNode = node->dup(node->getDst()->type, node->getDst()->name + format("$%d_%d", hi, lo));
      newDstNode->width = hi - lo + 1;
      newDstNode->super = new SuperNode(newDstNode);
      newSrcNode->bindReg(newDstNode);
      componentMap[newDstNode] = new NodeComponent();
      componentMap[newDstNode]->addElement(new NodeElement(ELE_SPACE));
      componentMap[newSrcNode] = new NodeComponent();
      componentMap[newSrcNode]->addElement(new NodeElement(ELE_SPACE));
      nodeSegments[newDstNode] = std::make_pair(new Segments(newDstNode->width), new Segments(newDstNode->width));
      nodeSegments[newSrcNode] = std::make_pair(new Segments(newSrcNode->width), new Segments(newSrcNode->width));
      if (node->updateTree) newSrcNode->updateTree = dupSplittedTree(node->updateTree, node, newSrcNode);
      if (node->resetTree) newSrcNode->resetTree = dupSplittedTree(node->resetTree, node, newSrcNode);
      splittedNode = newSrcNode;
      for (ExpTree* tree: node->getDst()->assignTree) {
        ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
        newTree->setlval(new ENode(newDstNode));
        newDstNode->assignTree.push_back(newTree);
      }
      splittedNodesSet[node->getDst()].insert(newDstNode);
    } else {
      splittedNode = node->dup(node->type, node->name + format("$%d_%d", hi, lo));
      splittedNode->width = hi - lo + 1;
      splittedNode->super = new SuperNode(splittedNode);
      componentMap[splittedNode] = new NodeComponent();
      componentMap[splittedNode]->addElement(new NodeElement(ELE_SPACE));
      nodeSegments[splittedNode] = std::make_pair(new Segments(splittedNode->width), new Segments(splittedNode->width));
    }
    elements.push_back(new NodeElement(ELE_NODE, splittedNode, splittedNode->width - 1, 0));

    for (int i = lo; i <= hi; i ++) splittedNodes[node][i] = std::make_pair(splittedNode, i - lo);
    splittedNodesSet[node].insert(splittedNode);

    for (ExpTree* tree: node->assignTree) {
      ExpTree* newTree = dupTreeWithBits(tree, hi, lo);
      newTree->setlval(new ENode(splittedNode));
      splittedNode->assignTree.push_back(newTree);
    }
    lo = hi + 1;
  }
  for (int i = (int)elements.size() - 1; i >= 0; i--) comp->addElement(elements[i]);

  componentMap[node] = comp;
  if (node->type == NODE_REG_SRC) {
    NodeComponent* dstcomp = new NodeComponent();
    for (NodeElement* element : comp->elements) dstcomp->addElement(new NodeElement(ELE_NODE, element->node->getDst(), element->hi, element->lo));
  }
}

void addRefer(NodeComponent* dst, NodeComponent* comp) {
  for (size_t i = 0; i < comp->elements.size(); i ++)
    dst->elements[0]->referNodes.insert(comp->elements[i]->referNodes.begin(), comp->elements[i]->referNodes.end());
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
  ret->merge(comp1);
  ret->merge(comp2);
  return ret;
}

Node* getLeafNode(bool isArray, ENode* enode);

NodeComponent* ENode::inferComponent(Node* n) {
  if (nodePtr) {
    for (ENode* childENode : child) childENode->inferComponent(n);
    Node* node = getLeafNode(true, this);
    if (node->isArray() || node->sign) {
      NodeComponent* ret = spaceComp(node->width);
      componentEMap[this] = ret;
      return ret;
    }
    Assert(componentMap.find(node) != componentMap.end(), "%s is not visited", node->name.c_str());
    NodeComponent* ret = componentMap[node]->getbits(width-1, 0);
    if (!ret->fullValid()) {
      ret = new NodeComponent();
      ret->addElement(new NodeElement(ELE_NODE, node, width - 1, 0));
    }
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
      }
      /* set all refer segments to arith */
      ret->setReferArith();
      break;
    case OP_AND: case OP_OR: case OP_XOR:
      ret = mergeMux(childComp[0], childComp[1], width);
      ret->setReferLogi();
    break;
    case OP_NOT:
      ret = childComp[0];
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
      ret->addElement(new NodeElement(str, base, width-1, 0));
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
      ret->addElement(new NodeElement("0", 16, values[0] - 1, 0));
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
  if (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    componentEMap.erase(top);
    for (ENode* child : top->child) {
      if (child) s.push(child);
    }
  }
}

NodeComponent* Node::reInferComponent() {
  if (reinferCount.find(this) == reinferCount.end()) reinferCount[this] = 0;
  reinferCount[this] ++;

  NodeComponent* oldComp = componentMap[this];
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
  printf("reinfer node %s (times %d) cuts %ld %p\n", name.c_str(), reinferCount[this], nodeSegments[this].second->cuts.size(), nodeSegments[this].second);
  display();
  newComp->display();
  return newComp;
}

void reInferAll(bool record, std::set<Node*>& reinferNodes) {
  while(!reInferQueue.empty()) {
    Node* node = reInferQueue.top();
    reInferQueue.pop();
    uniqueReinfer.erase(node);
    if (splittedNodes.find(node) != splittedNodes.end()) {
      for (Node* nextNode : splittedNodesSet[node]) addReInfer(nextNode);
      continue;
    }
    if (record) reinferNodes.insert(node);
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
  if (assignTree.size() != 1 || isArray() || sign || width == 0) {
    NodeComponent* ret = spaceComp(width);
    for (ExpTree* tree : assignTree) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this);
      addRefer(ret, comp);
    }
    for (ExpTree* tree : arrayVal) {
      NodeComponent* comp = tree->getRoot()->inferComponent(this);
      addRefer(ret, comp);
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
  }
  componentMap[node] = comp;
/* update node segment */
  if (nodeSegments.find(node) == nodeSegments.end()) nodeSegments[node] = std::make_pair(new Segments(node->width), new Segments(node->width));
/* update segment*/
  nodeSegments[node].second->construct(comp);
}

void ExpTree::replace(Node* oldNode, Node* newNode) {
  std::stack<ENode*> s;
  if (getRoot()) s.push(getRoot());
  if (getlval()) s.push(getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    top->width = -1; // clear width
    if (top->getNode() && getLeafNode(true, top) == oldNode) {
      top->nodePtr = newNode;
      top->child.clear();
    }
    for (ENode* childENode : top->child) {
      if (childENode) s.push(childENode);
    }
  }
  getRoot()->inferWidth();
}

ExpTree* dupSplittedTree(ExpTree* tree, Node* regold, Node* regnew) {
  ENode* lvalue = tree->getlval()->dup();
  ENode* rvalue = tree->getRoot()->dup();
  ExpTree* ret = new ExpTree(rvalue, lvalue);
  ret->replace(regold, regnew);
  ret->replace(regold->getDst(), regnew->getDst());
  return ret;
}

ExpTree* dupTreeWithBits(ExpTree* tree, int _hi, int _lo) {
  tree->display();
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
            s.push(std::make_tuple(top->getChild(0), top, 0, hi - top->getChild(1)->width, lo - top->getChild(1)->width));
          } else if (hi < top->getChild(1)->width) {
            if (parent) parent->child[idx] = top->getChild(1);
            else rvalue = top->getChild(1);
            s.push(std::make_tuple(top->getChild(1), top, 1, hi, lo));
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
          } else if (hi < top->getChild(0)->width) {
            if (parent) parent->child[idx] = top->getChild(0);
            else rvalue = top->getChild(0);
            s.push(std::make_tuple(top->getChild(0), top, 0, hi, lo));
          } else {
            top->values[0] = hi;
            s.push(std::make_tuple(top->getChild(0), top, 0, top->getChild(0)->width - 1, lo));
          }
          break;
        case OP_BITS:
          Assert(hi < top->width, "invalid bits %p [%d, %d]", top, hi, lo);
          top->values[0] = top->values[1] + hi;
          top->values[1] = top->values[1] + lo;
          break;
        case OP_AND:
        case OP_OR:
        case OP_NOT:
        case OP_XOR:
          for (size_t i = 0; i < top->child.size(); i ++) {
            if (top->getChild(i)) s.push(std::make_tuple(top->getChild(i), top, i, hi, lo));
          }
          break;
        case OP_SHR:
        case OP_SHL:
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
  return ret;
}

void ExpTree::updateWithSplittedNode() {
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  s.push(std::make_tuple(getRoot(), nullptr, -1));
  while (!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (componentEMap.find(top) != componentEMap.end() && componentEMap[top]->fullValid()) {
      ENode* replaceENode = nullptr;
      for (NodeElement* element : componentEMap[top]->elements) {
        ENode* enode;
        if (element->eleType == ELE_NODE) {
          enode = new ENode(element->node);
          enode->width = element->node->width;
          if (element->hi - element->lo + 1 != element->node->width) {
            ENode* bits = new ENode(OP_BITS);
            bits->addVal(element->hi);
            bits->addVal(element->lo);
            bits->width = element->hi - element->lo + 1;
            bits->addChild(enode);
            enode = bits;
          }
        } else if (element->eleType == ELE_INT) {
          enode = new ENode(OP_INT);
          enode->strVal = mpz_get_str(nullptr, 10, element->val);
          enode->width = element->hi + 1;
        } else Panic();
        if (replaceENode) {
          ENode* cat = new ENode(OP_CAT);
          cat->addChild(replaceENode);
          cat->addChild(enode);
          cat->width = replaceENode->width + enode->width;
          replaceENode = cat;
        } else replaceENode = enode;
      }
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
      // for (size_t i = 0; i < componentMap[node]->elements.size(); i ++) {
      //   NodeElement* element = componentMap[node]->elements[i];
      //   printf("=>  %s [%d, %d] (totalWidth = %d)\n", element->eleType == ELE_NODE ? element->node->name.c_str() : (element->eleType == ELE_INT ? (std::string("0x") + mpz_get_str(nullptr, 16, element->val)).c_str() : "EMPTY"),
      //        element->hi, element->lo, componentMap[node]->width);
      //   for (auto iter : element->referNodes) {
      //     printf("    -> %s [%d %d]\n", std::get<0>(iter)->name.c_str(), std::get<1>(iter), std::get<2>(iter));
      //   }
      // }
      Assert(node->width == componentMap[node]->countWidth(), "%s width not match %d != %d", node->name.c_str(), node->width, comp->width);
    }
  }
  /* update refer & update segments for each node */
  std::set<Node*> validNodes;
  for (int i = sortedSuper.size() - 1; i >= 0; i --) {
    for (int j = sortedSuper[i]->member.size() - 1; j >= 0; j --) {
      Node* node = sortedSuper[i]->member[j];
      NodeComponent* comp = componentMap[node];
      bool isValid = false;
      if (node->type != NODE_OTHERS) isValid = true;
      else if (comp->elements.size() == 1 && ((comp->elements[0]->eleType == ELE_NODE && comp->elements[0]->node == node) || comp->elements[0]->eleType == ELE_SPACE)) {
        isValid = true;
      } else if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_INT) {

      } else {
        isValid = validNodes.find(node) != validNodes.end();
      }
      if (isValid) {
        validNodes.insert(node);
        for (NodeElement* element : comp->elements) {
          if (element->eleType == ELE_SPACE) {
            for (auto iter : element->referNodes) {
              Node* referNode = referNode(iter);
              if (node == referNode) continue;
              validNodes.insert(referNode);
              int hi = referHi(iter), lo = referLo(iter);
              nodeSegments[referNode].first->addRange(hi, lo, referLevel(iter));
            }
          } else if (element->eleType == ELE_NODE) {
            Node* referNode = element->node;
            if (node == referNode) continue;
            validNodes.insert(referNode);
            nodeSegments[referNode].first->addRange(element->hi, element->lo, OPL_BITS);
          }
        }
      }
    }
  }
  for (Node* node : validNodes) nodeSegments[node].first->updateCut();

/* spliting registers */
  for (Node* reg : regsrc) {
    if (reg->status != VALID_NODE || reg->isArray() || reg->assignTree.size() > 1 || reg->sign || reg->width == 0) continue;
    // if (nodeSegments[reg].first->cuts.size() > 1 || nodeSegments[reg->getDst()].second->cuts.size() > 1) {
    //   printf("node %s(w = %d): overlap %d size %ld %ld\n", reg->name.c_str(), reg->width, nodeSegments[reg].first->overlap, nodeSegments[reg].first->cuts.size(), nodeSegments[reg->getDst()].second->cuts.size());
    //   for (int cut : nodeSegments[reg].first->cuts) printf("%d ", cut);
    //   printf("\n-------\n");
    //   for (int cut : nodeSegments[reg->getDst()].second->cuts) printf("%d ", cut);
    //   printf("\n-------\n");
    // }
    // if (nodeSegments[reg].first->cuts.size() <= 1|| nodeSegments[reg].first->overlap || nodeSegments[reg].first->cuts != nodeSegments[reg->getDst()].second->cuts) continue;
    // if (nodeSegments[reg].first->cuts.size() <= 1|| nodeSegments[reg].first->overlap) continue;
    Segments* seg;
    if (includes(nodeSegments[reg].first->cuts.begin(), nodeSegments[reg].first->cuts.end(), nodeSegments[reg->getDst()].second->cuts.begin(), nodeSegments[reg->getDst()].second->cuts.end())) {
      seg = nodeSegments[reg->getDst()].second;
    } else if (includes(nodeSegments[reg->getDst()].second->cuts.begin(), nodeSegments[reg->getDst()].second->cuts.end(), nodeSegments[reg].first->cuts.begin(), nodeSegments[reg].first->cuts.end())) {
      seg = nodeSegments[reg].first;
    } else {
      continue;
    }
    if (seg->cuts.size() <= 1) continue;
    printf("splitReg %s\n", reg->name.c_str());
    createSplittedNode(reg, seg);
    if (supersrc.find(reg->super) != supersrc.end()) {
      supersrc.erase(reg->super);
    }
    for (Node* splittedNode : splittedNodesSet[reg]) regsrc.push_back(splittedNode);
    reg->status = DEAD_NODE;
    reg->getDst()->status = DEAD_NODE;

    for (Node* next : reg->next) addReInfer(next);
    for (Node* node : splittedNodesSet[reg]) addReInfer(node);
    for (Node* node : splittedNodesSet[reg->getDst()]) addReInfer(node);
  }
  reInferAll();
  std::set<Node*> checkNodes(validNodes);
  while (!checkNodes.empty()) {
    /* split common nodes */
    for (Node* node : checkNodes) {
      // printf("node %s(w = %d, type %d): overlap %d size %ld %ld\n", node->name.c_str(), node->width, node->type, nodeSegments[node].first->overlap, nodeSegments[node].first->cuts.size(), nodeSegments[node].second->cuts.size());
      // for (int cut : nodeSegments[node].first->cuts) printf("%d ", cut);
      // printf("\n-------\n");
      // for (int cut : nodeSegments[node].second->cuts) printf("%d ", cut);
      // printf("\n-------\n");
      if (node->type != NODE_OTHERS) continue;
      Segments* seg;
      if (includes(nodeSegments[node].first->cuts.begin(), nodeSegments[node].first->cuts.end(), nodeSegments[node].second->cuts.begin(), nodeSegments[node].second->cuts.end())) {
        seg = nodeSegments[node].second;
      } else if (includes(nodeSegments[node].second->cuts.begin(), nodeSegments[node].second->cuts.end(), nodeSegments[node].first->cuts.begin(), nodeSegments[node].first->cuts.end())) {
        seg = nodeSegments[node].first;
      } else {
        continue;
      }
      if (seg->cuts.size() <= 1) continue;
      printf("splitNode %s %p\n", node->name.c_str(), node);
      createSplittedNode(node, seg);
      componentMap[node]->display();
      for (Node* n : splittedNodesSet[node]) {
        n->display();
        addReInfer(n);
      }
      node->status = DEAD_NODE;
      for (Node* next : node->next) addReInfer(next);
    }
    checkNodes.clear();
    reInferAll(true, checkNodes);
  }

/* updating componentMap */
  for (auto iter : componentMap) {
    Node* node = iter.first;
    NodeComponent* comp = iter.second;
    bool missingReferred = false;
    if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_NODE && node == comp->elements[0]->node) missingReferred = true;
    else missingReferred = !comp->fullValid();
    if (missingReferred) continue;
    for (size_t i = 0; i < comp->elements.size(); i ++) {
      NodeElement* element = comp->elements[i];
      if (element->eleType != ELE_NODE) continue;
      if (splittedNodes.find(element->node) != splittedNodes.end()) {
        std::vector<NodeElement*> replaceElements;
        int width = element->hi - element->lo + 1;
        int start = element->hi;
        while (width != 0) {
          Node* referNode = splittedNodes[element->node][start].first;
          int elementHi = splittedNodes[element->node][start].second;
          int elementLo = MAX(0, splittedNodes[element->node][start].second - width + 1);
          replaceElements.push_back(new NodeElement(ELE_NODE, referNode, elementHi, elementLo));
          start -= elementHi - elementLo + 1;
          width -= elementHi - elementLo + 1;
        }
        if (replaceElements.size() == 1) {
          comp->elements[i] = replaceElements[0];
        } else {
          comp->elements.erase(comp->elements.begin() + i);
          comp->elements.insert(comp->elements.begin() + i, replaceElements.begin(), replaceElements.end());
        }
        i += replaceElements.size() - 1;
      }
    }
  }
  /* update assignTree*/
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->width == 0) continue;
      for (ExpTree* tree : member->assignTree) tree->updateWithSplittedNode();
      for (ExpTree* tree : member->arrayVal) tree->updateWithSplittedNode();
    }
  }
  for (auto iter : splittedNodesSet) {
    for (Node* node : iter.second) {
      for (ExpTree* tree : node->assignTree) tree->updateWithSplittedNode();
      for (ExpTree* tree : node->arrayVal) tree->updateWithSplittedNode();
    }
  }
  /* update node connection */
  for (auto iter : componentMap) {
    Node* node = iter.first;
    NodeComponent* comp = iter.second;
    bool missingReferred = false;
    if (comp->elements.size() == 1 && comp->elements[0]->eleType == ELE_NODE && node == comp->elements[0]->node) missingReferred = true;
    else missingReferred = !comp->fullValid();
    if (missingReferred) continue;
    // if (comp->elements.size() != 1) continue;
    ENode* newRoot = nullptr;
    int hiBit = node->width - 1;
    for (size_t i = 0; i < comp->elements.size(); i ++) {
      NodeElement* element = comp->elements[i];
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
        ENode* cat = new ENode(OP_OR);
        cat->addChild(newRoot);
        cat->addChild(enode);
        cat->width = MAX(newRoot->width, enode->width);
        newRoot = cat;
      } else newRoot = enode;
      hiBit -= comp->elements[i]->hi - comp->elements[i]->lo + 1;

    }
    node->assignTree.clear();
    node->assignTree.push_back(new ExpTree(newRoot, new ENode(node)));
    num ++;
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
    std::remove_if(regsrc.begin(), regsrc.end(), [](const Node* n){ return n->status == DEAD_NODE; }),
        regsrc.end()
  );
  removeNodesNoConnect(DEAD_NODE);
/* update connection */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  reconnectAll();

  printf("[splitNode] update %d nodes (total %ld)\n", num, countNodes());
  printf("[splitNode] split %ld registers (total %ld)\n", splittedNodes.size(), regsrc.size());
}