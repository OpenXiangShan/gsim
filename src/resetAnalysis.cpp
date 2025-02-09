#include "common.h"
#include <map>
/* before splitted array  */

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

std::map<Node*, clockVal*> resetMap;

ResetType Node::inferReset() {
  if (reset != UNCERTAIN) return reset;
  if (assignTree.size() > 1) TODO();
  if (!isArray()) {
    reset = assignTree.back()->getRoot()->inferReset();
  } else {
    if (assignTree.size() != 0) TODO();
    for (ExpTree* tree : arrayVal) {
      ResetType newReset = tree->getRoot()->inferReset();
      if (reset == UNCERTAIN) reset = newReset;
      else if (reset != tree->getRoot()->reset) {
        printf("reset %d %d\n", reset, tree->getRoot()->reset);
        Panic();
      }
    }
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
      if (str == "h0" || str == "0")
        reset = ZERO_RESET;
      else {
        std::cout << "Unknown :" << str << std::endl;
        TODO();
      }
      break;
    case OP_ASASYNCRESET:
      reset = ASYRESET;
      break;
    case OP_BITS:
    case OP_OR:
      reset = UINTRESET;
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
  reset = resetType;
  Assert(resetType != UNCERTAIN, "reset %s is uncertain", name.c_str());
  if (resetType == ZERO_RESET) {
    /* do nothing */
  } else if (resetType == UINTRESET) {

  } else if (resetType == ASYRESET) {
    ENode* regTop = new ENode(OP_RESET);
    regTop->addChild(resetCond->getRoot());
    if (resetVal->getRoot()->getNode() != this)
      regTop->addChild(resetVal->getRoot());
    else
      regTop->addChild(nullptr);
    assignTree.clear();
    assignTree.push_back(new ExpTree(regTop, this));
  } else {
    Panic();
  }
}