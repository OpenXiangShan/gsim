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
    case OP_EQ:
      ret = new clockVal("0");
      printf("Warning: A clock signal driven by the == operator is detected. "
             "It is not supported now and treated as a constant clock signal. "
             "This may cause wrong result during simulation.\n");
      display();
      break;
    default:
      Assert(0, "invalid op %d", opType);
  }
  return ret;
}

clockVal* Node::clockCompute() {
  if (clockMap.find(this) != clockMap.end()) return clockMap[this];
  if (type == NODE_INP) {
    clockMap[this] = new clockVal(this);
    return clockMap[this];
  }
  Assert(assignTree.size() <= 1, "multiple clock assignment in %s", name.c_str());
  if (assignTree.size() != 0) {
    clockVal* ret = assignTree[0]->getRoot()->clockCompute();
    clockMap[this] = ret;
    return ret;
  }
  printf("status %d type %d\n", status, type);
  display();
  Panic();
}

Node* Node::clockAlias() {
  if (assignTree.size() != 1) return nullptr;
/* check if is when with invalid side */
  if (!assignTree[0]->getRoot()->getNode()) return nullptr;
  Assert(assignTree[0]->getRoot()->getChildNum() == 0, "alias clock %s to array %s\n", name.c_str(), assignTree[0]->getRoot()->getNode()->name.c_str());
  if (prev.size() != 1) return nullptr;
  return assignTree[0]->getRoot()->getNode();
}

void Node::setConstantZero(int w) {
  computeInfo = new valInfo();
  if (w > 0) computeInfo->width = w;
  else computeInfo->width = width;
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
          if (node->clock->type != NODE_INP) {
            ENode* clockENode = new ENode(val->node);
            clockENode->width = val->node->width;
            node->clock->assignTree[0]->setRoot(clockENode);
          }
        } else {
          node->setConstantZero(node->width);
          node->regNext->setConstantZero(node->regNext->width);
        }
      } else if (node->type == NODE_EXT && node->clock) {
        clockVal* val = clockMap[node->clock];
        if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->clock->assignTree[0]->setRoot(clockENode);
        } else {
          for (Node* member : node->member)
            member->setConstantZero(member->width);
        }
      } else {
        clockVal* val = nullptr;
        Node* clockMember = nullptr;
        if (node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER) {
          Assert(clockMap.find(node->clock) != clockMap.end(), "%s: clock %s not found", node->name.c_str(), node->clock->name.c_str());
          val = clockMap[node->clock];
          clockMember = node->clock;
        }
        if (val) {
          if (val->node) {
            ENode* clockENode = new ENode(val->node);
            clockENode->width = val->node->width;
            clockMember->assignTree[0]->setRoot(clockENode);
          } else {
            for (Node* member : node->member)
              member->setConstantZero(member->width);
          }
        }
      }
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* node : super->member) {
      if (!node->isClock) continue;
      for (Node* prev : node->prev) prev->next.erase(node);
      for (Node* next : node->next) next->prev.erase(node);
    }
  }
  input.erase(
    std::remove_if(input.begin(), input.end(), [](const Node* n){ return n->isClock; }),
      input.end()
  );
  /* remove all clock nodes */
  for (SuperNode* super : sortedSuper) {
    super->member.erase(
      std::remove_if(super->member.begin(), super->member.end(), [](const Node* n){ return n->isClock; }),
      super->member.end()
    );
  }
  /* remove empty superNodes */
  sortedSuper.erase(
    std::remove_if(sortedSuper.begin(), sortedSuper.end(), [](const SuperNode* sn) { return sn->member.size() == 0; }),
    sortedSuper.end()
  );

  reconnectSuper();
}