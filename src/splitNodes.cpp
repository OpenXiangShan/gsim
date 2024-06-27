#include <gmp.h>
#include <cstdio>
#include <stack>
#include <utility>
#include "common.h"

class NodeElement;
bool compMergable(NodeElement* ele1, NodeElement* ele2);

class NodeElement {
public:
  bool isNode = false;
  Node* node = nullptr;
  mpz_t val;
  int hi, lo;
  NodeElement(bool _isNode = false, Node* _node = nullptr, int _hi = -1, int _lo = -1) {
    mpz_init(val);
    isNode = _isNode;
    node = _node;
    hi = _hi;
    lo = _lo;
  }
  NodeElement(std::string str, int base = 16, int _hi = -1, int _lo = -1) {
    mpz_init(val);
    mpz_set_str(val, str.c_str(), base);
    hi = _hi;
    lo = _lo;
    isNode = false;
    updateWidth();
  }
  void updateWidth() {
    if (lo != 0) mpz_tdiv_q_2exp(val, val, lo);
    mpz_t mask;
    mpz_init(mask);
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, hi - lo + 1);
    mpz_sub_ui(mask, mask, 1);
    mpz_and(val, val, mask);
    hi = hi - lo;
    lo = 0;
  }
  NodeElement* dup() {
    NodeElement* ret = new NodeElement();
    mpz_set(ret->val, val);
    ret->isNode = isNode;
    ret->node = node;
    ret->hi = hi;
    ret->lo = lo;
    return ret;
  }
  /* select [_hi, lo] */
  NodeElement* getBits(int _hi, int _lo) {
    Assert(_hi <= hi - lo + 1, "invalid range [%d, %d]", _hi, lo);
    NodeElement* ret = dup();
    if (isNode) {
      ret->hi = _hi + ret->lo;
      ret->lo = _lo + ret->lo;
    } else {
      ret->hi = _hi;
      ret->lo = _lo;
      ret->updateWidth();
    }
    return ret;
  }
  void merge(NodeElement* ele) {
    Assert(compMergable(this, ele), "not mergable");
    if (isNode) {
      lo = ele->lo;
    } else {
      mpz_mul_2exp(val, val, ele->hi - lo + 1);
      mpz_add(val, val, ele->val);
      hi += ele->hi + 1;
      updateWidth();
    }
  }
};

class NodeComponent{
public:
  std::vector<NodeElement*> components;
  // std::vector<std::pair<int, int>> bits;
  int width;
  NodeComponent() {
    width = 0;
  }
  void addElement(NodeElement* element) {
    components.push_back(element);
    width += element->hi - element->lo + 1;
  }
  void merge(NodeComponent* node) {
    if (components.size() != 0 && node->components.size() != 0 && compMergable(components.back(), node->components[0])) {
      components.back()->merge(node->components[0]);
      components.insert(components.end(), node->components.begin() + 1, node->components.end());
    } else {
      components.insert(components.end(), node->components.begin(), node->components.end());
    }
    width += node->countWidth();
    countWidth();
  }
  NodeComponent* getbits(int hi, int lo) {
    int w = width;
    bool start = false;
    NodeComponent* comp = new NodeComponent();
    // comp->width = hi - lo + 1;
    if (hi >= width) {
      comp->addElement(new NodeElement("0", 16, hi - MAX(lo, width), 0));
      hi = MAX(lo, width - 1);
    }
    if (hi < width) {
      for (size_t i = 0; i < components.size(); i ++) {
        int memberWidth = components[i]->hi - components[i]->lo + 1;
        if ((w > hi && (w - memberWidth) <= hi)) {
          start = true;
        }
        if (start) {
          int selectHigh = MIN(hi, w-1) - (w - memberWidth);
          int selectLo = MAX(lo, w - memberWidth) - (w - memberWidth);
          comp->addElement(components[i]->getBits(selectHigh, selectLo));
          if ((w - memberWidth) <= lo) break;
        }
        w = w - memberWidth;
      }
    }
    countWidth();
    return comp;
  }
  int countWidth() {
    int w = 0;
    for (NodeElement* comp : components) w += comp->hi - comp->lo + 1;
    if (w != width) {
      for (size_t i = 0; i < components.size(); i ++) {
        printf("  %s [%d, %d] (totalWidth = %d)\n", components[i]->isNode ? components[i]->node->name.c_str() : (std::string("0x") + mpz_get_str(nullptr, 16, components[i]->val)).c_str(),
             components[i]->hi, components[i]->lo, width);
      }
    }
    Assert(w == width, "width not match %d != %d\n", w, width);
    return width;
  }
};

