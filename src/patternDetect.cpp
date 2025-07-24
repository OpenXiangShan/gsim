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

/* pattern 2:
  _T = Cat(lsignal, rsignal) == constant
  optimized:
  _T = (lsignal == constant1) & (rsignal == constant2) 
*/

// rop = op[high_bit:low_bit]
static void BITS_mpz(mpz_t rop, const mpz_t op, size_t high_bit, size_t low_bit) {
    mpz_t mask;
    mpz_init(mask);
    mpz_tdiv_q_2exp(rop, op, low_bit);
    mpz_set_ui(mask, 1);
    mpz_mul_2exp(mask, mask, (high_bit - low_bit + 1));
    mpz_sub_ui(mask, mask, 1);
    mpz_and(rop, rop, mask);
    mpz_clear(mask);
}

bool checkPattern2(Node *node) {
    if (node->isArray() || node->type != NODE_OTHERS || node->assignTree.size() != 1) return false;
    ENode *root = node->assignTree[0]->getRoot();
    ENode *equalNode = root->opType == OP_EQ ? root : nullptr;
    if(!equalNode) return false;
    bool ret = false;
    if(equalNode->getChild(1)->opType == OP_INT) {
        ENode *catNode = equalNode->getChild(0);
        if(catNode->opType != OP_CAT && catNode->opType != OP_OR)
            return false;
        ENode *lsignal = catNode->getChild(0);
        ENode *rsignal = catNode->getChild(1);
        if(lsignal->opType == OP_SHL) lsignal = lsignal->getChild(0);
        if(rsignal->opType == OP_PAD) rsignal = rsignal->getChild(0);

        mpz_t constant, constant1, constant2;
        mpz_inits(constant, constant1, constant2, nullptr);
        mpz_set_str(constant, equalNode->getChild(1)->strVal.data(), 10);
        // printf("constant:%s\n", mpz_get_str(nullptr, 10, constant));

        int lwidth = lsignal->width;
        int rwidth = rsignal->width;
        if(catNode->opType == OP_CAT) {
            BITS_mpz(constant1, constant, lwidth + rwidth, rwidth + 1);
            BITS_mpz(constant2, constant, rwidth - 1, 0);
            ret = true;
        } else if(catNode->opType == OP_OR) {
            ENode* shlNode = catNode->getChild(0);
            if(shlNode->opType == OP_SHL) {
                uint16_t shift = shlNode->values[0];
                BITS_mpz(constant1, constant, shlNode->width - 1, shift);
                BITS_mpz(constant2, constant, rwidth - 1, 0);
                ret = true;
            }
        }
        
        if(ret) {
          ENode *leqNode = new ENode(OP_EQ);
          ENode *reqNode = new ENode(OP_EQ);
          leqNode->width = 1;
          reqNode->width = 1;
          leqNode->addChild(lsignal->dup());
          leqNode->addChild(allocIntEnode(lwidth, std::string(mpz_get_str(nullptr, 10, constant1))));

          reqNode->addChild(rsignal->dup());
          reqNode->addChild(allocIntEnode(rwidth, std::string(mpz_get_str(nullptr, 10, constant2))));
        
          root->opType = OP_AND;
          root->child.clear();
          root->addChild(leqNode);
          root->addChild(reqNode);
        }
        mpz_clears(constant, constant1, constant2, nullptr);
    }   

    return ret;
}

void graph::patternDetect() {
  int num1 = 0;
  int num2 = 0;
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      num2 += checkPattern2(member);
      num1 += checkPattern1(member);
    }
  }
  removeNodesNoConnect(DEAD_NODE);
  reconnectAll();
  printf("[patternDetect] find %d pattern1\n", num1);
  printf("[patternDetect] find %d pattern2\n", num2);
}