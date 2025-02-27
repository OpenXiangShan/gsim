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
// [(1 << expr) &  1] -> (expr == 0)
// if (pattern == (A & 1)) return A
// else return nullptr
static ENode* isBitAndOne(ENode* enode) {
  if (enode->opType != OP_AND || enode->getChild(1)->opType != OP_INT) {
    // printf("---isBitAndOne  if1 failed---\n");
    return nullptr;
  }
  auto value = firStrBase(enode->getChild(1)->strVal);
  if (value.second != "1") {
    // printf("---isBitAndOne  if2 failed--- value.second == %s\n", value.second.c_str());
    return nullptr;
  }
  if (!enode->getChild(0)) {
    // printf("---isBitAndOne  if3 failed---\n");
    return nullptr;
  }
  return enode->getChild(0);
}
// (1 << expr) 
bool checkPattern1(Node* node) {
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
// ((1<<expr) & 1) 
bool checkPattern2(Node* node) {
  if (node->isArray() || node->type != NODE_OTHERS || node->assignTree.size() != 1) {
    // printf("---checkPattern2  if1 failed---\n");
    return false;
  }
  ENode* root = node->assignTree[0]->getRoot();
  ENode* bitAndNode = isBitAndOne(root);

  // ExpTree* tree = new ExpTree(bitAndNode, new ENode(node));
  // printf("---checkPattern2  bitAndNode tree---\n");
  // tree->display();
  // if (pattern == (A & 1)) bitAndNode is A
  if (!bitAndNode) {
    // printf("---checkPattern2  if2 failed---\n");
    return false;
  }
  // if A == (1 << expr), shiftNode is expr
  // printf("bitAndNode: (opType=OP_DSHL) == %d\n, (child0 opType=OP_INT) == %d  child0 value second == %s\n", 
  //   bitAndNode->opType == OP_DSHL, bitAndNode->getChild(0)->opType == OP_INT, firStrBase(bitAndNode->getChild(0)->strVal).second.c_str());
  ENode* shiftNode = nullptr;
  if (bitAndNode->opType == OP_DSHL && bitAndNode->getChild(0)->opType == OP_INT && firStrBase(bitAndNode->getChild(0)->strVal).second == "1") {
    shiftNode = bitAndNode->getChild(1);
  } else {
    shiftNode = nullptr;
  }
  // ExpTree* tree2 = new ExpTree(shiftNode, new ENode(node));
  // printf("---checkPattern2  shiftNode tree---\n");
  // tree2->display();
  if (!shiftNode) {
    // printf("---checkPattern2  if3 failed---\n");
    return false;
  }
  return true;
}

// A & 1
bool checkPattern3(Node *node) {
  if (node->isArray() || node->type != NODE_OTHERS || node->assignTree.size() != 1) {
    return false;
  }
  ENode* root = node->assignTree[0]->getRoot();
  if (root->opType != OP_AND) {
    return false;
  }
  if (root->getChild(1)->opType != OP_INT) {
    return false;
  }
  auto value = firStrBase(root->getChild(1)->strVal);
  if (value.second != "1") {
    return false;
  }
  return true;
}

void testCheckPattern2() {
  // Test case 1: Valid pattern (1 << 5) & 0x1
  // both "1" maybe 0h1 or 0b1 or 0o1 or 1
  {
    Node* testNode = new Node();
    testNode->type = NODE_OTHERS;
    testNode->width = 1;
    
    ENode* exprNode = new ENode(OP_INT);
    exprNode->strVal = "5";
    exprNode->width = 32;
    
    ENode* shiftNode = new ENode(OP_DSHL);
    shiftNode->width = 32;
    ENode* oneNode = new ENode(OP_INT);
    oneNode->strVal = "1";
    oneNode->width = 32;
    shiftNode->addChild(oneNode);
    shiftNode->addChild(exprNode);
    
    ENode* andNode = new ENode(OP_AND);
    andNode->width = 1;
    andNode->addChild(shiftNode);
    ENode* maskNode = new ENode(OP_INT);
    maskNode->strVal = "0h1";
    maskNode->width = 32;
    andNode->addChild(maskNode);
    
    ExpTree* tree = new ExpTree(andNode, new ENode(testNode));
    testNode->assignTree.push_back(tree);
    
    bool result = checkPattern2(testNode);
    Assert(result == true, "testCheckPattern2 case1 failed");
    
    delete testNode;
  }

  // Test case 2: Invalid pattern - not AND operation
  {
    Node* testNode = new Node();
    testNode->type = NODE_OTHERS;
    testNode->width = 1;
    
    ENode* exprNode = new ENode(OP_INT);
    exprNode->strVal = "5";
    exprNode->width = 32;
    
    ENode* shiftNode = new ENode(OP_DSHL);
    shiftNode->width = 32;
    ENode* oneNode = new ENode(OP_INT);
    oneNode->strVal = "1";
    oneNode->width = 32;
    shiftNode->addChild(oneNode);
    shiftNode->addChild(exprNode);
    
    // Using OR instead of AND
    ENode* orNode = new ENode(OP_OR);
    orNode->width = 1;
    orNode->addChild(shiftNode);
    ENode* maskNode = new ENode(OP_INT);
    maskNode->strVal = "1";
    maskNode->width = 1;
    orNode->addChild(maskNode);
    
    ExpTree* tree = new ExpTree(orNode, new ENode(testNode));
    testNode->assignTree.push_back(tree);
    
    bool result = checkPattern2(testNode);
    Assert(result == false, "testCheckPattern2 case2 failed");
    
    delete testNode;
  }

  // Test case 3: Invalid pattern - mask is not 1
  {
    Node* testNode = new Node();
    testNode->type = NODE_OTHERS;
    testNode->width = 1;
    
    ENode* exprNode = new ENode(OP_INT);
    exprNode->strVal = "5";
    exprNode->width = 32;
    
    ENode* shiftNode = new ENode(OP_DSHL);
    shiftNode->width = 32;
    ENode* oneNode = new ENode(OP_INT);
    oneNode->strVal = "1";
    oneNode->width = 32;
    shiftNode->addChild(oneNode);
    shiftNode->addChild(exprNode);
    
    ENode* andNode = new ENode(OP_AND);
    andNode->width = 1;
    andNode->addChild(shiftNode);
    ENode* maskNode = new ENode(OP_INT);
    maskNode->strVal = "2";  // Invalid mask value
    maskNode->width = 1;
    andNode->addChild(maskNode);
    
    ExpTree* tree = new ExpTree(andNode, new ENode(testNode));
    testNode->assignTree.push_back(tree);
    
    bool result = checkPattern2(testNode);
    Assert(result == false, "testCheckPattern2 case3 failed");
    
    delete testNode;
  }

  // Test case 4: Invalid pattern - shift value is not 1
  {
    Node* testNode = new Node();
    testNode->type = NODE_OTHERS;
    testNode->width = 1;
    
    ENode* exprNode = new ENode(OP_INT);
    exprNode->strVal = "5";
    exprNode->width = 32;
    
    ENode* shiftNode = new ENode(OP_DSHL);
    shiftNode->width = 32;
    ENode* oneNode = new ENode(OP_INT);
    oneNode->strVal = "2";  // Invalid shift value
    oneNode->width = 32;
    shiftNode->addChild(oneNode);
    shiftNode->addChild(exprNode);
    
    ENode* andNode = new ENode(OP_AND);
    andNode->width = 1;
    andNode->addChild(shiftNode);
    ENode* maskNode = new ENode(OP_INT);
    maskNode->strVal = "1";
    maskNode->width = 1;
    andNode->addChild(maskNode);
    
    ExpTree* tree = new ExpTree(andNode, new ENode(testNode));
    testNode->assignTree.push_back(tree);
    
    bool result = checkPattern2(testNode);
    Assert(result == false, "testCheckPattern2 case4 failed");
    
    delete testNode;
  }
}

void graph::patternDetect() {
  testCheckPattern2();
  int num1 = 0;
  int num2 = 0;
  int num3 = 0;

  // check pattern3
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num3 += checkPattern3(member);
    }
  }

  // check pattern2
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num2 += checkPattern2(member);
    }
  }


  // check pattern1
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num1 += checkPattern1(member);
    }
  }
  removeNodesNoConnect(DEAD_NODE);
  reconnectAll();
  printf("[patternDetect] find %d pattern1, %d pattern2((1<<expr) & 1), %d pattern3(A & 1)\n", num1, num2, num3);
}


