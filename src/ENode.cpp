#include "common.h"
#include <stack>
#include <tuple>
#include <utility>

int ENode::counter = 1;

#define Child(id, name) getChild(id)->name

void ENode::usedBitWithFixRoot(int rootWidth) {
  if (nodePtr) {
    return;
  }
  std::vector<int>childBits;
  if (child.size() == 0) return;
  switch (opType) {
    case OP_ADD: case OP_SUB: case OP_OR: case OP_XOR: case OP_AND:
      childBits.push_back(rootWidth);
      childBits.push_back(rootWidth);
      break;
    case OP_MUL:
      childBits.push_back(MIN(rootWidth, Child(0, width)));
      childBits.push_back(MIN(rootWidth, Child(1, width)));
      break;
    case OP_DIV: case OP_REM: case OP_DSHL: case OP_DSHR:
    case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_EQ: case OP_NEQ:
      childBits.push_back(Child(0, width));
      childBits.push_back(Child(1, width));
      break;
    case OP_CAT:
      childBits.push_back(MAX(rootWidth - Child(1, width), 0));
      childBits.push_back(MIN(Child(1, width), rootWidth));
      break;
    case OP_CVT:
      childBits.push_back(Child(0, sign) ? rootWidth : rootWidth - 1);
      break;
    case OP_ASCLOCK: case OP_ASASYNCRESET: case OP_ANDR:
    case OP_ORR: case OP_XORR: case OP_INDEX_INT: case OP_INDEX:
      childBits.push_back(Child(0, width));
      break;
    case OP_ASUINT: case OP_ASSINT: case OP_NOT: case OP_NEG: case OP_PAD: case OP_TAIL:
      childBits.push_back(rootWidth);
      break;
    case OP_SHL:
      childBits.push_back(MAX(0, rootWidth - values[0]));
      break;
    case OP_SHR:
      childBits.push_back(rootWidth + values[0]);
      break;
    case OP_HEAD:
      childBits.push_back(rootWidth + values[0]);
      break;
    case OP_BITS:
      childBits.push_back(MIN(rootWidth + values[1], values[0] + 1));
      break;
    case OP_BITS_NOSHIFT:
      childBits.push_back(rootWidth);
      break;
    case OP_SEXT:
      childBits.push_back(rootWidth);
      break;
    case OP_MUX:
    case OP_WHEN:
      childBits.push_back(1);
      childBits.push_back(rootWidth);
      childBits.push_back(rootWidth);
      break;
    case OP_STMT:
      for (size_t i = 0; i < getChildNum(); i ++) childBits.push_back(rootWidth);
      break;
    case OP_READ_MEM:
      childBits.push_back(Child(0, width));
      break;
    case OP_RESET:
      childBits.push_back(1);
      childBits.push_back(rootWidth);
      break;
    case OP_EXIT:
      childBits.push_back(1);
      break;
    case OP_PRINTF:
    case OP_ASSERT:
      for (size_t i = 0; i < getChildNum(); i ++) {
        childBits.push_back(Child(i, width));
      }
      break;
    default:
      printf("invalid op %d\n", opType);
      Panic();
   }

  Assert(child.size() == childBits.size(), "child.size %ld childBits.size %ld in op %d", child.size(), childBits.size(), opType);
  for (size_t i = 0; i < child.size(); i ++) {
    if (!child[i]) continue;
    child[i]->usedBitWithFixRoot(childBits[i]);
  }
  width = MIN(width, rootWidth);
  if (opType == OP_BITS) {
    values[0] = MIN(values[0], width - 1 + values[1]);
  }
}

void ENode::updateWidth() {
  for (ENode* childENode : child) {
    if (childENode) childENode->updateWidth();
  }
  width = usedBit;
}
/* add extention/bits for nodePtr when enode->width != nodePtr->width*/
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
          bits->addVal(top->width - 1);
          bits->addVal(0);
          bits->addChild(top);
          top->width = top->nodePtr->width;
          newChild = bits;
          if (top->sign) {
            ENode* asSint = new ENode(OP_ASSINT);
            asSint->width = bits->width;
            asSint->sign = true;
            asSint->addChild(bits);
            newChild = asSint;
          }
          remove = true;
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
          if (top->width == top->getChild(0)->width && top->sign == top->getChild(0)->sign) {
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
          break;
        case OP_EQ: case OP_NEQ: case OP_LT: case OP_LEQ: case OP_GT: case OP_GEQ: case OP_ADD: case OP_SUB: {
          int maxWidth = MAX(top->getChild(0)->width, top->getChild(1)->width);
          if (maxWidth != top->getChild(0)->width) {
            ENode* newENode = nullptr;
            if (top->getChild(0)->sign) {
              newENode = new ENode(OP_SEXT);
              newENode->width = maxWidth;
              newENode->sign = true;
              newENode->addVal(maxWidth);
            } else {
              newENode = new ENode(OP_PAD);
              newENode->width = maxWidth;
              newENode->addVal(maxWidth);
            }
            newENode->addChild(top->getChild(0));
            top->setChild(0, newENode);
          }
          if (maxWidth != top->getChild(1)->width) {
            ENode* newENode = nullptr;
            if (top->getChild(1)->sign) {
              newENode = new ENode(OP_SEXT);
              newENode->width = maxWidth;
              newENode->sign = true;
              newENode->addVal(maxWidth);
            } else {
              newENode = new ENode(OP_PAD);
              newENode->width = maxWidth;
              newENode->addVal(maxWidth);
            }
            newENode->addChild(top->getChild(1));
            top->setChild(1, newENode);
          }
          break;
        }
        case OP_OR: case OP_XOR:
          if (top->getChild(0)->width < top->width) {
            ENode* pad = new ENode(OP_PAD);
            pad->width = top->width;
            pad->sign = top->getChild(0)->sign;
            pad->addVal(top->width);
            pad->addChild(top->getChild(0));
            top->setChild(0, pad);
          }
          if (top->getChild(1)->width < top->width) {
            ENode* pad = new ENode(OP_PAD);
            pad->width = top->width;
            pad->sign = top->getChild(1)->sign;
            pad->addVal(top->width);
            pad->addChild(top->getChild(1));
            top->setChild(1, pad);
          }
          break;
        case OP_MUX:
          if (top->getChild(1)->width > top->getChild(2)->width) {
            ENode* pad = new ENode(OP_PAD);
            pad->width = top->width;
            pad->sign = top->getChild(1)->sign;
            pad->addVal(top->width);
            pad->addChild(top->getChild(1));
            top->setChild(1, pad);
          } else if (top->getChild(1)->width < top->getChild(2)->width) {
            ENode* pad = new ENode(OP_PAD);
            pad->width = top->width;
            pad->sign = top->getChild(2)->sign;
            pad->addVal(top->width);
            pad->addChild(top->getChild(2));
            top->setChild(2, pad);
          }
        default:
          break;
      }
    }
    if (remove) {
      if (!parent) setRoot(newChild);
      else {
        for (size_t i = 0; i < parent->getChildNum(); i ++) {
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
  ret->memoryNode = memoryNode;
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
