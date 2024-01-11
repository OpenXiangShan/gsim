/*
  Generator instructions for every nodes in sortedSuperNodes
  merge constantNode into here
*/

#include <map>
#include <gmp.h>
#include <stack>
#include <tuple>

#include "common.h"
#include "graph.h"
#include "opFuncs.h"

#define Child(id, name) getChild(id)->name
#define ChildInfo(id, name) getChild(id)->computeInfo->name

#define newLocalTmp() ("TMP$" + std::to_string(localTmpNum ++))
#define newMpzTmp() ("MPZ_TMP$" + std::to_string(mpzTmpNum ++))
static int localTmpNum = 0;
static int mpzTmpNum = 0;

static std::map<OPType, std::string> opMap = {
  {OP_ADD, " + "},
  {OP_SUB, " - "},
  {OP_MUL, " * "},
  {OP_DIV, " / "},
  {OP_REM, " % "},
  {OP_LT,  " < "},
  {OP_LEQ, " <= "},
  {OP_GT,  " > "},
  {OP_GEQ, " >= "},
  {OP_EQ,  " == "},
  {OP_NEQ, " != "},
  {OP_DSHL, " >> "},
  {OP_AND, " & "},
  {OP_OR,  " | "},
  {OP_XOR, " ^ "},
  {OP_ASCLOCK, "0 != "},
  {OP_NEG, " - "},
  {OP_CVT, ""},
  {OP_ORR, "0 != "},
  {OP_SHL, " << "},
  {OP_SHR, " >> "},
};

/* return: 0 - same size, positive - the first is larger, negative - the second is larger  */
static int typeCmp(int width1, int width2) {
  if (width1 <= 8 && width2 <= 8) return 0;
  int bits1 = upperLog2(width1);
  int bits2 = upperLog2(width2);
  return bits1 - bits2;
}

static std::string upperCast(int width1, int width2, bool sign) {
  if (typeCmp(width1, width2) > 0) { 
    return Cast(width1, sign);
  }
  return "";
}

static std::string bitMask(int width) {
  Assert(width > 0, "invalid width %d", width);
  std::string ret = std::string(width/4, 'f');
  const char* headTable[] = {"", "1", "3", "7"};
  ret = headTable[width % 4] + ret;
  if (ret.length() > 16) {
    ret = ("UINT128(0x" + ret.substr(0, ret.length()-16) + ", 0x" + ret.substr(ret.length()-16, 16) + ")");
  } else {
    ret = "0x" + ret;
  }
  return ret;
}

