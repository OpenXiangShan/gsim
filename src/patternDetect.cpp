#include <cstdio>
#include <string>
#include "common.h"
/* detect and optimize some specific patterns */
/* behind constantAnalysis */
Node* getLeafNode(bool isArray, ENode* enode);
/* pattern 1:
  oneHot = dshl(1, shift)
  _T_0 = bits(oneHot, 0, 0)
  _T_1 = bits(oneHot, 1, 1)
  ....
  optimize:
  _T_i = shift == i
*/
static ENode* isDshlOne(ENode* enode) {
  if(enode->opType != OP_DSHL || enode->getChild(0)->opType != OP_INT) return nullptr;
  auto value = firStrBase(enode->getChild(0)->strVal);
  if (value.second != "1") return nullptr;
  if (enode->getChild(1)->nodePtr) return enode->getChild(1);
  return nullptr;
}
static bool isBitsOne(ENode* enode) {
  return (enode->opType == OP_BITS && enode->values[0] == enode->values[1]);
}
bool checkPatern1(Node* node) {
  if (node->isArray() || node->type != NODE_OTHERS || node->assignTree.size() != 1) return false;
  ENode* root = node->assignTree[0]->getRoot();
  ENode* shiftENode = isDshlOne(root);
  if (!shiftENode) return false;
  bool ret = true;
  for (Node* next : node->next) {
    if (next->isArray() || next->assignTree.size() != 1 || !isBitsOne(next->assignTree[0]->getRoot())) {
      ret = false;
      continue;
    }
    ENode* intENode = new ENode(OP_INT);
    intENode->strVal = std::to_string(next->assignTree[0]->getRoot()->values[0]);
    intENode->width = shiftENode->width;
    ENode* eq = new ENode(OP_EQ);
    eq->width = 1;
    eq->addChild(shiftENode->dup());
    eq->addChild(intENode);
    ExpTree* newTree = new ExpTree(eq, new ENode(next));
    next->assignTree.clear();
    next->assignTree.push_back(newTree);
  }
  if (ret) node->status = DEAD_NODE;
  return ret;
}

void graph::patternDetect() {
  int num1 = 0;
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num1 += checkPatern1(member);
    }
  }
  removeNodesNoConnect(DEAD_NODE);
  reconnectAll();
  printf("[patternDetect] find %d pattern1\n", num1);
}