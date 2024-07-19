/*
  Generator instructions for every nodes in sortedSuperNodes
  merge constantNode into here
*/

#include <cstdio>
#include <map>
#include <gmp.h>
#include <queue>
#include <stack>
#include <tuple>
#include <utility>

#include "common.h"
#include "graph.h"
#include "opFuncs.h"

#define Child(id, name) getChild(id)->name
#define ChildCons(id, name) consEMap[getChild(id)]->name

void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);
static void recomputeAllNodes();

static std::map<Node*, valInfo*> consMap;
static std::map<ENode*, valInfo*> consEMap;

static std::priority_queue<Node*, std::vector<Node*>, ordercmp> recomputeQueue;
static std::set<Node*> uniqueRecompute;

static void addRecompute(Node* node) {
  if (uniqueRecompute.find(node) == uniqueRecompute.end()) {
    recomputeQueue.push(node);
    uniqueRecompute.insert(node);
  }
}

static bool mpzOutOfBound(mpz_t& val, int width) {
  mpz_t tmp;
  mpz_init(tmp);
  mpz_set_ui(tmp, 1);
  mpz_mul_2exp(tmp, tmp, width);
  mpz_sub_ui(tmp, tmp, 1);
  if (mpz_cmp(val, tmp) > 0) return true;
  return false;
}

valInfo* setENodeCons(ENode* enode, std::string str) {
  valInfo* consInfo = new valInfo(enode->width, enode->sign);
  consInfo->setConstantByStr(str);
  consEMap[enode] = consInfo;
  return consInfo;
}

valInfo* setNodeCons(Node* node, std::string str) {
  node->status = CONSTANT_NODE;
  valInfo* consInfo = new valInfo(node->width, node->sign);
  consInfo->setConstantByStr(str);
  consMap[node] = consInfo;
  return consInfo;
}

bool cons_resetConsEq(valInfo* dstInfo, Node* regsrc) {
  if (!regsrc->resetTree) return true;
  valInfo* info = regsrc->resetTree->getRoot()->computeConstant(regsrc, regsrc->name.c_str());
  if (info->status == VAL_EMPTY) return true;
  mpz_t consVal;
  mpz_init(consVal);
  mpz_set(consVal, dstInfo->status == VAL_CONSTANT ? dstInfo->consVal : dstInfo->assignmentCons);
  if (info->status == VAL_CONSTANT && mpz_cmp(info->consVal, consVal) == 0) return true;
  if (info->sameConstant && mpz_cmp(info->assignmentCons, consVal) == 0) return true;
  return false;
}

valInfo* ENode::consMux(bool isLvalue) {
  /* cond is constant */
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (mpz_cmp_ui(ChildCons(0, consVal), 0) == 0) return consEMap[getChild(2)];
    else return consEMap[getChild(1)];
  }
  if (ChildCons(1, status) == VAL_CONSTANT && ChildCons(2, status) == VAL_CONSTANT) {
    if (mpz_cmp(ChildCons(1, consVal), ChildCons(2, consVal)) == 0) {
      return consEMap[getChild(1)];
    }
  }
  return new valInfo(width, sign);
}

