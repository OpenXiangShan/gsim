#include "common.h"
#include <map>

class clockVal {
public:
  Node* node = nullptr;
  int val = 0;
  bool isInvalid = false;
  ENode* gateENode = nullptr; // ClockGate
  clockVal(Node* n) { node = n; }
  clockVal(int v) { val = v; }
  clockVal() { isInvalid = true; }
};

std::map<Node*, clockVal*> clockMap;

clockVal* ENode::clockCompute() {
  if (width == 0) return new clockVal(0);
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
      ret = new clockVal(0);
      break;
    case OP_EQ:
      ret = new clockVal(0);
      printf("Warning: A clock signal driven by the == operator is detected. "
             "It is not supported now and treated as a constant clock signal. "
             "This may cause wrong result during simulation.\n");
      display();
      break;
    case OP_OR:
      ret = new clockVal();
      ret->gateENode = this;
      break;
    case OP_AND:
      childVal = getChild(0)->clockCompute(); // first child
      ret = getChild(1)->clockCompute(); // second child
      if (childVal->node && ret->gateENode) {
        ret->node = childVal->node;
      } else if (childVal->gateENode && ret->node) {
        ret->gateENode = ret->gateENode;
      } else Panic();
      break;
    default:
      Assert(0, "invalid op %d", opType);
  }
  return ret;
}

clockVal* Node::clockCompute() {
  if (clockMap.find(this) != clockMap.end()) return clockMap[this];
  if (type == NODE_INP || type == NODE_REG_SRC) {
    clockMap[this] = new clockVal(this);
    return clockMap[this];
  }
  Assert(assignTree.size() <= 1, "multiple clock assignment in %s", name.c_str());
  if (assignTree.size() != 0) {
    clockMap[this] = assignTree[0]->getRoot()->clockCompute();
  } else {
    clockMap[this] = new clockVal(0);
    printf("Warning: An external clock signal is detected. "
           "It is not supported now and treated as a constant clock signal. "
           "This may cause wrong result during simulation.\n");
    display();
  }
  return clockMap[this];
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
  ENode* enode = new ENode(OP_INT);
  enode->width = w;
  enode->strVal = "0";
  assignTree.clear();
  arrayVal.clear();
  assignTree.push_back(new ExpTree(enode, this));
}
void Node::setConstantInfoZero(int w) {
  computeInfo = new valInfo();
  if (w > 0) computeInfo->width = w;
  else computeInfo->width = width;
  computeInfo->setConstantByStr("0");
  status = CONSTANT_NODE;
}

/* clock alias & clock constant analysis */
void graph::clockOptimize(std::map<std::string, Node*>& allSignals) {
  for (auto iter : allSignals) {
    Node* node = iter.second;
    if (!node->isClock) continue;
    Assert(!node->isArray(), "clock %s is array", node->name.c_str());
    if (node->type == NODE_INP) clockMap[node] = new clockVal(node);
    else node->clockCompute();
  }

  for (auto iter : allSignals) {
    Node* node = iter.second;
    if (node->type == NODE_REG_DST) {
      Assert(clockMap.find(node->clock) != clockMap.end(), "%s: clock %s(%p) not found", node->name.c_str(), node->clock->name.c_str(), node->clock);
      clockVal* val = clockMap[node->clock];
      Assert(!val->gateENode || val->node, "invalid clock %s %d", node->clock->name.c_str(), node->clock->lineno);
      if (val->gateENode) {
        ENode* gate = new ENode(OP_WHEN);
        gate->width = node->width;
        gate->sign = node->sign;
        ENode* cond = val->gateENode->dup();
        gate->addChild(cond);
        gate->addChild(nullptr);
        gate->addChild(nullptr);
        for (ExpTree* tree : node->assignTree) {
          ENode* gateDup = gate->dup();
          gateDup->setChild(1, tree->getRoot());
          tree->setRoot(gateDup);
        }
        for (ExpTree* tree : node->arrayVal) {
          ENode* gateDup = gate->dup();
          gateDup->setChild(1, tree->getRoot());
          tree->setRoot(gateDup);
        }
      } else if (val->node) {
        if (node->clock->type != NODE_INP) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          node->clock->assignTree[0]->setRoot(clockENode);
        }
      } else {
        node->setConstantZero(0);
        node->regNext->setConstantZero(0);
      }
    } else if (node->type == NODE_EXT && node->clock) {
      clockVal* val = clockMap[node->clock];
      if (val->node) {
        ENode* clockENode = new ENode(val->node);
        clockENode->width = val->node->width;
        node->clock->assignTree[0]->setRoot(clockENode);
      } else {
        for (Node* member : node->member)
          member->setConstantZero(0);
      }
    } else if (node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER) {
      clockVal* val = nullptr;
      Node* clockMember = nullptr;
      if (node->type == NODE_READER || node->type == NODE_WRITER || node->type == NODE_READWRITER) {
        Assert(clockMap.find(node->clock) != clockMap.end(), "%s: clock %s not found", node->name.c_str(), node->clock->name.c_str());
        val = clockMap[node->clock];
        clockMember = node->clock;
      }
      if (val) {
        Assert(!val->gateENode || val->node, "invalid clock %s %d", node->clock->name.c_str(), node->clock->lineno);
        if (val->gateENode) {
          if (node->type == NODE_WRITER) {
            ENode* gate = new ENode(OP_WHEN);
            gate->width = node->width;
            gate->sign = node->sign;
            ENode* cond = val->gateENode->dup();
            gate->addChild(cond);
            gate->addChild(nullptr);
            gate->addChild(nullptr);
            for (ExpTree* tree : node->assignTree) {
              ENode* gateDup = gate->dup();
              gateDup->setChild(1, tree->getRoot());
              tree->setRoot(gateDup);
            }
            for (ExpTree* tree : node->arrayVal) {
              ENode* gateDup = gate->dup();
              gateDup->setChild(1, tree->getRoot());
              tree->setRoot(gateDup);
            }
          }
        } else if (val->node) {
          ENode* clockENode = new ENode(val->node);
          clockENode->width = val->node->width;
          clockMember->assignTree[0]->setRoot(clockENode);
        } else {
          for (Node* member : node->member)
            member->setConstantZero(0);
        }
      }
    }
  }
}
