#include "common.h"
#include <stack>
#include <tuple>
#include <utility>

int ENode::counter = 1;

#define w0 getChild(0)->width
#define w1 getChild(1)->width
#define w2 getChild(2)->width
#define s0 getChild(0)->sign
#define s1 getChild(1)->sign
#define s2 getChild(2)->sign

void ENode::inferWidth() {
  for (ENode* enode : child) {
    if (enode) enode->inferWidth();
  }
  if (width != -1) return;
  if (nodePtr) {
    Node* realNode = nodePtr;
    if(nodePtr->isArray() && nodePtr->arraySplitted()) {
      ArrayMemberList* list = getArrayMember(nodePtr);
      realNode = list->member[0];
    }
    realNode->inferWidth();
    setWidth(realNode->width, realNode->sign);
    isClock = realNode->isClock;
  } else {
    isClock = false;
    switch (opType) {
      case OP_ADD:  case OP_SUB:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MAX(w0, w1) + 1, s0);
        break;
      case OP_MUL:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + w1, s0);
        break;
      case OP_CAT:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + w1, false);
        break;
      case OP_DIV:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(w0 + s0, s0);
        break;
      case OP_REM:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MIN(w0, w1), s0);
        break;
      case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(1, false);
        break;
      case OP_DSHL:
        Assert(getChildNum() == 2 && !s1, "invalid child");
        setWidth(w0 + (1 << w1) - 1, s0);
        break;
      case OP_DSHR:
        Assert(getChildNum() == 2, "invalid child");
        setWidth(w0, s0);
        break;
      case OP_AND: // the width is actually max, while the upper bits are zeros
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        if (w0 != w1) {
          if (s1) {
            int w = MAX(w0, w1);
            ENode* enode;
            enode = new ENode(OP_SEXT);
            enode->addVal(w);
            enode->width = w;
            enode->sign = s1;
            if (w0 > w1) {
              enode->addChild(getChild(1));
              setChild(1, enode);
            } else {
              enode->addChild(getChild(0));
              setChild(0, enode);
            }
          } else {
            if (getChild(0)->nodePtr && getChild(1)->nodePtr) {
              int w = MIN(w0, w1);
              ENode* enode = new ENode(OP_BITS);
              enode->addVal(w-1);
              enode->addVal(0);
              enode->width = w;
              if (w0 < w1) {
                enode->addChild(getChild(1));
                setChild(1, enode);
              } else {
                enode->addChild(getChild(0));
                setChild(0, enode);
              }
            } else {
              int w = MAX(w0, w1);
              ENode* enode = new ENode(OP_PAD);
              enode->addVal(w);
              enode->width = w;
              if (w0 > w1) {
                enode->addChild(getChild(1));
                setChild(1, enode);
              } else {
                enode->addChild(getChild(0));
                setChild(0, enode);
              }
            }
          }
        }
        setWidth(MAX(w0, w1), false);
        break;
      case OP_OR: case OP_XOR:
        Assert(getChildNum() == 2 && s0 == s1, "invalid child");
        setWidth(MAX(w0, w1), false);
        break;
      case OP_ASUINT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, false);
        break;
      case OP_ASSINT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, true);
        break;
      case OP_ASCLOCK:
        isClock = true;
      case OP_ASASYNCRESET:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(1, false);
        break;
      case OP_CVT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0 + !s0, true);
        break;
      case OP_NEG:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0 + 1, true);
        break;
      case OP_NOT:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(w0, false);
        break;
      case OP_ANDR:
      case OP_ORR:
      case OP_XORR:
        Assert(getChildNum() == 1, "invalid child");
        setWidth(1, false);
        break;
      case OP_PAD:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(MAX(w0, values[0]), s0);
        break;
      case OP_SHL:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(w0 + values[0], s0);
        break;
      case OP_SHR:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(MAX(w0-values[0], 0), s0);
        break;
      case OP_HEAD:
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(values[0], false);
        values[0] = w0 - values[0];
        break;
      case OP_TAIL:
        Assert(w0 > values[0], "the width of argument(%d) in tail is no greater than %d", w0, values[0]);
        Assert(getChildNum() == 1 && values.size() == 1, "invalid child");
        setWidth(w0-values[0], false);
        values[0] = w0 - values[0];
        break;
      case OP_BITS: // values[0] may exceeds w0 due to the shrink width of add
        Assert(getChildNum() == 1 && values.size() == 2, "invalid child");
        // Assert(w0 > values[0] && values[0] >= values[1], "invalid bits(%d, %d) with argument(%d) ", values[0], values[1], w0);
        setWidth(values[0]-values[1] + 1, false);
        break;
      case OP_BITS_NOSHIFT:
        Assert(getChildNum() == 1 && values.size() == 2, "invalid child");
        setWidth(values[0] + 1, false);
        break;
      case OP_MUX:
      case OP_WHEN:
        if (getChild(1) && getChild(2)) {
          if (w1 > w2) getChild(2)->setWidth(w1, s2);
          if (w2 > w1) getChild(1)->setWidth(w2, s1);
        }
        Assert(getChildNum() == 3 && (getChild(1) || getChild(2)), "invalid child");
        if (getChild(1) && getChild(2)) setWidth(MAX(w1, w2), s1);
        else if (getChild(1)) setWidth(w1, s1);
        else setWidth(w2, s2);
        break;
      case OP_STMT:
        for (ENode* enode : child) {
          setWidth(MAX(width, enode->width), enode->sign);
        }
        for (ENode* enode : child) {
          if (enode->width != width) enode->setWidth(width, sign);
        }
        break;
      case OP_READ_MEM:
        setWidth(getChild(0)->nodePtr->parent->parent->width, getChild(0)->nodePtr->parent->parent->sign);
        break;
      case OP_INVALID:
        setWidth(0, false);
        break;
      case OP_RESET:
        setWidth(w1, false);
        break;
      case OP_PRINTF:
      case OP_ASSERT:
        setWidth(1, false);
        break;
      case OP_INDEX_INT:
      case OP_INDEX:
        break;
      default:
        Assert(0, "invalid operand %d", opType);
    }
  }
}

