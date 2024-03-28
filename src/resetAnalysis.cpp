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
  int base;
  std::string str;
  switch (opType) {
    case OP_ASUINT:
    case OP_ASSINT:
      reset = UINTRESET;
      break;
    case OP_INT:
      std::tie(base, str) = firStrBase(strVal);
      if (str == "0") reset = ZERO_RESET;
      else TODO();
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

void fillOuterWhen(ExpTree* newTree, ENode* enode) {
  ENode* whenNode = newTree->getRoot();
  while (whenNode->opType == OP_WHEN) {
    if (!whenNode->getChild(1)) whenNode->setChild(1, enode);
    if (!whenNode->getChild(2)) whenNode->setChild(2, enode);
    if (whenNode->getChild(1) && whenNode->getChild(2)) break;
    else if (whenNode->getChild(1)) whenNode = whenNode->getChild(1);
    else if (whenNode->getChild(2)) whenNode = whenNode->getChild(2);
    else Assert(0, "emptyWhen");
  }
}

void Node::addReset() {
  Assert(type == NODE_REG_SRC, "%s(%d) is not regsrc", name.c_str(), type);

  ResetType resetType = resetCond->getRoot()->inferReset();
  Assert(resetType != UNCERTAIN, "reset %s is uncertain", name.c_str());

  if (resetType == ZERO_RESET) {
    /* do nothing */
  } else if (resetType == UINTRESET) {
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
    else getDst()->valTree = new ExpTree(new ENode(this), getDst());
  } else {
    Panic();
  }
}