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
static int localTmpNum = 0;

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

valInfo* ENode::computeConstant() {
  TODO();
}


valInfo* ENode::instsMux() {
  /* cond is constant */
  if (ChildInfo(0, status) == VAL_CONSTANT) {
    if (ChildInfo(0, valStr) == "0x0") return getChild(2)->computeInfo;
    else return getChild(1)->computeInfo;
  }
  /* not constant */
  bool childBasic = Child(1, width) <= BASIC_WIDTH || Child(2, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = new valInfo();
  ret->opNum = ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + ChildInfo(1, valStr) + " : " + ChildInfo(2, valStr) + ")";
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsWhen(Node* node) {
  /* cond is constant */
  if (getChild(0)->computeInfo->status == VAL_CONSTANT) {
    if (ChildInfo(0, valStr) == "0x0") return getChild(2) ? Child(2, computeInfo) : new valInfo();
    else return getChild(1) ? getChild(1)->computeInfo : new valInfo();
  }
  /* not constant */
  bool childBasic = Child(1, width) <= BASIC_WIDTH || Child(2, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) {
    if (childNode) ret->mergeInsts(childNode->computeInfo);
  }
  // ret->opNum = ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  if (childBasic && enodeBasic) {
    auto assignment = [node](bool isStmt, std::string expr) {
      return isStmt ? expr : (node->name + " = " + expr + ";");
    };
    std::string condStr = "if (" + ChildInfo(0, valStr) + ") ";
    std::string trueStr = "{ " + (getChild(1) ? assignment(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr)) : "") + " }";
    std::string falseStr = "else { " + (getChild(2) ? assignment(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr)) : "") + " }";
    ret->valStr = condStr + trueStr + falseStr;
    ret->opNum = -1; // assignment rather than expr
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAdd() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH || Child(1, width) <= BASIC_WIDTH;
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

valInfo* ENode::instsSub() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsMul() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsDIv() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsRem() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsLt() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_lt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " < " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsLeq() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_leq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " <= " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsGt() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_gt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " > " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsGeq() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_geq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " >= " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsEq() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_eq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNeq() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsDshl() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_dshl(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " << " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshr() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " >> " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAnd() {
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

valInfo* ENode::instsOr() {
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

valInfo* ENode::instsXor() {
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
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsCat() {
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
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsUInt() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsAsSInt() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
  bool enodeBaisc = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBaisc) {
    std::string shiftStr = std::to_string(widthBits(width) - Child(0, width));
    ret->valStr = "((" + Cast(width, true) + ChildInfo(0, valStr) + " << " + shiftStr + ") >> " + shiftStr + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsClock() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsAsSyncReset() {
  TODO();
}

valInfo* ENode::instsCvt() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsNeg() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsNot() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsAndr() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsOrr() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH || getChild(1)->width <= BASIC_WIDTH;
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

valInfo* ENode::instsXorr() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH || Child(1, width) <= BASIC_WIDTH;
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

valInfo* ENode::instsPad() {
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
      std::string tmp_def = widthType(Child(0, width), sign) + " TMP$" + std::to_string(localTmpNum ++) + " = " + ChildInfo(0, valStr) + ";";
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

valInfo* ENode::instsShl() {
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

valInfo* ENode::instsShr() {
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
valInfo* ENode::instsHead() {
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
valInfo* ENode::instsTail() {
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

valInfo* ENode::instsBits() {
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

valInfo* ENode::instsIndexInt() {
  valInfo* ret = new valInfo();

  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = 0;
  ret->valStr = "[" + std::to_string(values[0]) + "]";
  return ret;
}

valInfo* ENode::instsIndex() {
  valInfo* ret = new valInfo();
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);
  
  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = ChildInfo(0, opNum);
  ret->valStr = "[" + ChildInfo(0, valStr) + "]";
  return ret;
}

valInfo* ENode::instsInt() {
  valInfo* ret = new valInfo();
  std::string str;
  int base;
  std::tie(base, str) = firStrBase(strVal);
  ret->setConstantByStr(str, base);
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsPrintf() {
  valInfo* ret = new valInfo();
  ret->status = VAL_FINISH;
  std::string printfInst = "if(" + ChildInfo(0, valStr) +  ") printf(" + strVal;
  for (int i = 1; i < getChildNum(); i ++) {
    if (Child(i, width) > BASIC_WIDTH) TODO();
    printfInst += ", " + ChildInfo(i, valStr);
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
valInfo* ENode::compute(Node* n) {
  if (computeInfo) return computeInfo;
  for (ENode* childNode : child) {
    if (childNode) childNode->compute(n);
  }
  if (nodePtr) {
    computeInfo = nodePtr->compute();
    if (child.size() != 0) {
      valInfo* indexInfo = computeInfo->dup();
      computeInfo = indexInfo;
      computeInfo->valStr += child[0]->computeInfo->valStr;
    }
    return computeInfo;
  }

  valInfo* ret = nullptr;
  switch(opType) {
    case OP_ADD: ret = instsAdd(); break;
    case OP_SUB: ret = instsSub(); break;
    case OP_MUL: ret = instsMul(); break;
    case OP_DIV: ret = instsDIv(); break;
    case OP_REM: ret = instsRem(); break;
    case OP_LT:  ret = instsLt(); break;
    case OP_LEQ: ret = instsLeq(); break;
    case OP_GT:  ret = instsGt(); break;
    case OP_GEQ: ret = instsGeq(); break;
    case OP_EQ:  ret = instsEq(); break;
    case OP_NEQ: ret = instsNeq(); break;
    case OP_DSHL: ret = instsDshl(); break;
    case OP_DSHR: ret = instsDshr(); break;
    case OP_AND: ret = instsAnd(); break;
    case OP_OR:  ret = instsOr(); break;
    case OP_XOR: ret = instsXor(); break;
    case OP_CAT: ret = instsCat(); break;
    case OP_ASUINT: ret = instsAsUInt(); break;
    case OP_ASSINT: ret = instsAsSInt(); break;
    case OP_ASCLOCK: ret = instsAsClock(); break;
    case OP_ASASYNCRESET: ret = instsAsSyncReset(); break;
    case OP_CVT: ret = instsCvt(); break;
    case OP_NEG: ret = instsNeg(); break;
    case OP_NOT: ret = instsNot(); break;
    case OP_ANDR: ret = instsAndr(); break;
    case OP_ORR: ret = instsOrr(); break;
    case OP_XORR: ret = instsXorr(); break;
    case OP_PAD: ret = instsPad(); break;
    case OP_SHL: ret = instsShl(); break;
    case OP_SHR: ret = instsShr(); break;
    case OP_HEAD: ret = instsHead(); break;
    case OP_TAIL: ret = instsTail(); break;
    case OP_BITS: ret = instsBits(); break;
    case OP_INDEX_INT: ret = instsIndexInt(); break;
    case OP_INDEX: ret = instsIndex(); break;
    case OP_MUX: ret = instsMux(); break;
    case OP_WHEN: ret = instsWhen(n); break;
    case OP_INT: ret = instsInt(); break;
    case OP_PRINTF: ret = instsPrintf(); break;
    case OP_ASSERT: ret = instsAssert(); break;
    default:
      Panic();
  }
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
  valInfo* ret = valTree->getRoot()->compute(this)->dup();
  if (ret->status == VAL_CONSTANT) {
    status = CONSTANT_NODE;
  } else {
    ret->valStr = name;
  }
  computeInfo = ret;
  for (std::string inst : valTree->getRoot()->computeInfo->insts) insts.push_back(inst);
  return ret;
}


valInfo* Node:: computeArray() {
  if (computeInfo) return computeInfo;

  for (ExpTree* tree : arrayVal) {
    valInfo* info = tree->getRoot()->compute(this);
    for (std::string inst : info->insts) insts.push_back(inst);
  }
  computeInfo = new valInfo();
  computeInfo->valStr = name;
  return computeInfo;
}

void graph::instsGenerator() {
  for (SuperNode* super : sortedSuper) {
    localTmpNum = 0;
    for (Node* n : super->member) {
      if (n->dimension.size() != 0) {
        n->computeArray();
      } else {
        if (!n->valTree) continue;
        n->compute();
        valInfo* assignInfo = n->valTree->getRoot()->computeInfo;
        if (n->width <= BASIC_WIDTH) {
          if (assignInfo->status == VAL_VALID) {
            if (assignInfo->opNum < 0) {
              n->insts.push_back(assignInfo->valStr);
            } else if (assignInfo->opNum > 0 || assignInfo->valStr != n->name) {
              n->insts.push_back(n->name + " = " + assignInfo->valStr + ";");
            }
          }
        } else {
          TODO();
        }
      }
    }
  }
}
