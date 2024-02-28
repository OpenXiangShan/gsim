#include "common.h"
#include <map>

class clockVal {
public:
  Node* node = nullptr;
  mpz_t val;
  bool isInvalid = false;
  clockVal(Node* n) {
    node = n;
  }
  clockVal(mpz_t& mpz_val) {
    mpz_init(val);
    mpz_set(val, mpz_val);
  }
  clockVal(std::string s) {
    mpz_init(val);
    mpz_set_str(val, s.c_str(), 16);
  }
  clockVal() {
    isInvalid = true;
  }
};

std::map<Node*, clockVal*> clockMap;

clockVal* ENode::clockCompute() {
  if (width == 0) return new clockVal("0");
  if (nodePtr) {
    if (clockMap.find(nodePtr) != clockMap.end()) return clockMap[nodePtr];
    else return nodePtr->clockCompute();
  }

  clockVal* childVal = nullptr;
  clockVal* ret = nullptr;
  switch (opType) {
    case OP_WHEN:
      childVal = getChild(1)->clockCompute();
      ret = getChild(2)->clockCompute();
      Assert(childVal->isInvalid || ret->isInvalid, "invalid clock");
      if (ret->isInvalid) ret = childVal;
      break;
    case OP_ASUINT:
    case OP_ASSINT:
    case OP_ASCLOCK:
    case OP_ASASYNCRESET:
      ret = getChild(0)->clockCompute();
      break;
    case OP_BITS:
      Assert(values[0] == 0 && values[1] == 0, "bits %d %d", values[0], values[1]);
      ret = getChild(0)->clockCompute();
      break;
    case OP_INVALID:
      ret = new clockVal();
      ret->isInvalid = true;
      break;
    case OP_INT:
      ret = new clockVal("0");
      break;
    default:
      Assert(0, "invalid op %d", opType);
  }
  return ret;
}

clockVal* Node::clockCompute() {
  if (clockMap.find(this) != clockMap.end()) return clockMap[this];
  if (valTree) {
    clockVal* ret = valTree->getRoot()->clockCompute();
    clockMap[this] = ret;
    return ret;
  }
  Panic();
}

Node* Node::clockAlias() {
  if (!valTree) return nullptr;
/* check if is when with invalid side */
  if (!valTree->getRoot()->getNode()) return nullptr;
  Assert(valTree->getRoot()->getChildNum() == 0, "alias clock %s to array %s\n", name.c_str(), valTree->getRoot()->getNode()->name.c_str());
  if (prev.size() != 1) return nullptr;
  return valTree->getRoot()->getNode();
}

void Node::setConstantZero(int width) {
  computeInfo = new valInfo();
  computeInfo->width = width;
  computeInfo->setConstantByStr("0");
  status = CONSTANT_NODE;
}

/* clock alias & clock constant analysis */
void graph::clockOptimize() {
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (!node->isClock) continue;
      Assert(!node->isArray(), "clock %s is array", node->name.c_str());
      if (node->type == NODE_INP) clockMap[node] = new clockVal(node);
      else {
        node->clockCompute();
      }
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (node->type == NODE_REG_DST) {
        Assert(clockMap.find(node->clock) != clockMap.end(), "%s: clock %s(%p) not found", node->name.c_str(), node->clock->name.c_str(), node->clock);
        clockVal* val = clockMap[node->clock];
        if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->clock->valTree->setRoot(clockENode);
        } else {
          node->setConstantZero(node->width);
          node->regNext->setConstantZero(node->regNext->width);
        }
      } else if (node->type == NODE_READER) {
        Assert(clockMap.find(node->get_member(READER_CLK)) != clockMap.end(), "%s: clock %s not found", node->name.c_str(), node->get_member(READER_CLK)->name.c_str());
        clockVal* val = clockMap[node->get_member(READER_CLK)];
        if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->get_member(READER_CLK)->valTree->setRoot(clockENode);
        } else {
          for (Node* member : node->member)
            member->setConstantZero(member->width);
        }
      } else if (node->type ==  NODE_WRITER) {
        Assert(clockMap.find(node->get_member(WRITER_CLK)) != clockMap.end(), "%s: clock %s not found", node->name.c_str(), node->get_member(WRITER_CLK)->name.c_str());
        clockVal* val = clockMap[node->get_member(WRITER_CLK)];
        if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->get_member(WRITER_CLK)->valTree->setRoot(clockENode);
        } else {
          for (Node* member : node->member)
            member->setConstantZero(member->width);
        }
      } else if (node->type == NODE_READWRITER) {
        Assert(clockMap.find(node->get_member(READWRITER_CLK)) != clockMap.end(), "%s: clock %s not found", node->name.c_str(), node->get_member(READWRITER_CLK)->name.c_str());
        clockVal* val = clockMap[node->get_member(READWRITER_CLK)];
        if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->get_member(READWRITER_CLK)->valTree->setRoot(clockENode);
        } else {
          for (Node* member : node->member)
            member->setConstantZero(member->width);
        }
      }
    }
  }
}