void ENode::clearWidth() {
  std::stack<ENode*> s;
  for (ENode* childENode : child) {
    if (childENode) childENode->clearWidth();
  }
  if (!nodePtr && opType != OP_INT) width = -1;
}

void ENode::updateWidth() {
  for (ENode* childENode : child) {
    if (childENode) childENode->updateWidth();
  }
  width = usedBit;
}

void ExpTree::updateWithNewWidth() {
  std::stack<std::pair<ENode*, ENode*>> s;
  s.push(std::make_pair(getRoot(), nullptr));
  while (!s.empty()) {
    ENode* top, *parent;
    std::tie(top, parent) = s.top();
    s.pop();
    bool remove = false;
    ENode* newChild = nullptr;
    if (top->nodePtr) {
      if (top->width < top->nodePtr->width) {
        if (parent && parent->opType == OP_BITS) {
          top->width = top->nodePtr->width;
        } else {
          ENode* bits = new ENode(OP_BITS);
          bits->width = top->width;
          bits->sign = top->sign;
          bits->addVal(top->width - 1);
          bits->addVal(0);
          bits->addChild(top);
          remove = true;
          newChild = bits;
          top->width = top->nodePtr->width;
        }
      } else if (top->width > top->nodePtr->width) {
        if (top->sign) {
          ENode* sext = new ENode(OP_SEXT);
          sext->width = top->width;
          sext->sign = true;
          sext->addVal(top->width);
          sext->addChild(top);
          top->width = top->nodePtr->width;
          remove = true;
          newChild = sext;
        } else {
          ENode* enode = new ENode(OP_PAD);
          enode->addVal(top->width);
          enode->width = top->width;
          enode->addChild(top);
          top->width = top->nodePtr->width;
          remove = true;
          newChild = enode;
        }
      }
    } else {
      switch(top->opType) {
        case OP_BITS:
          if (top->width == top->getChild(0)->width) {
            remove = true;
            newChild = top->getChild(0);
            break;
          } else if (top->width > top->getChild(0)->width) {
            if (top->sign) {
              top->opType = OP_SEXT;
              top->values.clear();
              top->addVal(top->width);
            } else {
              top->opType = OP_PAD;
              top->values.clear();
              top->addVal(top->width);
            }
          } else if ((top->values[0] - top->values[1] + 1) > top->width) top->values[0] = top->values[1] + top->width - 1;
          break;
        case OP_PAD:
          if (top->width == top->getChild(0)->width) {
            remove = true;
            newChild = top->getChild(0);
          } else if (top->values[0] > top->width) top->values[0] = top->width;
          break;
        case OP_SEXT:
          if (top->width == top->getChild(0)->width) {
            remove = true;
            newChild = top->getChild(0);
            break;
          }
          top->values[0] = top->width;
          Assert(top->width >= top->getChild(0)->width, "invalid sext %p width %d childWidth %d", top, top->width, top->getChild(0)->width);
          break;
        case OP_CAT:
          if (top->width == top->getChild(1)->width) {
            remove = true;
            newChild = top->getChild(1);
          }
          Assert(top->width >= top->getChild(1)->width, "invalid cat");
          break;
        case OP_TAIL:
        case OP_HEAD:
          if (top->width == top->getChild(0)->width) {
            remove = true;
            newChild = top->getChild(0);
          }
          break;
        case OP_MUL:
          if (top->width != top->getChild(0)->width + top->getChild(1)->width) {
            remove = true;
            ENode* bits = new ENode(OP_BITS);
            bits->addChild(top);
            bits->addVal(top->width-1);
            bits->addVal(0);
            bits->width = top->width;
            bits->sign = top->sign;
            top->width = top->getChild(0)->width + top->getChild(1)->width;
            newChild = bits;
          }
        default:
          break;
      }
    }
    if (remove) {
      if (!parent) setRoot(newChild);
      else {
        for (int i = 0; i < parent->getChildNum(); i ++) {
          if (parent->getChild(i) == top) parent->setChild(i, newChild);
        }
      }
      s.push(std::make_pair(newChild, parent));
    } else {
      for (ENode* child : top->child) {
        if (child) s.push(std::make_pair(child, top));
      }
    }
  }
}
/*
return the index of lvalue(required to be array)
if the lvalue is also an array(e.g. a[1], a defined as a[2][2]), return the range of index
*/
std::pair<int, int> ENode::getIdx(Node* node) {
  Assert(node->isArray(), "%s is not array", node->name.c_str());
  std::vector<int>index;

  for (ENode* indexENode : child) {
    if (indexENode->opType != OP_INDEX_INT) return std::make_pair(-1, -1);
    index.push_back(indexENode->values[0]);
  }
  Assert(index.size() <= node->dimension.size(), "invalid index");
  int num = 1;
  for (int i = (int)node->dimension.size() - 1; i >= (int)index.size(); i --) {
    num *= node->dimension[i];
  }
  int base = 0;
  for (size_t i = 0; i < index.size(); i ++) {
    base = base * node->dimension[i] + index[i];
  }
  return std::make_pair(base * num, (base + 1) * num - 1);
}