valInfo* ENode::consWhen(Node* node, bool isLvalue) {
  /* cond is constant */
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (mpz_cmp_ui(ChildCons(0, consVal), 0) == 0) {
      if (getChild(2)) return consEMap[getChild(2)];
      else {
        valInfo* consInfo = new valInfo(0, 0);
        consInfo->status = VAL_EMPTY;
        return consInfo;
      }
    } else {
      if (getChild(1)) return consEMap[getChild(1)];
      valInfo* consInfo = new valInfo(0, 0);
      consInfo->status = VAL_EMPTY;
      return consInfo;
    }
  }
  if (getChild(1) && ChildCons(1, status) == VAL_CONSTANT && getChild(2) && ChildCons(2, status) == VAL_CONSTANT) {
    if (mpz_cmp(ChildCons(1, consVal), ChildCons(2, consVal)) == 0) {
      return consEMap[getChild(1)];
    }
  }
  if (getChild(1) && ChildCons(1, status) == VAL_INVALID && (node->type == NODE_OTHERS || node->type == NODE_MEM_MEMBER)) {
    if (getChild(2)) return consEMap[getChild(2)];
    valInfo* consInfo = new valInfo(0, 0);
    consInfo->status = VAL_EMPTY;
    return consInfo;
  }
  if (getChild(2) && ChildCons(2, status) == VAL_INVALID && (node->type == NODE_OTHERS || node->type == NODE_MEM_MEMBER)) {
    if (getChild(1)) return consEMap[getChild(1)];
    valInfo* consInfo = new valInfo(0, 0);
    consInfo->status = VAL_EMPTY;
    return consInfo;
  }
  valInfo* ret = new valInfo(width, sign);

  if (!getChild(1) && getChild(2)) {
    if (ChildCons(2, sameConstant)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildCons(2, assignmentCons));
    }
    if (ChildCons(2, beg) != ChildCons(2, end)) {
      for (int i = ChildCons(2, beg); i <= ChildCons(2, end); i ++) {
        if (ChildCons(2, getMemberInfo(i)) && ChildCons(2, getMemberInfo(i))->status == VAL_CONSTANT) {
          valInfo* info = new valInfo(width, sign);
          info->sameConstant = true;
          mpz_set(info->assignmentCons, ChildCons(2, getMemberInfo(i))->consVal);
          ret->memberInfo.push_back(info);
        } else {
          ret->memberInfo.push_back(nullptr);
        }
      }
    }
  } else if (getChild(1) && !getChild(2)) {
    if (ChildCons(1, sameConstant)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildCons(1, assignmentCons));
    }
  } else if (getChild(1) && getChild(2)) {
    if (ChildCons(1, sameConstant) && ChildCons(2, sameConstant) && (mpz_cmp(ChildCons(1, assignmentCons), ChildCons(2, assignmentCons)) == 0)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildCons(1, assignmentCons));
    }
  }
  return ret;
}

valInfo* ENode::consStmt(Node* node, bool isLvalue) {
  if (getChildNum() == 1) return getChild(0)->computeConstant(node, isLvalue);
  return new valInfo(width, sign);
}

