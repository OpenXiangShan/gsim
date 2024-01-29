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

static std::string setMpz(std::string dstName, ENode* enode, valInfo* dstInfo) {
  std::string ret;
  if (enode->width > BASIC_WIDTH) {
    ret = format("mpz_set(%s, %s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  } else if (enode->width > 64) {
    std::string localVal = enode->computeInfo->valStr;
    if (enode->computeInfo->opNum > 0) {
      localVal = newLocalTmp();
      dstInfo->insts.push_back(format("%s %s = %s;", widthUType(enode->width).c_str(), localVal.c_str(), enode->computeInfo->valStr.c_str()));
    }
    ret = format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);", dstName.c_str(), localVal.c_str());
  } else {
    ret = format("mpz_set_ui(%s, %s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  }
  return ret;
}

static std::string get128(std::string name) {
  return format("(mpz_size(%s) == 1 ? mpz_get_ui(%s) : \
            ((__uint128_t)mpz_getlimbn(%s, 1) << 64 | mpz_get_ui(%s)))",
              name.c_str(), name.c_str(), name.c_str(), name.c_str());
}

static std::string set128(std::string lvalue, valInfo* info, valInfo* ret) {
  std::string localName = info->valStr;
  if (info->opNum > 0) {
    localName = newLocalTmp();
    ret->insts.push_back(format("%s %s = %s;", widthUType(info->width).c_str(), localName.c_str(), info->valStr.c_str()));
  }
  return format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);\n", lvalue.c_str(), localName.c_str());
}

static bool isSubArray(std::string name, Node* node) {
  size_t count = std::count(name.begin(), name.end(), '[');
  Assert(node->type == NODE_ARRAY_MEMBER || count <= node->dimension.size(), "invalid array %s", name.c_str());
  return node->dimension.size() != count;
}

valInfo* ENode::instsMux(Node* node, std::string lvalue, bool isRoot) {
  /* cond is constant */
  if (ChildInfo(0, status) == VAL_CONSTANT) {
    if (mpz_cmp_ui(ChildInfo(0, consVal), 0) == 0) computeInfo = getChild(2)->computeInfo;
    else computeInfo = getChild(1)->computeInfo;
    return computeInfo;
  }
  if (ChildInfo(1, status) == VAL_CONSTANT && ChildInfo(2, status) == VAL_CONSTANT) {
    if (ChildInfo(1, valStr) == ChildInfo(2, valStr)) {
      computeInfo = getChild(1)->computeInfo;
      return computeInfo;
    }
  }

  /* not constant */
  bool childBasic = Child(1, width) <= BASIC_WIDTH && Child(2, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = computeInfo;
  ret->opNum = ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + ChildInfo(1, valStr) + " : " + ChildInfo(2, valStr) + ")";
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string trueAssign = setMpz(dstName, getChild(1), ret);
    std::string falseAssign = setMpz(dstName, getChild(2), ret);
    ret->insts.push_back(format("if (%s) %s else %s", ChildInfo(0, valStr).c_str(), trueAssign.c_str(), falseAssign.c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (enodeBasic && !childBasic) {
    if (width <= 64) {
      std::string lname = Child(1, width) <= 64 ? ChildInfo(1, valStr) : format("mpz_get_ui(%s)", ChildInfo(1, valStr).c_str());
      std::string rname = Child(2, width) <= 64 ? ChildInfo(2, valStr) : format("mpz_get_ui(%s)", ChildInfo(2, valStr).c_str());
      ret->valStr = format("(%s ? %s : %s)", ChildInfo(0, valStr).c_str(), lname.c_str(), rname.c_str());
    } else {
      TODO();
    }
  } else {
    printf("width %d = mux %s(%d) %s(%d)\n", width, ChildInfo(1, valStr).c_str(), Child(1, width), ChildInfo(2, valStr).c_str(), Child(2, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsWhen(Node* node, std::string lvalue, bool isRoot) {
  /* cond is constant */
  if (getChild(0)->computeInfo->status == VAL_CONSTANT) {
    if (mpz_cmp_ui(ChildInfo(0, consVal), 0) == 0) {
      if (getChild(2)) computeInfo = Child(2, computeInfo);
      else computeInfo->status = VAL_INVALID;
    }
    else {
      if (getChild(1)) computeInfo = Child(1, computeInfo);
      else computeInfo->status = VAL_INVALID;
    }
    return computeInfo;
  }
  if (getChild(1) && ChildInfo(1, status) == VAL_CONSTANT && getChild(2) && ChildInfo(2, status) == VAL_CONSTANT) {
    if (ChildInfo(1, valStr) == ChildInfo(2, valStr)) {
      computeInfo = getChild(1)->computeInfo;
      return computeInfo;
    }
  }

  if (getChild(1) && ChildInfo(1, status) == VAL_INVALID) {
    if (getChild(2)) computeInfo = Child(2, computeInfo);
    else computeInfo->status = VAL_INVALID;
    return computeInfo;
  }
  if (getChild(2) && ChildInfo(2, status) == VAL_INVALID) {
    if (getChild(1)) computeInfo = Child(1, computeInfo);
    else computeInfo->status = VAL_INVALID;
    return computeInfo;
  }
  /* not constant */
  bool childBasic = (!getChild(1) || Child(1, width) <= BASIC_WIDTH) && (!getChild(2) || Child(2, width) <= BASIC_WIDTH);
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = computeInfo;
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
  } else if (!childBasic && !enodeBasic) { // can merge into childBasic && enodeBasic
    auto assignmentMpz = [lvalue, node, ret](bool isStmt, std::string expr, int width, bool sign, valInfo* info) {
      if (isStmt) return expr;
      if (expr.length() == 0) return std::string("");
      else if (isSubArray(lvalue, node)) TODO();
      else if (width <= 64) return format(sign ? "mpz_set_si(%s, %s);" : "mpz_set_ui(%s, %s);", lvalue.c_str(), expr.c_str());
      else if (info->status == VAL_CONSTANT && info->consLength <= 16) return format(sign ? "mpz_set_si(%s, %s);" : "mpz_set_ui(%s, %s);", lvalue.c_str(), expr.c_str());
      else if (info->status == VAL_CONSTANT) TODO();
      else if (width <= BASIC_WIDTH) return set128(lvalue, info, ret);
      return format("mpz_set(%s, %s);", lvalue.c_str(), expr.c_str());
    };
    std::string condStr = format("if(%s)", ChildInfo(0, valStr).c_str());
    std::string trueStr = format("{ %s }", (getChild(1) ? assignmentMpz(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr), Child(1, width), Child(1, sign), Child(1, computeInfo)) : "").c_str());
    std::string falseStr = format("else { %s }", (getChild(2) ? assignmentMpz(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr), Child(2, width), Child(2, sign), Child(2, computeInfo)) : "").c_str());
    ret->valStr = condStr + trueStr + falseStr;
    ret->opNum = -1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAdd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH && Child(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_add(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " + " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (enodeBasic) {
    TODO();
  } else if (!childBasic && enodeBasic) {
    TODO();
  } else {
    Panic();
  }
  return ret;
}

valInfo* ENode::instsSub(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_sub(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " - " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsMul(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_mul(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " * " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string lname = newMpzTmp();
    std::string rname = newMpzTmp();
    /* assign left & right values to mpz variables */
    if (Child(0, width) > 64) {
      std::string localVal = ChildInfo(0, valStr);
      if (ChildInfo(0, opNum) > 0) {
        localVal = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(Child(0, width)).c_str(), localVal.c_str(), ChildInfo(0, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);\n", lname.c_str(), localVal.c_str()));
    } else {
      std::string funcName = sign ? "mpz_set_si" : "mpz_set_ui";
      ret->insts.push_back(format("%s(%s, %s);", funcName.c_str(), lname.c_str(), ChildInfo(0, valStr).c_str()));
    }
    if (Child(1, width) > 64) {
      std::string localVal = ChildInfo(0, valStr);
      if (ChildInfo(1, opNum) > 0) {
        localVal = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(Child(1, width)).c_str(), localVal.c_str(), ChildInfo(1, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);\n", rname.c_str(), localVal.c_str()));
    } else {
      std::string funcName = sign ? "mpz_set_si" : "mpz_set_ui";
      ret->insts.push_back(format("%s(%s, %s);", funcName.c_str(), rname.c_str(), ChildInfo(1, valStr).c_str()));
    }
    ret->insts.push_back(format("mpz_mul(%s, %s, %s);", dstName.c_str(), lname.c_str(), rname.c_str()));
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDIv(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_div(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " / " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsRem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_rem(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign)+ ChildInfo(0, valStr) + " % " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsLt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_lt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_leq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_gt(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_geq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_eq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_neq(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    ret->valStr = format("(mpz_cmp(%s, %s) != 0)", ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str());
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
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
      std::string midName = newMpzTmp();
      ret->insts.push_back(format("mpz_set_ui(%s, %s);", midName.c_str(), ChildInfo(0, valStr).c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
    ret->valStr = ChildInfo(0, valStr);
    ret->opNum = ChildInfo(0, opNum);
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + Cast(Child(0, width), Child(0, sign)) + ChildInfo(0, valStr) + " >> " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    if (Child(1, width) > 64) TODO();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (enodeBasic && !childBasic) {
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %s);", tmp.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    if (width <= 64) {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", tmp.c_str(), bitMask(width).c_str());
      ret->opNum = 1;
    } else TODO();
  } else {
    printf("%d = %s(%d) >> %s(%d)\n", width, ChildInfo(0, valStr).c_str(), Child(0, width), ChildInfo(1, valStr).c_str(), Child(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAnd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_and(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " & " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    if (width <= 64) {
      std::string lname = Child(0, width) <= 64 ? ChildInfo(0, valStr) : format("mpz_get_ui(%s)", ChildInfo(0, valStr).c_str());
      std::string rname = Child(1, width) <= 64 ? ChildInfo(1, valStr) : format("mpz_get_ui(%s)", ChildInfo(1, valStr).c_str());
      ret->valStr = format("(%s & %s)", lname.c_str(), rname.c_str());
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    } else {
      TODO();
    }
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("%d = %s(%d) & %s(%d)\n", width, ChildInfo(0, valStr).c_str(), Child(0, width), ChildInfo(1, valStr).c_str(), Child(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_ior(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " | " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_orr(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("%d = %s(%d) | %s(%d)\n", width, ChildInfo(0, valStr).c_str(), Child(0, width), ChildInfo(1, valStr).c_str(), Child(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXor(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_xor(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH && getChild(1)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_cat(ret->consVal, ChildInfo(0, consVal), Child(0, width), ChildInfo(1, consVal), Child(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    std::string hi = "(" + upperCast(width, Child(0, width), false) + ChildInfo(0, valStr) + " << " + std::to_string(Child(1, width)) + ")";
    ret->valStr = "(" + hi + " | " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBasic) { // child <= 128, cur > 128
    Assert(ChildInfo(0, opNum) >= 0 && ChildInfo(1, opNum) >= 0, "invalid opNum (%d %s) (%d, %s)", ChildInfo(0, opNum), ChildInfo(0, valStr).c_str(), ChildInfo(1, opNum), ChildInfo(1, valStr).c_str());
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string midName = newMpzTmp();
    /* set first value */
    if (Child(0, width) > 64) {
      std::string leftName = ChildInfo(0, valStr);
      if (ChildInfo(0, opNum) > 0) {
        leftName = newLocalTmp();
        ret->insts.push_back(widthUType(Child(0, width)) + " " + leftName + " = " + ChildInfo(0, valStr) + ";");
      }
      /* import size = 2, least significant first, each 8B, host edian */
      std::string setLeftInst = "mpz_import(" + midName + ", 2, -1, 8, 0, 0, (mp_limb_t*)&" + leftName + ");";
      ret->insts.push_back(setLeftInst);
    } else {
      ret->insts.push_back("mpz_set_ui(" + midName + ", " + ChildInfo(0, valStr) + ");");
    }
    /* concat second value*/
    if (Child(1, width) > 64) {
      std::string rightName = ChildInfo(1, valStr);
      if (ChildInfo(1, opNum) > 0) {
        rightName = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(Child(1, width)).c_str(), rightName.c_str(), ChildInfo(1, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), midName.c_str(), Child(1, width) - 64));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s >> 64);", midName.c_str(), midName.c_str(), rightName.c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, 64);", midName.c_str(), midName.c_str()));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), midName.c_str(), rightName.c_str()));
    } else {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), midName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    }
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (!childBasic && !enodeBasic) { // child > 128, cur > 128
    std::string midName = newMpzTmp();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), ChildInfo(0, valStr).c_str(), Child(1, width)));
    if (Child(1, width) <= 64)
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    else
      ret->insts.push_back(format("mpz_add(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsUInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asUInt(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + Cast(width, false) + ChildInfo(0, valStr) + " & " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("if (mpz_sgn(%s) < 0) {\
      mpz_set_ui(%s, 1); \
      mpz_mul_2exp(%s, %s, %d); \
      mpz_add(%s, %s, %s); \
    } else { \
      mpz_set(%s, %s); \
    }", ChildInfo(0, valStr).c_str(),
        tmp.c_str(),
        tmp.c_str(), tmp.c_str(), width,
        dstName.c_str(), ChildInfo(0, valStr).c_str(), tmp.c_str(),
        dstName.c_str(), ChildInfo(0, valStr).c_str()
    ));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("node %s width %d\n", node->name.c_str(), width);
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsSInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    int shiftBits = widthBits(width) - Child(0, width);
    ret->valStr = format("(%s(%s << %d) >> %d)", Cast(width, true).c_str(), ChildInfo(0, valStr).c_str(), shiftBits, shiftBits);
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic){
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("if (mpz_tstbit(%s, %d)){\
      mpz_set_ui(%s, 1); \
      mpz_mul_2exp(%s, %s, %d); \
      mpz_sub(%s, %s, %s); \
    } else {\
      mpz_set(%s, %s); \
    }", ChildInfo(0, valStr).c_str(), width,
        tmp.c_str(),
        tmp.c_str(), tmp.c_str(), width,
        dstName.c_str(), ChildInfo(0, valStr).c_str(), tmp.c_str(),
        dstName.c_str(), ChildInfo(0, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("node %s width %d\n", node->name.c_str(), width);
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsClock(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asClock(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAsAsyncReset(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asAsyncReset(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsCvt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_cvt : u_cvt)(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1; // may keep same ?
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNeg(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_neg(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_not(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string tmpMpz = newMpzTmp();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_set_ui(%s, 1);", tmpMpz.c_str()));
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", tmpMpz.c_str(), tmpMpz.c_str(), Child(0, width)));
    ret->insts.push_back(format("mpz_sub_ui(%s, %s, 1);", tmpMpz.c_str(), tmpMpz.c_str()));
    ret->insts.push_back(format("mpz_xor(%s, %s, %s);", dstName.c_str(), tmpMpz.c_str(), ChildInfo(0, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("%d = ~ %s(%d)\n", width, ChildInfo(0, valStr).c_str(), Child(0, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAndr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_andr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOrr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = getChild(0)->width <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_orr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0 )";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    ret->valStr = format("(mpz_sgn(%s) == 0)", ChildInfo(0, valStr).c_str());
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXorr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_xorr(ret->consVal, ChildInfo(0, consVal), Child(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
    computeInfo = getChild(0)->computeInfo;
    return computeInfo;
  }
  /* SInt padding */
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_pad : u_pad)(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_shl(ret->consVal, ChildInfo(0, consVal), Child(0, width), n);
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, Child(0, width), sign) + ChildInfo(0, valStr) + " << " + std::to_string(n) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsShr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    (sign ? s_shr : u_shr)(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", dstName.c_str(), ChildInfo(0, valStr).c_str(), n));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if(!childBasic && enodeBasic) {
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", tmp.c_str(), ChildInfo(0, valStr).c_str(), n));
    if (width > 64) {
      ret->valStr = get128(tmp);
      ret->opNum = 1;
    } else {
      ret->valStr = format("mpz_get_ui(%s)", tmp.c_str());
      ret->opNum = 1;
    }
  } else {
    printf("%d = %s(%d) >> %d\n", width, ChildInfo(0, valStr).c_str(), Child(0, width), n);
    TODO();
  }
  return ret;
}
/*
  trancate the n least significant bits
  different from tail operantion defined in firrtl spec (changed in inferWidth)
*/
valInfo* ENode::instsHead(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = Child(0, width) - values[0];
  Assert(n > 0, "child width %d is less than current width %d", Child(0, width), values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_head(ret->consVal, ChildInfo(0, consVal), Child(0, width), values[0]);
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_tail(ret->consVal, ChildInfo(0, consVal), Child(0, width), n); // u_tail remains the last n bits
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
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
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = Child(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  
  int hi = values[0];
  int lo = values[1];
  int w = hi - lo + 1;

  if (isConstant) {
    if (sign) TODO();
    u_bits(ret->consVal, ChildInfo(0, consVal), Child(0, width), hi, lo);
    ret->setConsStr();
  } else if (lo >= Child(0, width)) {
    ret->setConstantByStr("0");
  } else if (childBasic && enodeBasic) {
    std::string shift;
    if (Child(0, sign)) {
      shift = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
    }  else {
      shift = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
    }
    ret->valStr = "(" + shift + " & " + bitMask(w) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    std::string mpzTmp1 = newMpzTmp();
    std::string mpzTmp2 = newMpzTmp(); // mask
    std::string shiftVal = mpzTmp1;
    if (lo != 0) {
      ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", mpzTmp1.c_str(), ChildInfo(0, valStr).c_str(), lo));
    } else {
      shiftVal = ChildInfo(0, valStr);
    }
    if (w > 64) {
      ret->insts.push_back(format("mpz_set_ui(%s, 1);", mpzTmp2.c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", mpzTmp2.c_str(), mpzTmp2.c_str(), w));
      ret->insts.push_back(format("mpz_sub_ui(%s, %s, 1);", mpzTmp2.c_str(), mpzTmp2.c_str()));
      ret->insts.push_back(format("mpz_and(%s, %s, %s);", mpzTmp2.c_str(), shiftVal.c_str(), mpzTmp2.c_str()));
      ret->valStr = get128(mpzTmp2);
    } else {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", shiftVal.c_str(), bitMask(w).c_str());
    }
    ret->opNum = 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsIndexInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;

  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = 0;
  ret->valStr = "[" + std::to_string(values[0]) + "]";
  return ret;
}

valInfo* ENode::instsIndex(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);
  
  Assert(width <= BASIC_WIDTH, "index width %d > BASIC_WIDTH", width);
  ret->opNum = ChildInfo(0, opNum);
  ret->valStr = "[" + ChildInfo(0, valStr) + "]";
  return ret;
}

valInfo* ENode::instsInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  std::string str;
  int base;
  std::tie(base, str) = firStrBase(strVal);
  ret->setConstantByStr(str, base);
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsReadMem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  Assert(node->type == NODE_MEM_MEMBER, "invalid type %d", node->type);
  ret->valStr = node->parent->parent->name + "[" + node->parent->get_member(READER_ADDR)->computeInfo->valStr + "]";
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsInvalid(Node* node, std::string lvalue, bool isRoot) {
  computeInfo->status = VAL_INVALID;
  return computeInfo;
}

valInfo* ENode::instsPrintf() {
  valInfo* ret = computeInfo;
  ret->status = VAL_FINISH;
  std::string printfInst = "if(" + ChildInfo(0, valStr) +  ") printf(" + strVal;
  for (int i = 1; i < getChildNum(); i ++) {
    if (Child(i, width) > BASIC_WIDTH) printfInst += ", mpz_get_ui(" + ChildInfo(i, valStr) + ")";
    else printfInst += ", " + ChildInfo(i, valStr);
  }
  printfInst += ");";

  if (ChildInfo(0, status) != VAL_CONSTANT || mpz_cmp_ui(ChildInfo(0, consVal), 0) != 0) {
    ret->insts.push_back(printfInst);
  }

  return ret;
}

valInfo* ENode::instsAssert() {
  valInfo* ret = computeInfo;
  ret->status = VAL_FINISH;
  std::string predStr = ChildInfo(0, valStr);
  std::string enStr = ChildInfo(1, valStr);
  std::string assertInst;
  if (ChildInfo(0, status) == VAL_CONSTANT) { // pred is constant
    if (mpz_cmp_ui(ChildInfo(0, consVal), 0) == 0) {
      if (ChildInfo(1, status) == VAL_CONSTANT) {
        assertInst = mpz_cmp_ui(ChildInfo(1, consVal), 0) == 0? "" : ("Assert(true, " + strVal + ");");
      } else {
        assertInst = "Assert(!" + enStr + ", " + strVal + ");";
      }
    } else { // pred is always satisfied
      assertInst = "";
    }
  } else if (ChildInfo(1, status) == VAL_CONSTANT) { // en is constant pred is not constant
    if (mpz_cmp_ui(ChildInfo(1, consVal), 0) == 0) assertInst = "";
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
    width = computeInfo->width;
    return computeInfo;
  }

  valInfo* ret = new valInfo();
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  switch(opType) {
    case OP_ADD: instsAdd(n, lvalue, isRoot); break;
    case OP_SUB: instsSub(n, lvalue, isRoot); break;
    case OP_MUL: instsMul(n, lvalue, isRoot); break;
    case OP_DIV: instsDIv(n, lvalue, isRoot); break;
    case OP_REM: instsRem(n, lvalue, isRoot); break;
    case OP_LT:  instsLt(n, lvalue, isRoot); break;
    case OP_LEQ: instsLeq(n, lvalue, isRoot); break;
    case OP_GT:  instsGt(n, lvalue, isRoot); break;
    case OP_GEQ: instsGeq(n, lvalue, isRoot); break;
    case OP_EQ:  instsEq(n, lvalue, isRoot); break;
    case OP_NEQ: instsNeq(n, lvalue, isRoot); break;
    case OP_DSHL: instsDshl(n, lvalue, isRoot); break;
    case OP_DSHR: instsDshr(n, lvalue, isRoot); break;
    case OP_AND: instsAnd(n, lvalue, isRoot); break;
    case OP_OR:  instsOr(n, lvalue, isRoot); break;
    case OP_XOR: instsXor(n, lvalue, isRoot); break;
    case OP_CAT: instsCat(n, lvalue, isRoot); break;
    case OP_ASUINT: instsAsUInt(n, lvalue, isRoot); break;
    case OP_ASSINT: instsAsSInt(n, lvalue, isRoot); break;
    case OP_ASCLOCK: instsAsClock(n, lvalue, isRoot); break;
    case OP_ASASYNCRESET: instsAsAsyncReset(n, lvalue, isRoot); break;
    case OP_CVT: instsCvt(n, lvalue, isRoot); break;
    case OP_NEG: instsNeg(n, lvalue, isRoot); break;
    case OP_NOT: instsNot(n, lvalue, isRoot); break;
    case OP_ANDR: instsAndr(n, lvalue, isRoot); break;
    case OP_ORR: instsOrr(n, lvalue, isRoot); break;
    case OP_XORR: instsXorr(n, lvalue, isRoot); break;
    case OP_PAD: instsPad(n, lvalue, isRoot); break;
    case OP_SHL: instsShl(n, lvalue, isRoot); break;
    case OP_SHR: instsShr(n, lvalue, isRoot); break;
    case OP_HEAD: instsHead(n, lvalue, isRoot); break;
    case OP_TAIL: instsTail(n, lvalue, isRoot); break;
    case OP_BITS: instsBits(n, lvalue, isRoot); break;
    case OP_INDEX_INT: instsIndexInt(n, lvalue, isRoot); break;
    case OP_INDEX: instsIndex(n, lvalue, isRoot); break;
    case OP_MUX: instsMux(n, lvalue, isRoot); break;
    case OP_WHEN: instsWhen(n, lvalue, isRoot); break;
    case OP_INT: instsInt(n, lvalue, isRoot); break;
    case OP_READ_MEM: instsReadMem(n, lvalue, isRoot); break;
    case OP_INVALID: instsInvalid(n, lvalue, isRoot); break;
    case OP_PRINTF: instsPrintf(); break;
    case OP_ASSERT: instsAssert(); break;
    default:
      Panic();
  }
  return computeInfo;

}
/*
  compute node
*/
valInfo* Node::compute() {
  if (computeInfo) return computeInfo;
  if (isArray()) return computeArray();

  if (!valTree) {
    computeInfo = new valInfo();
    if (type == NODE_OTHERS) { // invalid nodes
      computeInfo->setConstantByStr("0");
    } else {
      computeInfo->valStr = name;
    }
    computeInfo->width = width;
    computeInfo->sign = sign;
    return computeInfo;
  }
  Assert(valTree && valTree->getRoot(), "empty valTree in node %s", name.c_str());
  bool isRoot = anyExtEdge() || next.size() != 1;
  valInfo* ret = valTree->getRoot()->compute(this, name, isRoot)->dup();
  if (ret->status == VAL_INVALID) ret->setConstantByStr("0");
  if (ret->status == VAL_CONSTANT) {
    status = CONSTANT_NODE;
    if (type == NODE_REG_DST) {
      getSrc()->status = CONSTANT_NODE;
      getSrc()->computeInfo = ret;
      /* re-compute nodes depend on src */
      for (Node* next : (regSplit ? getSrc() : this)->next) {
        if (next->computeInfo)
          next->recompute();
      }
    }
  } else if (isRoot || ret->opNum < 0){
    ret->valStr = name;
    ret->opNum = 0;
  } else {
    ret->valStr = upperCast(width, ret->width, sign) + ret->valStr;
    status = MERGED_NODE;
  }
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  for (std::string inst : valTree->getRoot()->computeInfo->insts) insts.push_back(inst);
  return ret;
}

void ExpTree::clearInfo(){
  std::stack<ENode*> s;
  s.push(getRoot());
  s.push(getlval());

  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (!top) continue;
    top->computeInfo = nullptr;
    for (ENode* child : top->child) s.push(child);
  }
}

void Node::recompute() {
  if (!computeInfo) return;
  valInfo* prevVal = computeInfo;
  computeInfo = nullptr;
  for (ExpTree* tree : arrayVal) {
    if (tree)
      tree->clearInfo();
  }
  insts.clear();
  if (valTree) valTree->clearInfo();
  compute();
  if (prevVal->valStr != computeInfo->valStr) {
    for (Node* nextNode : next) nextNode->recompute();
  }
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
  computeInfo->width = width;
  computeInfo->sign = sign;

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
    if(!tree) continue;
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
  std::set<Node*>s;
  for (SuperNode* super : sortedSuper) {
    localTmpNum = 0;
    mpzTmpNum = 0;
    for (Node* n : super->member) {
      if (n->dimension.size() != 0) {
        n->computeArray();
      } else {
        if (!n->valTree) continue;
        n->compute();
        if (n->status == MERGED_NODE) continue;
        s.insert(n);
      }
    }
    maxTmp = MAX(maxTmp, mpzTmpNum);
  }
  /* generate assignment instrs */
  for (Node* n : s) {
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
  /* remove constant nodes */
  size_t totalNodes = countNodes();
  size_t totalSuper = sortedSuper.size();
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == CONSTANT_NODE) member->removeConnection();
    }
  }
  removeNodes(CONSTANT_NODE);
  size_t optimizeNodes = countNodes();
  size_t optimizeSuper = sortedSuper.size();
  printf("[instGenerator] remove %ld constantNodes (%ld -> %ld)\n", totalNodes - optimizeNodes, totalNodes, optimizeNodes);
  printf("[instGenerator] remove %ld superNodes (%ld -> %ld)\n",  totalSuper - optimizeSuper, totalSuper, optimizeSuper);
}