bool ENode::hasVarIdx(Node* node) {
  int beg, end;
  std::tie(beg, end) = getIdx(node);
  if (beg < 0 || beg != end) return true;
  return false;
}
/*
return right value of connected node
* for normal nodes and array nodes that are not splitted, return themselves
* for splitted array, return the corresponding member nodes if it represents a single member
                    or return the array itself if the index is not determined or it represent multiple members
*/
Node* ENode::getConnectNode() {
  /* not array */
  if (!nodePtr || !nodePtr->isArray()) {
    return nodePtr;
  }
  /* un-splitted array */
  if (!nodePtr->arraySplitted()) return nodePtr;
  /* splitted array */
  int begin, end;
  std::tie(begin, end) = getIdx(nodePtr);
  if (begin < 0 || begin != end) return nodePtr;
  return nodePtr->getArrayMember(begin);
}

ENode* ENode::dup() {
  ENode* ret = new ENode(opType);
  ret->setNode(nodePtr);
  ret->sign = sign;
  ret->width = width;
  ret->values.insert(ret->values.end(), values.begin(), values.end());
  ret->strVal = strVal;
  /* TODO : check isClock */
  for (ENode* childNode : child) {
    if (childNode) ret->addChild(childNode->dup());
    else ret->addChild(nullptr);
  }
  return ret;
}

int ENode::getArrayIndex(Node* node) {
  Assert(nodePtr, "empty  node");
  Assert(nodePtr == node, "lvalue not match %s != %s", nodePtr->name.c_str(), node->name.c_str());
  Assert(child.size() <= node->dimension.size(), "%s index out of bound", node->name.c_str());
  int idx = 0;
  size_t fixNum = 0;

  for (ENode* childENode : child) {
    if (childENode->opType == OP_INDEX_INT) {
      idx = idx * (node->dimension[fixNum++]) + childENode->values[0];
    } else {
      TODO();
      return idx;
    }
  }
  if (fixNum < node->dimension.size()) TODO();

  return idx;
}

ArrayMemberList* ENode::getArrayMember(Node* node) {
  Assert(nodePtr, "empty node");
  Assert(nodePtr == node, "lvalue not match %s != %s", nodePtr->name.c_str(), node->name.c_str());
  Assert(child.size() <= node->dimension.size(), "%s index out of bound", node->name.c_str());
  ArrayMemberList* ret = new ArrayMemberList();
  int begin, end;
  std::tie(begin, end) = getIdx(nodePtr);
  if (begin < 0) TODO();
  for (int i = begin; i <= end; i ++) {
    ret->add_member(node->getArrayMember(i), i);
  }
  return ret;
}