valInfo* ENode::consAdd(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_add(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consSub(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_sub(ret->consVal, ChildCons(0, consVal), ChildCons(1, consVal), width);
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consMul(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_mul(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consDIv(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_div(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consRem(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_rem(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consLt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_lt(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consLeq(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_leq(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consGt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_gt(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consGeq(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_geq(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consEq(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_eq(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  } else if ((ChildCons(0, status) == VAL_CONSTANT && mpzOutOfBound(ChildCons(0, consVal), Child(1, width)))
      ||(ChildCons(1, status) == VAL_CONSTANT && mpzOutOfBound(ChildCons(1, consVal), Child(0, width)))) {
    ret->setConstantByStr("0");
  }
  return ret;
}

valInfo* ENode::consNeq(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    us_neq(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consDshl(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    if (sign) TODO();
    u_dshl(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consDshr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAnd(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    if (sign) TODO();
    u_and(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  } else if ((ChildCons(0, status) == VAL_CONSTANT && mpz_sgn(ChildCons(0, consVal)) == 0) ||
            (ChildCons(1, status) == VAL_CONSTANT && mpz_sgn(ChildCons(1, consVal)) == 0)) {
          ret->setConstantByStr("0");
  }
  return ret;
}

valInfo* ENode::consOr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  mpz_t mask;
  mpz_init(mask);
  mpz_set_ui(mask, 1);
  mpz_mul_2exp(mask, mask, width);
  mpz_sub_ui(mask, mask, 1);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    if (sign) TODO();
    u_ior(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  } else if ((ChildCons(0, status) == VAL_CONSTANT && mpz_cmp(ChildCons(0, consVal), mask) == 0) ||
            (ChildCons(1, status) == VAL_CONSTANT && mpz_cmp(ChildCons(1, consVal), mask) == 0)) {
    mpz_set(ret->consVal, mask);
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consXor(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    if (sign) TODO();
    u_xor(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consCat(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if ((ChildCons(0, status) == VAL_CONSTANT) && (ChildCons(1, status) == VAL_CONSTANT)) {
    if (sign) TODO();
    u_cat(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), ChildCons(1, consVal), ChildCons(1, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAsUInt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_asUInt(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAsSInt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAsClock(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_asClock(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAsAsyncReset(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_asAsyncReset(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consCvt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    (sign ? s_cvt : u_cvt)(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consNeg(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (!sign) TODO();
    s_neg(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consNot(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_not(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consAndr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_andr(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consOrr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_orr(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consXorr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_xorr(ret->consVal, ChildCons(0, consVal), ChildCons(0, width));
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consPad(bool isLvalue) {
  /* no operation for UInt variable */
  if (!sign || (width <= ChildCons(0, width))) {
    return consEMap[getChild(0)];
  }
  /* SInt padding */
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    (sign ? s_pad : u_pad)(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consShl(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  int n = values[0];

  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_shl(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), n);
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consShr(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    (sign ? s_shr : u_shr)(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  }
  return ret;
}
/*
  trancate the n least significant bits
  different from tail operantion defined in firrtl spec (changed in inferWidth)
*/
valInfo* ENode::consHead(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  int n = values[0];
  Assert(n >= 0, "child width %d is less than current width %d", Child(0, width), values[0]);

  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_head(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), n);
    ret->setConsStr();
  }
  return ret;
}

/*
  remain the n least significant bits
  different from tail operantion defined in firrtl spec (changed in inferWidth)
*/
valInfo* ENode::consTail(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  int n = MIN(width, values[0]);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (sign) TODO();
    u_tail(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), n); // u_tail remains the last n bits
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::consBits(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  int hi = values[0];
  int lo = values[1];

  if (ChildCons(0, status) == VAL_CONSTANT) {
    u_bits(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), hi, lo);
    ret->setConsStr();
  } else if (lo >= Child(0, width) || lo >= ChildCons(0, width)) {
    ret->setConstantByStr("0");
  }
  return ret;
}

valInfo* ENode::consBitsNoShift(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  int hi = values[0];
  int lo = values[1];

  if (ChildCons(0, status) == VAL_CONSTANT) {
    u_bits_noshift(ret->consVal, ChildCons(0, consVal), ChildCons(0, width), hi, lo);
    ret->setConsStr();
  } else if (lo >= Child(0, width) || lo >= ChildCons(0, width)) {
    ret->setConstantByStr("0");
  }
  return ret;
}


valInfo* ENode::consIndexInt(bool isLvalue) {
  return new valInfo(width, sign);
}

valInfo* ENode::consIndex(bool isLvalue) {
  return new valInfo(width, sign);
}

valInfo* ENode::consInt(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  std::string str;
  int base;
  std::tie(base, str) = firStrBase(strVal);
  ret->setConstantByStr(str, base);
  return ret;
}

valInfo* ENode::consReadMem(bool isLvalue) {
  return new valInfo(width, sign);
}

valInfo* ENode::consInvalid(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  ret->status = VAL_INVALID;
  return ret;
}

valInfo* ENode::consReset(bool isLvalue) {
  valInfo* ret = new valInfo(width, sign);
  if (ChildCons(0, status) == VAL_CONSTANT) {
    if (mpz_sgn(ChildCons(0, consVal)) == 0) {
      ret->status = VAL_EMPTY_SRC;
    } else {
      ret = consEMap[getChild(1)];
    }
  }
  return ret;
}

/* compute enode */
valInfo* ENode::computeConstant(Node* node, bool isLvalue) {
  if (consEMap.find(this) != consEMap.end()) return consEMap[this];
  if (width == 0 && !isLvalue) {
    valInfo* ret = setENodeCons(this, "0");
    consEMap[this] = ret;
    return ret;
  }
  for (ENode* childNode : child) {
    if (childNode) childNode->computeConstant(node, isLvalue);
  }
  valInfo* ret;
  if (nodePtr) {
    if (nodePtr->isArray() && nodePtr->arraySplitted()) {
      if (getChildNum() < nodePtr->dimension.size()) {
        int beg, end;
        std::tie(beg, end) = getIdx(nodePtr);
        if (beg >= 0 && beg == end) {
          ret = nodePtr->getArrayMember(beg)->computeConstant()->dup();
        } else {
          ret = new valInfo(width, sign);
          ret->beg = beg;
          ret->end = end;
          ret->width = nodePtr->width;
          ret->sign = nodePtr->sign;
          ret->splittedArray = nodePtr;
          if (ret->beg >= 0) {
            for (int i = ret->beg; i <= ret->end; i ++) {
              Node* member = nodePtr->getArrayMember(i);
              member->computeConstant();
              if (consMap.find(member) != consMap.end()) ret->memberInfo.push_back(consMap[member]);
              else ret->memberInfo.push_back(nullptr);
            }
          }
        }
      } else {
        int idx = getArrayIndex(nodePtr);
        ret = nodePtr->getArrayMember(idx)->computeConstant()->dup();
      }
    } else if (nodePtr->isArray()) {
      int beg, end;
      std::tie(beg, end) = getIdx(nodePtr);
      ret = nodePtr->computeConstant()->dup(beg, end);
      ret->beg = beg;
      ret->end = end;
    } else {
      ret = nodePtr->computeConstant()->dup();
    }

    if (!isLvalue && ret->width <= BASIC_WIDTH) {
      if (ret->width > width) {
        if (ret->status == VAL_CONSTANT) {
          ret->updateConsVal();
        }
      }
    }
    ret->width = width;
    consEMap[this] = ret;
    return ret;
  }

  switch(opType) {
    case OP_ADD: ret = consAdd(isLvalue); break;
    case OP_SUB: ret = consSub(isLvalue); break;
    case OP_MUL: ret = consMul(isLvalue); break;
    case OP_DIV: ret = consDIv(isLvalue); break;
    case OP_REM: ret = consRem(isLvalue); break;
    case OP_LT:  ret = consLt(isLvalue); break;
    case OP_LEQ: ret = consLeq(isLvalue); break;
    case OP_GT:  ret = consGt(isLvalue); break;
    case OP_GEQ: ret = consGeq(isLvalue); break;
    case OP_EQ:  ret = consEq(isLvalue); break;
    case OP_NEQ: ret = consNeq(isLvalue); break;
    case OP_DSHL: ret = consDshl(isLvalue); break;
    case OP_DSHR: ret = consDshr(isLvalue); break;
    case OP_AND: ret = consAnd(isLvalue); break;
    case OP_OR:  ret = consOr(isLvalue); break;
    case OP_XOR: ret = consXor(isLvalue); break;
    case OP_CAT: ret = consCat(isLvalue); break;
    case OP_ASUINT: ret = consAsUInt(isLvalue); break;
    case OP_SEXT:
    case OP_ASSINT: ret = consAsSInt(isLvalue); break;
    case OP_ASCLOCK: ret = consAsClock(isLvalue); break;
    case OP_ASASYNCRESET: ret = consAsAsyncReset(isLvalue); break;
    case OP_CVT: ret = consCvt(isLvalue); break;
    case OP_NEG: ret = consNeg(isLvalue); break;
    case OP_NOT: ret = consNot(isLvalue); break;
    case OP_ANDR: ret = consAndr(isLvalue); break;
    case OP_ORR: ret = consOrr(isLvalue); break;
    case OP_XORR: ret = consXorr(isLvalue); break;
    case OP_PAD: ret = consPad(isLvalue); break;
    case OP_SHL: ret = consShl(isLvalue); break;
    case OP_SHR: ret = consShr(isLvalue); break;
    case OP_HEAD: ret = consHead(isLvalue); break;
    case OP_TAIL: ret = consTail(isLvalue); break;
    case OP_BITS: ret = consBits(isLvalue); break;
    case OP_BITS_NOSHIFT: ret = consBitsNoShift(isLvalue); break;
    case OP_INDEX_INT: ret = consIndexInt(isLvalue); break;
    case OP_INDEX: ret = consIndex(isLvalue); break;
    case OP_MUX: ret = consMux(isLvalue); break;
    case OP_WHEN: ret = consWhen(node, isLvalue); break;
    case OP_STMT: ret = consStmt(node, isLvalue); break;
    case OP_INT: ret = consInt(isLvalue); break;
    case OP_READ_MEM: ret = consReadMem(isLvalue); break;
    case OP_INVALID: ret = consInvalid(isLvalue); break;
    case OP_RESET: ret = consReset(isLvalue); break;
    case OP_PRINTF:
    case OP_ASSERT:
      ret = new valInfo();
      break;
    default:
      Panic();
  }
  consEMap[this] = ret;
  return ret;

}

void clearConsEMap(ExpTree* tree) {
  std::stack<ENode*> s;
  s.push(tree->getRoot());
  s.push(tree->getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (!top) continue;
    consEMap.erase(top);
    for (ENode* child : top->child) s.push(child);
  }
}

static void recomputeAllNodes() {
  while (!recomputeQueue.empty()) {
    Node* node = recomputeQueue.top();
    recomputeQueue.pop();
    uniqueRecompute.erase(node);
    node->recomputeConstant();
  }
}

void Node::recomputeConstant() {
  if (consMap.find(this) == consMap.end()) return;
  valInfo* prevVal = consMap[this];
  std::vector<valInfo*> prevArrayVal (consMap[this]->memberInfo);
  consMap.erase(this);
  for (ExpTree* tree : arrayVal) {
    if (tree) clearConsEMap(tree);
  }
  for (ExpTree* tree : assignTree) clearConsEMap(tree);
  computeConstant();
  bool recomputeNext = false;
  if (consMap[this]->status != prevVal->status) {
    recomputeNext = true;
  }
  for (size_t i = 0; i < prevArrayVal.size(); i ++) {
    if ((!prevArrayVal[i] && consMap[this]->memberInfo[i]) || (prevArrayVal[i] && !consMap[this]->memberInfo[i])) {
      recomputeNext = true;
      break;
    }
    if (prevArrayVal[i] && consMap[this]->memberInfo[i] && prevArrayVal[i]->status != consMap[this]->memberInfo[i]->status) {
      recomputeNext = true;
      break;
    }
  }
  if (recomputeNext) {
    for (Node* nextNode : (type == NODE_REG_DST ? getSrc()->next : next)) {
      if (uniqueRecompute.find(nextNode) == uniqueRecompute.end()) {
        recomputeQueue.push(nextNode);
        uniqueRecompute.insert(nextNode);
      }
    }
  }
}

valInfo* Node::computeConstantArray() {
  valInfo* ret = new valInfo(width, sign);
  std::set<int> allIdx;
  bool anyVarIdx = false;
  for (ExpTree* tree : assignTree) {
    std::string lvalue = name;
    if (tree->getlval()) {
      int beg, end;
      std::tie(beg, end) = tree->getlval()->getIdx(this);
      if (beg < 0) anyVarIdx = true;
      else {
        for (int i = beg; i <= end; i ++) {
          if (allIdx.find(i) != allIdx.end()) anyVarIdx = true;
          allIdx.insert(i);
        }
      }
    } else {
      display();
      TODO();
    }
    tree->getRoot()->computeConstant(this, false);
  }

  for (ExpTree* tree : arrayVal) {
      int beg, end;
      if (tree->getlval()) {
        for (ENode* lchild : tree->getlval()->child) lchild->computeConstant(this, true);
        tree->getRoot()->computeConstant(this, false);
        std::tie(beg, end) = tree->getlval()->getIdx(this);
        if (beg < 0) anyVarIdx = true;
        else {
          for (int i = beg; i <= end; i ++) {
            if (allIdx.find(i) != allIdx.end()) anyVarIdx = true;
            allIdx.insert(i);
          }
        }
      } else {
        TODO();
      }
  }

  if (!anyVarIdx) {
    int num = arrayEntryNum();
    ret->memberInfo.resize(num, nullptr);
    for (ExpTree* tree : assignTree) {
      int infoIdxBeg, infoIdxEnd;
      std::tie(infoIdxBeg, infoIdxEnd) = tree->getlval()->getIdx(this);
      if (consEMap[tree->getRoot()]->status == VAL_CONSTANT) {
        if (infoIdxBeg == infoIdxEnd) {
          ret->memberInfo[infoIdxBeg] = consEMap[tree->getRoot()];
        } else if (consEMap[tree->getRoot()]->memberInfo.size() != 0) {
          for (int i = 0; i <= infoIdxEnd - infoIdxBeg; i ++) {
            ret->memberInfo[infoIdxBeg + i] = consEMap[tree->getRoot()]->getMemberInfo(i);
          }
        }
      }
    }

    for (ExpTree* tree : arrayVal) {
      int infoIdxBeg, infoIdxEnd;
      std::tie(infoIdxBeg, infoIdxEnd) = tree->getlval()->getIdx(this);
      if (consEMap[tree->getRoot()]->status == VAL_CONSTANT) {
        if (infoIdxBeg == infoIdxEnd) {
          ret->memberInfo[infoIdxBeg] = consEMap[tree->getRoot()];

        } else if (consEMap[tree->getRoot()]->memberInfo.size() != 0) {
          for (int i = 0; i <= infoIdxEnd - infoIdxBeg; i ++) {
            ret->memberInfo[infoIdxBeg + i] = consEMap[tree->getRoot()]->getMemberInfo(i);
          }
        }

      }
    }

    if (type == NODE_REG_DST && getSrc()->assignTree.size() == 0) {
      for (int i = 0; i < num; i ++) {
        if (ret->memberInfo[i] && ret->memberInfo[i]->sameConstant) {
          ret->memberInfo[i]->status = VAL_CONSTANT;
          mpz_set(ret->memberInfo[i]->consVal, ret->memberInfo[i]->assignmentCons);
          ret->memberInfo[i]->setConsStr();
        }
      }
    }

    mpz_t sameConsVal;
    mpz_init(sameConsVal);
    bool isStart = true;
    bool isSame = true;
    for (int i = 0; i < num; i++) {
      if (!ret->memberInfo[i] || ret->memberInfo[i]->status != VAL_CONSTANT) {
        isSame = false;
        break;
      }
      if (isStart) {
        mpz_set(sameConsVal, ret->memberInfo[i]->consVal);
        isStart = false;
      }
      if (mpz_cmp(sameConsVal, ret->memberInfo[i]->consVal) != 0) {
        isSame = false;
        break;
      }
    }
    if (isSame) {
      ret->status = VAL_CONSTANT;
      mpz_set(ret->consVal, sameConsVal);
      status = CONSTANT_NODE;
    }

  }

  consMap[this] = ret;
  return ret;
}

void graph::constantMemory() {
  int num = 0;
  mpz_t val;
  mpz_init(val);
  while (1) {
    for (Node* mem : memory) {
      bool isConstant = true;
      bool isFirst = true;
      for (Node* port : mem->member) {
        if (port->type == NODE_WRITER) {
          Node* wdata = port->get_member(WRITER_DATA);
          if (wdata->status == CONSTANT_NODE) {
            if (isFirst) {
              mpz_set(val, consMap[wdata]->consVal);
              isFirst = false;
            } else if (mpz_cmp(consMap[wdata]->consVal, val) == 0) {

            } else {
              isConstant = false;
              break;
            }
          } else {
            isConstant = false;
            break;
          }
        }
      }
      if (isConstant) {
        num ++;
        for (Node* port : mem->member) {
          if (port->type == NODE_READER) {
            setNodeCons(port->get_member(READER_ADDR), "0");
            setNodeCons(port->get_member(READER_EN), "0");
            setNodeCons(port->get_member(READER_DATA), mpz_get_str(NULL, 16, val));
            for (Node* next : port->get_member(READER_DATA)->next) {
              if (consMap.find(next) != consMap.end()) addRecompute(next);
            }
          } else if (port->type == NODE_WRITER) {
            setNodeCons(port->get_member(WRITER_ADDR), "0");
            setNodeCons(port->get_member(WRITER_EN), "0");
            setNodeCons(port->get_member(WRITER_MASK), "0");
          } else TODO();
        }
        mem->status = CONSTANT_NODE;
      }
    }
    if (recomputeQueue.empty()) break;
    recomputeAllNodes();
    memory.erase(
      std::remove_if(memory.begin(), memory.end(), [](const Node* n){ return n->status == CONSTANT_NODE; }),
        memory.end()
    );
  }
  printf("[constantNode] find %d constant memories\n", num);
}

valInfo* Node::computeConstant() {
  if (consMap.find(this) != consMap.end()) return consMap[this];
  if (computeInfo) {
    consMap[this] = computeInfo;
    return computeInfo;
  }

  Assert(status != DEAD_NODE, "compute constant deadNode %s\n", name.c_str());
  if (width == 0) {
    status = CONSTANT_NODE;
    valInfo* consInfo = new valInfo(width, sign);
    consInfo->setConstantByStr("0");
    consMap[this] = consInfo;
    return consInfo;
  }
  if (isArray()) {
    return computeConstantArray();
  }

  if (assignTree.size() == 0) {
    valInfo* ret = new valInfo(width, sign);
    if (type == NODE_OTHERS) {
      status = CONSTANT_NODE;
      ret->setConstantByStr("0");
    }
    consMap[this] = ret;
    return ret;
  }
  valInfo* ret = nullptr;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    ExpTree* tree = assignTree[i];
    valInfo* info = tree->getRoot()->computeConstant(this, false);
    if (info->status == VAL_EMPTY || info->status == VAL_INVALID) continue;
    ret = info->dupWithCons();
    if ((ret->status == VAL_INVALID || ret->status == VAL_CONSTANT) && (i < assignTree.size() - 1) ) {
      // TODO: replace using OP_INT
      fillEmptyWhen(assignTree[i+1], tree->getRoot());
      assignTree.erase(assignTree.begin() + i);
      i --;
    }
  }

  if (!ret) ret = assignTree.back()->getRoot()->computeConstant(this, false)->dup();
  Assert(ret, "empty info in %s\n", name.c_str());
  if (ret->status == VAL_INVALID || ret->status == VAL_EMPTY) ret->setConstantByStr("0");
  if (ret->status == VAL_CONSTANT) {
    status = CONSTANT_NODE;
    if (type == NODE_REG_DST) {
      if ((getSrc()->assignTree.size() == 0 || (getSrc()->status == CONSTANT_NODE && mpz_cmp(ret->consVal, consMap[getSrc()]->consVal) == 0)) && cons_resetConsEq(ret, getSrc())) {
        getSrc()->status = CONSTANT_NODE;
        consMap[getSrc()] = ret;
        /* re-compute nodes depend on src */
        for (Node* next : (regSplit ? getSrc() : this)->next) {
          if (consMap.find(next) != consMap.end()) {
            addRecompute(next);
          }
        }
        recomputeAllNodes();
      }
    } else if (type == NODE_MEM_MEMBER && mpz_sgn(ret->consVal) == 0) {
      Node* port = parent;
      if (port->type == NODE_READER) {
        if (this == port->get_member(READER_EN)) {
          setNodeCons(port->get_member(READER_DATA), "0");
          setNodeCons(port->get_member(READER_ADDR), "0");
          for (Node* next : port->get_member(READER_DATA)->next) {
            if (consMap.find(next) != consMap.end()) addRecompute(next);
          }
        }
      } else if (port->type == NODE_WRITER) {
        if (this == port->get_member(WRITER_EN) || this == port->get_member(WRITER_MASK)) {
          setNodeCons(port->get_member(WRITER_ADDR), "0");
          setNodeCons(port->get_member(WRITER_DATA), "0");
          if (this != port->get_member(WRITER_EN)) setNodeCons(port->get_member(WRITER_EN), "0");
          if (this != port->get_member(WRITER_MASK)) setNodeCons(port->get_member(WRITER_MASK), "0");
        }
      }
      recomputeAllNodes();
    }
  } else if (type == NODE_REG_DST && assignTree.size() == 1 && ret->sameConstant &&
    (getSrc()->assignTree.size() == 0 || (getSrc()->status == CONSTANT_NODE && mpz_cmp(consMap[getSrc()]->consVal, ret->assignmentCons) == 0))
    && cons_resetConsEq(ret, getSrc())) {
    ret->status = VAL_CONSTANT;
    mpz_set(ret->consVal, ret->assignmentCons);
    ret->setConsStr();
    status = CONSTANT_NODE;
    getSrc()->status = CONSTANT_NODE;
    consMap[getSrc()] = ret;
    for (Node* next : (regSplit ? getSrc() : this)->next) {
      if (consMap.find(next) != consMap.end()) {
        addRecompute(next);
      }
    }
    recomputeAllNodes();
  }
  ret->width = width;
  ret->sign = sign;
  consMap[this] = ret;
  return ret;
}

bool isConsZero(ENode* enode) {
  if (consEMap.find(enode) == consEMap.end()) return false;
  if (consEMap[enode]->status != VAL_CONSTANT) return false;
  if (mpz_sgn(consEMap[enode]->consVal) == 0) return true;
  return false;
}

void ExpTree::updateNewChild(ENode* parent, ENode* child, int idx) {
  if (parent) parent->child[idx] = child;
  else setRoot(child);
}

void ExpTree::removeConstant() {
  if (consEMap[getRoot()]->status == VAL_CONSTANT) return;  // used for array
  if (!getRoot() || consEMap[getRoot()]->status == VAL_CONSTANT) return;
  std::stack<std::tuple<ENode*, ENode*, int>> s;
  Assert(getRoot(), "emptyRoot");
  s.push(std::make_tuple(getRoot(), nullptr, -1));
  if (getlval()) s.push(std::make_tuple(getlval(), nullptr, -1));
  while (!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    bool remove = false;
    Assert(parent || consEMap.find(top) == consEMap.end() || consEMap[top]->status != VAL_CONSTANT, "constant not removed");
    if (consEMap.find(top) != consEMap.end() && consEMap[top]->status == VAL_CONSTANT) {
      if (parent && parent->opType == OP_INDEX) {
          Assert(parent->child.size() == 1, "opIndex with child %ld\n", top->child.size());
          parent->opType = OP_INDEX_INT;
          parent->values.push_back(mpz_get_ui(consEMap[top]->consVal));
          parent->child.clear();
          continue;
      } else {
        top->nodePtr = nullptr;
        top->opType = OP_INT;
        top->child.clear();
        top->strVal = mpz_get_str(NULL, 10, consEMap[top]->consVal);
      }
    } else if ((top->opType == OP_MUX || top->opType == OP_WHEN) &&
          consEMap.find(top->getChild(0)) != consEMap.end() && consEMap[top->getChild(0)]->status == VAL_CONSTANT) {
      valInfo* info = consEMap[top->getChild(0)];
      if (mpz_cmp_ui(info->consVal, 0) == 0) updateNewChild(parent, top->getChild(2), idx);//parent->child[idx] = top->getChild(2);
      else updateNewChild(parent, top->getChild(1), idx);
      remove = true;
    } else if (top->opType == OP_OR && (isConsZero(top->getChild(0)) || isConsZero(top->getChild(1)))) {
      if (isConsZero(top->getChild(0))) {
        ENode* newChild = top->getChild(1);
        if (top->width > top->getChild(1)->width) {
          newChild = new ENode(OP_PAD);
          newChild->addVal(top->width);
          newChild->width = top->width;
          newChild->addChild(top->getChild(1));
        }
        updateNewChild(parent, newChild, idx);
      } else {
        ENode* newChild = top->getChild(0);
        if (top->width > top->getChild(0)->width) {
          newChild = new ENode(OP_PAD);
          newChild->addVal(top->width);
          newChild->width = top->width;
          newChild->addChild(top->getChild(0));
        }
        updateNewChild(parent, newChild, idx);
      }
      remove = true;
    }

    if (!remove) {
      for (size_t i = 0; i < top->child.size(); i ++) {
        if (top->child[i]) s.push(std::make_tuple(top->child[i], top, i));
      }
    } else {
      ENode* childENode = parent ? parent->getChild(idx) : root;
      if (childENode) s.push(std::make_tuple(childENode, parent, idx));
    }
    if (parent && parent->opType == OP_STMT) {
      parent->child.erase(
      std::remove_if(parent->child.begin(), parent->child.end(), [](ENode* enode){ return enode == nullptr; }),
        parent->child.end()
      );
    }
  }
}

void graph::constantAnalysis() {
  std::set<Node*>s;
  for (SuperNode* super : sortedSuper) {
    for (Node* n : super->member) {
      n->computeConstant();
    }
  }
  constantMemory();

  /* remove constant nodes */
  int consNum = 0;
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == CONSTANT_NODE) {
          consNum ++;
          member->computeInfo = consMap[member];
      }
    }
  }
  // removeNodes(CONSTANT_NODE);

  /* update valTree */
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == CONSTANT_NODE) {
        member->assignTree.clear();
        member->arrayVal.clear();
        ENode* enode = new ENode(OP_INT);
        enode->computeInfo = member->computeInfo;
        enode->width = member->width;
        enode->strVal = mpz_get_str(NULL, 10, member->computeInfo->consVal);
        if (member->isArray()) member->arrayVal.push_back(new ExpTree(enode, member));
        else member->assignTree.push_back(new ExpTree(enode, member));
        continue;
      }
      for (ExpTree* tree : member->arrayVal) {
        tree->removeConstant();
      }
      member->arrayVal.erase(
      std::remove_if(member->arrayVal.begin(), member->arrayVal.end(), [](ExpTree* tree){ return !tree->getRoot(); }),
        member->arrayVal.end()
      );
      for (ExpTree* tree : member->assignTree) {
        tree->removeConstant();
      }
      member->assignTree.erase(
      std::remove_if(member->assignTree.begin(), member->assignTree.end(),
        [](ExpTree* tree){ return !tree->getRoot() || (consEMap.find(tree->getRoot()) != consEMap.end() && consEMap[tree->getRoot()]->status == VAL_CONSTANT); }),
        member->assignTree.end()
      );
    }
  }
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->prev.clear();
      member->next.clear();
    }
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      member->updateConnect();
    }
  }
  removeEmptySuper();
  reconnectSuper();

  size_t optimizeNodes = countNodes();
  printf("[constantNode] find %d constantNodes (total %ld)\n", consNum, optimizeNodes);

}