static std::string setMpz(std::string dstName, ENode* enode) {
  std::string ret;
  if (enode->width > BASIC_WIDTH) {
    ret = format("mpz_set(%s, %s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  } else if (enode->width > 64) {
    ret = format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  } else {
    ret = format("mpz_set_ui(%s, %s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  }
  return ret;
}

static bool isSubArray(std::string name, Node* node) {
  size_t count = std::count(name.begin(), name.end(), '[');
  Assert(count <= node->dimension.size(), "invalid array %s", name.c_str());
  return node->dimension.size() != count;
}

valInfo* ENode::instsMux(Node* node, std::string lvalue, bool isRoot) {
  /* cond is constant */
  if (ChildInfo(0, status) == VAL_CONSTANT) {
    if (ChildInfo(0, valStr) == "0x0") return getChild(2)->computeInfo;
    else return getChild(1)->computeInfo;
  }
  /* not constant */
  bool childBasic = Child(1, width) <= BASIC_WIDTH && Child(2, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = new valInfo();
  ret->opNum = ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + ChildInfo(1, valStr) + " : " + ChildInfo(2, valStr) + ")";
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string trueAssign = setMpz(dstName, getChild(1));
    std::string falseAssign = setMpz(dstName, getChild(2));
    ret->insts.push_back(format("if (%s) %s else %s", ChildInfo(0, valStr).c_str(), trueAssign.c_str(), falseAssign.c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsWhen(Node* node, std::string lvalue, bool isRoot) {
  /* cond is constant */
  if (getChild(0)->computeInfo->status == VAL_CONSTANT) {
    if (ChildInfo(0, valStr) == "0x0") return getChild(2) ? Child(2, computeInfo) : new valInfo();
    else return getChild(1) ? getChild(1)->computeInfo : new valInfo();
  }
  /* not constant */
  bool childBasic = (!getChild(1) || Child(1, width) <= BASIC_WIDTH) && (!getChild(2) || Child(2, width) <= BASIC_WIDTH);
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) {
    if (childNode) ret->mergeInsts(childNode->computeInfo);
  }
  // ret->opNum = ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  if (childBasic && enodeBasic) {
    auto assignment = [lvalue, node](bool isStmt, std::string expr, int width, bool sign) {
      if (isStmt) return expr;
      if (expr.length() == 0) return std::string("");
      else if (isSubArray(lvalue, node)) return format("memcpy(%s, %s, sizeof(%s));", lvalue.c_str(), expr.c_str(), lvalue.c_str());
      else if (node->sign && node->width != width) return format("%s = %s%s;", lvalue.c_str(), Cast(width, sign).c_str(), expr.c_str());
      return lvalue + " = " + expr + ";";
    };
    std::string condStr = "if (" + ChildInfo(0, valStr) + ") ";
    std::string trueStr = "{ " + (getChild(1) ? assignment(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr), Child(1, width), Child(1, sign)) : "") + " }";
    std::string falseStr = "else { " + (getChild(2) ? assignment(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr), Child(2, width), Child(2, sign)) : "") + " }";
    ret->valStr = condStr + trueStr + falseStr;
    ret->opNum = -1; // assignment rather than expr
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAdd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH && Child(1, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_add(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " + " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (enodeBaisc) {
    TODO();
  } else if (!childBasic && enodeBaisc) {
    TODO();
  } else {
    Panic();
  }
  return ret;
}

valInfo* ENode::instsSub(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_sub(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " - " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsMul(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_mul(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " * " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDIv(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_div(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " / " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsRem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_rem(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " % " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsLt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_lt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = format("(%s%s < %s%s)", Cast(Child(0, width), Child(0, sign)).c_str(), ChildInfo(0, valStr).c_str(),
                                            Cast(Child(1, width), Child(1, sign)).c_str(), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " < " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsLeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_leq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = format("(%s%s <= %s%s)", Cast(Child(0, width), Child(0, sign)).c_str(), ChildInfo(0, valStr).c_str(),
                                            Cast(Child(1, width), Child(1, sign)).c_str(), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " <= " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsGt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_gt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = format("(%s%s > %s%s)", Cast(Child(0, width), Child(0, sign)).c_str(), ChildInfo(0, valStr).c_str(),
                                            Cast(Child(1, width), Child(1, sign)).c_str(), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " > " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsGeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_geq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = format("(%s%s >= %s%s)", Cast(Child(0, width), Child(0, sign)).c_str(), ChildInfo(0, valStr).c_str(),
                                            Cast(Child(1, width), Child(1, sign)).c_str(), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " >= " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsEq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_eq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && enodeBaisc) {
    if (Child(0, width) > BASIC_WIDTH && Child(1, width) > BASIC_WIDTH) {
      ret->valStr = format("(mpz_cmp(%s, %s) == 0)", ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str());
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    } else {
      TODO();
    }
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_neq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_dshl(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " << " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    if (Child(0, width) < 64 && Child(1, width) < 64) {
      ret->insts.push_back(format("mpz_set_ui(%s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %s);", dstName.c_str(), dstName.c_str(), ChildInfo(1, valStr).c_str()));
      ret->valStr = dstName;
      ret->opNum = 0;
    } else {
      TODO();
    }
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + Cast(Child(0, width), Child(0, sign)) + ChildInfo(0, valStr) + " >> " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAnd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_and(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " & " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_ior(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " | " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXor(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_xor(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBaisc) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    if (Child(0, width) > BASIC_WIDTH && Child(1, width) > BASIC_WIDTH) {
      ret->insts.push_back(format("mpz_xor(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
      ret->valStr = dstName;
      ret->opNum = 0;
    } else {
      TODO();
    }
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsCat(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_cat(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    std::string hi = "(" + upperCast(width, Child(0, width), false) + ChildInfo(0, valStr) + " << " + std::to_string(Child(1, width)) + ")";
    ret->valStr = "(" + hi + " | " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBaisc) { // child <= 128, cur > 128
    Assert(ChildInfo(0, opNum) >= 0 && ChildInfo(1, opNum) >= 0, "invalid opNum (%d %s) (%d, %s)", ChildInfo(0, opNum), ChildInfo(0, valStr).c_str(), ChildInfo(1, opNum), ChildInfo(1, valStr).c_str());
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    /* set first value */
    if (Child(0, width) > 64) {
      std::string leftName = ChildInfo(0, valStr);
      if (ChildInfo(0, opNum) > 0) {
        leftName = newLocalTmp();
        ret->insts.push_back(widthUType(Child(0, width)) + " " + leftName + " = " + ChildInfo(0, valStr) + ";");
      }
      /* import size = 2, least significant first, each 8B, host edian */
      std::string setLeftInst = "mpz_import(" + dstName + ", 2, -1, 8, 0, 0, (mp_limb_t*)&" + leftName + ");";
      ret->insts.push_back(setLeftInst);
    } else {
      ret->insts.push_back("mpz_set_ui(" + dstName + ", " + ChildInfo(0, valStr) + ");");
    }
    /* concat second value*/
    if (Child(1, width) > 64) {
      std::string rightName = ChildInfo(1, valStr);
      if (ChildInfo(1, opNum) > 0) {
        rightName = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(Child(1, width)).c_str(), rightName.c_str(), ChildInfo(1, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), dstName.c_str(), Child(1, width) - 64));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s >> 64);", dstName.c_str(), dstName.c_str(), rightName.c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, 64);", dstName.c_str(), dstName.c_str()));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), dstName.c_str(), rightName.c_str()));
    } else {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), dstName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), dstName.c_str(), ChildInfo(1, valStr).c_str()));
    }
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (!childBasic && !enodeBaisc) { // child > 128, cur > 128
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), ChildInfo(0, valStr).c_str(), Child(1, width)));
    ret->insts.push_back(format("mpz_add(%s, %s, %s);", dstName.c_str(), dstName.c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsUInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asUInt(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + Cast(width, false) + ChildInfo(0, valStr) + " & " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsSInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    int shiftBits = widthBits(width) - Child(0, width);
    ret->valStr = format("(%s(%s << %d) >> %d)", Cast(width, true).c_str(), ChildInfo(0, valStr).c_str(), shiftBits, shiftBits);
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsClock(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asClock(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsSyncReset(Node* node, std::string lvalue, bool isRoot) {
  TODO();
}

valInfo* ENode::instsCvt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_cvt : u_cvt)(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1; // may keep same ?
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNeg(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_neg(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    /* cast explicitly only if type do not match or argument is UInt */
    std::string castStr = (typeCmp(width, Child(0, width)) > 0 || !getChild(0)->sign) ? Cast(width, sign) : "";
    ret->valStr = "(-" + castStr + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNot(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_not(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAndr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_andr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOrr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_orr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0 )";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXorr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_xorr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(__builtin_parity(" + ChildInfo(0, valStr) + ") & 1)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsPad(Node* node, std::string lvalue, bool isRoot) {
  /* no operation for UInt variable */
  if (!sign || (width <= Child(0, width))) {
    return getChild(0)->computeInfo;
  }
  /* SInt padding */
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_pad : u_pad)(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    std::string operandName;
    if (ChildInfo(0, opNum) == 0) operandName = ChildInfo(0, valStr);
    else {
      operandName = newLocalTmp();
      std::string tmp_def = widthType(Child(0, width), sign) + " " + operandName + " = " + ChildInfo(0, valStr) + ";";
      ret->insts.push_back(tmp_def);
    }
    int shiftBits = widthBits(width) - Child(0, width);
    std::string lshiftVal = "(" + Cast(width, true) + operandName + " << " + std::to_string(shiftBits) + ")";
    std::string signExtended = "(" + lshiftVal + " >>" + std::to_string(shiftBits) + ")";
    ret->valStr = "(" + signExtended + " & " + bitMask(width) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsShl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_shl(ret->consVal, ChildInfo(0, consVal), Child(0, width), n);
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " << " + std::to_string(n) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsShr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    (sign ? s_shr : u_shr)(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}
/*
  trancate the n least significant bits
  different from tail operantion defined in firrtl spec (changed in inferWidth)
*/
valInfo* ENode::instsHead(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = Child(0, width) - values[0];
  Assert(n > 0, "child width %d is less than current width %d", Child(0, width), values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_head(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

/*
  remain the n least significant bits
  different from tail operantion defined in firrtl spec (changed in inferWidth)
*/
valInfo* ENode::instsTail(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_tail(ret->consVal, ChildInfo(0, consVal), Child(0, width), n); // u_tail remains the last n bits
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsBits(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  
  int hi = values[0];
  int lo = values[1];

  if (isConstant) {
    if (sign) TODO();
    u_bits(ret->consVal, ChildInfo(0, consVal), Child(0, width), hi, lo);
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    std::string shift;
    if (Child(0, sign)) {
      shift = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
    }  else {
      shift = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
    }
    ret->valStr = "(" + shift + " & " + bitMask(hi - lo + 1) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsIndexInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();

  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = 0;
  ret->valStr = "[" + std::to_string(values[0]) + "]";
  return ret;
}

valInfo* ENode::instsIndex(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);
  
  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = ChildInfo(0, opNum);
  ret->valStr = "[" + ChildInfo(0, valStr) + "]";
  return ret;
}

valInfo* ENode::instsInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  std::string str;
  int base;
  std::tie(base, str) = firStrBase(strVal);
  ret->setConstantByStr(str, base);
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsReadMem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = new valInfo();
  Assert(node->type == NODE_MEM_MEMBER, "invalid type %d", node->type);
  ret->valStr = node->parent->parent->name + "[" + node->parent->get_member(READER_ADDR)->name + "]";
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsPrintf() {
  valInfo* ret = new valInfo();
  ret->status = VAL_FINISH;
  std::string printfInst = "if(" + ChildInfo(0, valStr) +  ") printf(" + strVal;
  for (int i = 1; i < getChildNum(); i ++) {
    if (Child(i, width) > BASIC_WIDTH) printfInst += ", mpz_get_ui(" + ChildInfo(i, valStr) + ")";
    else printfInst += ", " + ChildInfo(i, valStr);
  }
  printfInst += ");";

  if (ChildInfo(0, status) != VAL_CONSTANT || ChildInfo(0, valStr) != "0x0") {
    ret->insts.push_back(printfInst);
  }

  return ret;
}

valInfo* ENode::instsAssert() {
  valInfo* ret = new valInfo();
  ret->status = VAL_FINISH;
  std::string predStr = ChildInfo(0, valStr);
  std::string enStr = ChildInfo(1, valStr);
  std::string assertInst;
  if (ChildInfo(0, status) == VAL_CONSTANT) { // pred is constant
    if (predStr == "0x0") {
      if (ChildInfo(1, status) == VAL_CONSTANT) {
        assertInst = enStr == "0x0" ? "" : ("Assert(true, " + strVal + ");");
      } else {
        assertInst = "Assert(!" + enStr + ", " + strVal + ");";
      }
    } else { // pred is always satisfied
      assertInst = "";
    }
  } else if (ChildInfo(1, status) == VAL_CONSTANT) { // en is constant pred is not constant
    if (enStr == "0x0") assertInst = "";
    else {
      assertInst = "Assert(" + predStr + ", " + strVal + ");";
    }
  } else {
    assertInst = "Assert(!" + enStr + " || " + predStr + ", " + strVal + ");";
  }
  
  if (assertInst.length() != 0) ret->insts.push_back(assertInst);

  return ret;
}

/* compute enode */
valInfo* ENode::compute(Node* n, std::string lvalue, bool isRoot) {
  if (computeInfo) return computeInfo;
  for (ENode* childNode : child) {
    if (childNode) childNode->compute(n, lvalue, false);
  }
  if (nodePtr) {
    computeInfo = nodePtr->compute();
    if (child.size() != 0) {
      valInfo* indexInfo = computeInfo->dup();
      computeInfo = indexInfo;
      for (ENode* childENode : child)
        computeInfo->valStr += childENode->computeInfo->valStr;
    }
    return computeInfo;
  }

  valInfo* ret = nullptr;
  switch(opType) {
    case OP_ADD: ret = instsAdd(n, lvalue, isRoot); break;
    case OP_SUB: ret = instsSub(n, lvalue, isRoot); break;
    case OP_MUL: ret = instsMul(n, lvalue, isRoot); break;
    case OP_DIV: ret = instsDIv(n, lvalue, isRoot); break;
    case OP_REM: ret = instsRem(n, lvalue, isRoot); break;
    case OP_LT:  ret = instsLt(n, lvalue, isRoot); break;
    case OP_LEQ: ret = instsLeq(n, lvalue, isRoot); break;
    case OP_GT:  ret = instsGt(n, lvalue, isRoot); break;
    case OP_GEQ: ret = instsGeq(n, lvalue, isRoot); break;
    case OP_EQ:  ret = instsEq(n, lvalue, isRoot); break;
    case OP_NEQ: ret = instsNeq(n, lvalue, isRoot); break;
    case OP_DSHL: ret = instsDshl(n, lvalue, isRoot); break;
    case OP_DSHR: ret = instsDshr(n, lvalue, isRoot); break;
    case OP_AND: ret = instsAnd(n, lvalue, isRoot); break;
    case OP_OR:  ret = instsOr(n, lvalue, isRoot); break;
    case OP_XOR: ret = instsXor(n, lvalue, isRoot); break;
    case OP_CAT: ret = instsCat(n, lvalue, isRoot); break;
    case OP_ASUINT: ret = instsAsUInt(n, lvalue, isRoot); break;
    case OP_ASSINT: ret = instsAsSInt(n, lvalue, isRoot); break;
    case OP_ASCLOCK: ret = instsAsClock(n, lvalue, isRoot); break;
    case OP_ASASYNCRESET: ret = instsAsSyncReset(n, lvalue, isRoot); break;
    case OP_CVT: ret = instsCvt(n, lvalue, isRoot); break;
    case OP_NEG: ret = instsNeg(n, lvalue, isRoot); break;
    case OP_NOT: ret = instsNot(n, lvalue, isRoot); break;
    case OP_ANDR: ret = instsAndr(n, lvalue, isRoot); break;
    case OP_ORR: ret = instsOrr(n, lvalue, isRoot); break;
    case OP_XORR: ret = instsXorr(n, lvalue, isRoot); break;
    case OP_PAD: ret = instsPad(n, lvalue, isRoot); break;
    case OP_SHL: ret = instsShl(n, lvalue, isRoot); break;
    case OP_SHR: ret = instsShr(n, lvalue, isRoot); break;
    case OP_HEAD: ret = instsHead(n, lvalue, isRoot); break;
    case OP_TAIL: ret = instsTail(n, lvalue, isRoot); break;
    case OP_BITS: ret = instsBits(n, lvalue, isRoot); break;
    case OP_INDEX_INT: ret = instsIndexInt(n, lvalue, isRoot); break;
    case OP_INDEX: ret = instsIndex(n, lvalue, isRoot); break;
    case OP_MUX: ret = instsMux(n, lvalue, isRoot); break;
    case OP_WHEN: ret = instsWhen(n, lvalue, isRoot); break;
    case OP_INT: ret = instsInt(n, lvalue, isRoot); break;
    case OP_READ_MEM: ret = instsReadMem(n, lvalue, isRoot); break;
    case OP_PRINTF: ret = instsPrintf(); break;
    case OP_ASSERT: ret = instsAssert(); break;
    default:
      Panic();
  }
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  return ret;

}
/*
  compute node
*/
valInfo* Node::compute() {
  if (computeInfo) return computeInfo;
  if (isArray()) return computeArray();

  if (!valTree) {
    computeInfo = new valInfo();
    computeInfo->valStr = name;
    return computeInfo;
  }
  Assert(valTree && valTree->getRoot(), "empty valTree in node %s", name.c_str());
  valInfo* ret = valTree->getRoot()->compute(this, name, true)->dup();
  if (ret->status == VAL_CONSTANT) {
    status = CONSTANT_NODE;
  } else {
    ret->valStr = name;
  }
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  for (std::string inst : valTree->getRoot()->computeInfo->insts) insts.push_back(inst);
  return ret;
}

void Node::finialConnect(std::string lvalue, valInfo* info) {
  if (info->valStr.length() == 0) return; // empty, used for when statment with constant condition
  if (info->opNum < 0) {
    insts.push_back(info->valStr);
  } else if (isSubArray(lvalue, this)) {
    if (width <= BASIC_WIDTH)
      insts.push_back(format("memcpy(%s, %s, sizeof(%s));", lvalue.c_str(), info->valStr.c_str(), lvalue.c_str()));
    else
      TODO();
  } else {
    if (width <= BASIC_WIDTH) {
      if (sign && width != info->width) insts.push_back(format("%s = %s%s;", lvalue.c_str(), Cast(info->width, info->sign).c_str(), info->valStr.c_str()));
      else insts.push_back(format("%s = %s;", lvalue.c_str(), info->valStr.c_str()));

    } else
      TODO();
  }

}

valInfo* Node:: computeArray() {
  if (computeInfo) return computeInfo;
  computeInfo = new valInfo();
  computeInfo->valStr = name;

  if (valTree) {
    std::string lvalue = name;
    if (valTree->getlval()) {
      valInfo* lindex = valTree->getlval()->compute(this, "INVALID_STR", false);
      lvalue = lindex->valStr;
    }
    valInfo* info = valTree->getRoot()->compute(this, lvalue, false);
    for (std::string inst : info->insts) insts.push_back(inst);
    finialConnect(lvalue, info);
  }

  if (width > BASIC_WIDTH) TODO();
  for (ExpTree* tree : arrayVal) {
    valInfo* lindex = nullptr;
    if (tree->getlval()) {
      lindex = tree->getlval()->compute(this, "INVALID_STR", false);
      valInfo* info = tree->getRoot()->compute(this, lindex->valStr, false);
      for (std::string inst : info->insts) insts.push_back(inst);
      finialConnect(lindex->valStr, info);
    } else {
      TODO();
    }
  }
  return computeInfo;
}

void graph::instsGenerator() {
  for (SuperNode* super : sortedSuper) {
    localTmpNum = 0;
    mpzTmpNum = 0;
    for (Node* n : super->member) {
      if (n->dimension.size() != 0) {
        n->computeArray();
      } else {
        if (!n->valTree) continue;
        n->compute();
        valInfo* assignInfo = n->valTree->getRoot()->computeInfo;
        if (assignInfo->status == VAL_VALID) {
          if (assignInfo->opNum < 0) {
              n->insts.push_back(assignInfo->valStr);
          } else if (assignInfo->opNum > 0 || assignInfo->valStr != n->name) {
            if (n->width <= BASIC_WIDTH)
              n->insts.push_back(n->name + " = " + assignInfo->valStr + ";");
            else
              n->insts.push_back(format("mpz_set(%s, %s);", n->name.c_str(), assignInfo->valStr.c_str()));
          }
        }
      }
    }
    maxTmp = MAX(maxTmp, mpzTmpNum);
  }
}
