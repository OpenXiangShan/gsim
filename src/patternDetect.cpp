#include <cstdio>
#include <string>
#include "common.h"
#include <stack>
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
    return nullptr;
  }
  auto value = firStrBase(enode->getChild(1)->strVal);
  if (value.second != "1") {
    return nullptr;
  }
  if (!enode->getChild(0)) {
    return nullptr;
  }
  return enode->getChild(0);
}

/* pattern 4:
  node expr_lo = cat(expr_A, expr_B) 
  node expr_hi = cat(expr_C, expr_D) 
  node _expr_T = cat(expr_hi, expr_lo) 
  node expr = orr(_expr_T) 
  optimize:
  expr = ( orr(expr_A) | orr(expr_B) | orr(expr_C) | orr(expr_D) )
*/
bool checkNestedCat(ENode* enode, std::vector<ENode*>& leafNodes, bool hasCat) {
  if (enode->opType == OP_EMPTY && enode->nodePtr) {
    Node* refNode = enode->nodePtr;
    if (refNode->assignTree.size() == 1) {
      return checkNestedCat(refNode->assignTree[0]->getRoot(), leafNodes, hasCat);
    }
    return false;
  }
  if (enode->opType == OP_CAT) {
    if (!enode->getChild(0) || !enode->getChild(1)) {
      return false;
    }
    hasCat = true;
    return checkNestedCat(enode->getChild(0), leafNodes, hasCat) && checkNestedCat(enode->getChild(1), leafNodes, hasCat);
  }
  if (enode->opType == OP_BITS && hasCat) {
    leafNodes.push_back(enode);
    return true;
  }
  return false;
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
    ENode* intENode = allocIntEnode(shiftENode->width, std::to_string(next->assignTree[0]->getRoot()->values[0]));
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
    return false;
  }
  ENode* root = node->assignTree[0]->getRoot();
  ENode* bitAndNode = isBitAndOne(root);


  // if (pattern == (A & 1)) bitAndNode is A
  if (!bitAndNode) {
    return false;
  }
  
  ENode* shiftNode = nullptr;
  if (bitAndNode->opType == OP_DSHL && bitAndNode->getChild(0)->opType == OP_INT && firStrBase(bitAndNode->getChild(0)->strVal).second == "1") {
    shiftNode = bitAndNode->getChild(1);
  } else {
    shiftNode = nullptr;
  }

  if (!shiftNode) {
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

/* orr (expr), expr = cat(expr_A, expr_B)
 * orr(cat(expr_A, expr_B))
 * maybe recursive
 */
bool checkPattern4(Node* node) {
  if (node->isArray() || node->type != NODE_OTHERS || node->assignTree.size() != 1) {
    return false;
  }
  ENode* root = node->assignTree[0]->getRoot();
  if (root->opType != OP_ORR) {
    return false;
  }
  std::vector<ENode*> leafNodes;
  if (!root->getChild(0)) {
    return false; // never happen
  }
  bool res = checkNestedCat(root->getChild(0), leafNodes, false);
  if (res && leafNodes.size() > 1) {
    ENode* newRoot = new ENode(OP_OR);
    newRoot->width = root->width;
    newRoot->sign = root->sign;
    for (ENode* leaf : leafNodes) {
      ENode* orr = new ENode(OP_ORR);
      orr->width = 1;
      orr->addChild(leaf->dup());
      newRoot->addChild(orr);
    }
    printf("p4 tree before optimize:\n");
    root->display();
    ExpTree* newTree = new ExpTree(newRoot, new ENode(node));
    node->assignTree.clear();
    node->assignTree.push_back(newTree);
    printf("p4 tree after optimize:\n");
    newTree->display();
  }
  return res;
}

void testCheckPattern4() {
  Node *node = new Node();
  node->type = NODE_OTHERS;
  node->assignTree.push_back(new ExpTree(new ENode(OP_ORR)));
  ENode *orr = node->assignTree[0]->getRoot();
  orr->addChild(new ENode(OP_CAT));
  ENode* cat = orr->getChild(0);
  ENode* bits1 = new ENode(OP_BITS);
  bits1->width = 2;
  bits1->addVal(0);
  bits1->addVal(1);
  cat->addChild(bits1);
  ENode* bits2 = new ENode(OP_BITS);
  bits2->width = 2;
  bits2->addVal(0);
  bits2->addVal(1);
  cat->addChild(bits2);
  Assert(checkPattern4(node), "testCheckPattern4 case 1 failed");

  // Test case for pattern: node B = cat(C, D); node A = orr(B);
  Node *nodeB = new Node();
  nodeB->type = NODE_OTHERS;
  nodeB->assignTree.push_back(new ExpTree(new ENode(OP_CAT)));
  ENode *catB = nodeB->assignTree[0]->getRoot();
  
  // Add children C and D to cat node
  ENode* enodeC = new ENode(OP_BITS);
  enodeC->width = 2;
  enodeC->addVal(0);
  enodeC->addVal(1);
  catB->addChild(enodeC);
  
  ENode* enodeD = new ENode(OP_BITS); 
  enodeD->width = 2;
  enodeD->addVal(0);
  enodeD->addVal(1);
  catB->addChild(enodeD);

  // Create node A with orr operation
  Node *nodeA = new Node();
  nodeA->type = NODE_OTHERS;
  nodeA->assignTree.push_back(new ExpTree(new ENode(OP_ORR)));
  ENode *orrA = nodeA->assignTree[0]->getRoot();
  
  // Link orr to node B
  ENode *refB = new ENode(OP_EMPTY);
  refB->nodePtr = nodeB;
  orrA->addChild(refB);

  Assert(checkPattern4(nodeA), "testCheckPattern4 case 2 failed");
}

void graph::patternDetect() {
  int num1 = 0;
  int num2 = 0;
  int num3 = 0;
  int num4 = 0;

  //testCheckPattern4();

  // check pattern4
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num4 += checkPattern4(member);
    }
  }

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


  // check pattern1, and optimize
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num1 += checkPattern1(member);
    }
  }
  removeNodesNoConnect(DEAD_NODE);
  reconnectAll();
  printf("[patternDetect] find %d pattern1, %d pattern2((1<<expr) & 1), %d pattern3(A & 1) %d pattern4(orr(cat(expr)))\n", num1, num2, num3, num4);
}