bool compMergable(NodeElement* ele1, NodeElement* ele2) {
  if (ele1->isNode != ele2->isNode) return false;
  if (ele1->isNode) return ele1->node == ele2->node && ele1->lo == ele2->hi + 1;
  else return true;
}

NodeComponent* merge2(NodeComponent* comp1, NodeComponent* comp2) {
  NodeComponent* ret = new NodeComponent();
  ret->merge(comp1);
  ret->merge(comp2);
  return ret;
}

std::map <Node*, NodeComponent*> componentMap;
std::map <ENode*, NodeComponent*> componentEMap;
static std::vector<Node*> checkNodes;
static std::map<Node*, Node*> aliasMap;

Node* getLeafNode(bool isArray, ENode* enode);

NodeComponent* ENode::inferComponent() {
  if (nodePtr) {
    Node* node = getLeafNode(true, this);
    if (node->isArray()) return nullptr;
    Assert(componentMap.find(node) != componentMap.end(), "%s is not visited", node->name.c_str());
    NodeComponent* ret = componentMap[node]->getbits(width-1, 0);
    if (ret) componentEMap[this] = ret;
    return ret;
  }
  NodeComponent* ret = nullptr;
  NodeComponent* child1, *child2;
  switch (opType) {
    case OP_BITS:
      child1 = getChild(0)->inferComponent();
      if (child1) ret = child1->getbits(values[0], values[1]);
      break;
    case OP_CAT: 
      child1 = getChild(0)->inferComponent();
      child2 = getChild(1)->inferComponent();
      if (child1 && child2) {
        ret = merge2(child1, child2);
      }
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
      child1 = getChild(0)->inferComponent();
      if (child1) ret = child1->getbits(child1->width - 1, values[0]);
      break;
    case OP_TAIL:
      child1 = getChild(0)->inferComponent();
      if (child1) ret = child1->getbits(values[0]-1, 0);
      break;
    default:
      break;
  }
  if (ret) componentEMap[this] = ret;
  return ret;
}

NodeComponent* Node::inferComponent() {
  Assert(assignTree.size() == 1, "invalid assignTree %s", name.c_str());
  return assignTree[0]->getRoot()->inferComponent();
}

void graph::splitNodes() {
  int num = 0;
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->isArray() || node->assignTree.size() != 1 || node->sign) {
        componentMap[node] = new NodeComponent();
        componentMap[node]->addElement(new NodeElement(true, node, node->width - 1, 0));
        continue;
      }
      NodeComponent* comp = node->inferComponent();
      if (comp) componentMap[node] = comp;
      else {
        componentMap[node] = new NodeComponent();
        componentMap[node]->addElement(new NodeElement(true, node, node->width - 1, 0));
      }
      // printf("after infer %s\n", node->name.c_str());
      // node->display();
      // for (size_t i = 0; i < componentMap[node]->components.size(); i ++) {
      //   NodeElement* element = componentMap[node]->components[i];
      //   printf("=>  %s [%d, %d] (totalWidth = %d)\n", element->isNode ? element->node->name.c_str() : (std::string("0x") + mpz_get_str(nullptr, 16, element->val)).c_str(),
      //        element->hi, element->lo, componentMap[node]->width);
      // }
      Assert(node->width == componentMap[node]->countWidth(), "%s width not match %d != %d", node->name.c_str(), node->width, comp->width);
    }
  }
  for (auto iter : componentMap) {
    Node* node = iter.first;
    NodeComponent* comp = iter.second;
    if (comp->components.size() == 1 && comp->components[0]->isNode && node == comp->components[0]->node) continue;
    // if (comp->components.size() != 1) continue;
    ENode* newRoot = nullptr;
    int hiBit = node->width - 1;
    for (size_t i = 0; i < comp->components.size(); i ++) {
      NodeElement* element = comp->components[i];
      ENode* enode;
      if (element->isNode) {
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
      hiBit -= comp->components[i]->hi - comp->components[i]->lo + 1;

    }
    node->assignTree.clear();
    node->assignTree.push_back(new ExpTree(newRoot, new ENode(node)));
    num ++;
  }

/* update connection */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  reconnectAll();

  printf("[splitNode] update %d nodes (total %ld)\n", num, countNodes());
}