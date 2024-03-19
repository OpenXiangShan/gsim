#include "common.h"
#include <map>
/* before splitted array  */

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

std::map<Node*, clockVal*> resetMap;

ResetType Node::inferReset() {
  if (reset != UNCERTAIN) return reset;
  if (valTree) {
    valTree->getRoot()->inferReset();
    reset = valTree->getRoot()->reset;
  } else {
    reset = UINTRESET;
  }
  return reset;
}

ResetType ENode::inferReset() {
  if (reset != UNCERTAIN) return reset;
  if (nodePtr) {
    reset = nodePtr->inferReset(); 
    return reset;
  }

  switch (opType) {
    case OP_ASUINT:
    case OP_ASSINT:
    case OP_INT:
      reset = UINTRESET;
      break;
    case OP_ASASYNCRESET:
      reset = ASYRESET;
      break;
    case OP_ASCLOCK:
    default:
      printf("opType %d\n", opType);
      Panic();
  }
  return reset;
}

void Node::addReset() {
  Assert(type == NODE_REG_SRC, "%s(%d) is not regsrc", name.c_str(), type);

  ResetType resetType = resetCond->getRoot()->inferReset();
  Assert(resetType != UNCERTAIN, "reset %s is uncertain", name.c_str());

  // if (resetType == ZERO_RESET) {
  //   if (getDst()->valTree) fillEmptyWhen(getDst()->valTree, new ENode(OP_CONSTANT_INVALID));
  //   for (ExpTree* tree : getDst()->arrayVal) fillEmptyWhen(tree, new ENode(OP_CONSTANT_INVALID));
  // } else 
  if (resetType == UINTRESET) {
    ENode* regTop = new ENode(OP_WHEN);
    regTop->addChild(resetCond->getRoot());
    regTop->addChild(resetVal->getRoot());
    if (getDst()->valTree) {
      regTop->addChild(getDst()->valTree->getRoot());
      getDst()->valTree->setRoot(regTop);
    } else {
      regTop->addChild(nullptr);
      getDst()->valTree = new ExpTree(regTop, this->getDst());
    }
    for (ExpTree* tree : getDst()->arrayVal) {
      ENode* arrayWhenTop = new ENode(OP_WHEN);
      arrayWhenTop->addChild(resetCond->getRoot());
      arrayWhenTop->addChild(nullptr);
      arrayWhenTop->addChild(tree->getRoot());
      tree->setRoot(arrayWhenTop);
    }
  } else if (resetType == ASYRESET) {
    ENode* regTop = new ENode(OP_RESET);
    regTop->addChild(resetCond->getRoot());
    if (resetVal->getRoot()->getNode() != this)
      regTop->addChild(resetVal->getRoot());
    else
      regTop->addChild(nullptr);
    if (valTree) {
      valTree->setRoot(regTop);
    } else {
      valTree = new ExpTree(regTop, this);
    }
    if (getDst()->valTree) fillEmptyWhen(getDst()->valTree, new ENode(this));
  } else {
    Panic();
  }
}