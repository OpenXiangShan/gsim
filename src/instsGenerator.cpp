/*
  Generator instructions for every nodes in sortedSuperNodes
  merge constantNode into here
*/

#include <map>
#include <gmp.h>
#include <queue>
#include <stack>
#include <tuple>
#include <format>
#include <utility>

#include "common.h"
#include "graph.h"
#include "opFuncs.h"
#include "util.h"

#define MUX_OPT true

#define Child(id, name) getChild(id)->name
#define ChildInfo(id, name) getChild(id)->computeInfo->name

#define newLocalTmp() ("TMP$" + std::to_string((*localTmpNum) ++))
static int *localTmpNum = nullptr;

#define INVALID_LVALUE "INVALID_STR"
#define IS_INVALID_LVALUE(name) (name == INVALID_LVALUE)

static int countArrayIndex(std::string name);
static bool isSubArray(std::string name, Node* node);
void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);
std::string idx2Str(Node* node, int idx, int dim);
static void recomputeAllNodes();

static int stmtDepth = 0;
int maxConcatNum = 0;

static std::stack<int*> tmpStack;
static void tmp_push() {
  tmpStack.push(localTmpNum);
}

static void tmp_pop() {
  localTmpNum = tmpStack.top();
  tmpStack.pop();
}

static std::priority_queue<Node*, std::vector<Node*>, ordercmp> recomputeQueue;
static std::set<Node*> uniqueRecompute;

static inline bool charInName(char c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

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

std::string legalCppCons(std::string str) {
  if (str.length() <= 16) return "0x" + str;
  int concatNum = (str.length() + 15) / 16;
  std::string ret;
  for (int i = concatNum - 1; i >= 0; i --) {
    ret += "0x" + str.substr(MAX(0, (int)str.length() - (i + 1) * 16), MIN(16, (int)str.length() - i * 16));
    if (i != 0) ret += ", ";
  }
  maxConcatNum = MAX(maxConcatNum, concatNum);
  return std::format("UINT_CONCAT{}({})", concatNum, ret);
}

static std::string addBracket(std::string str) {
  if (str.length() > 0 && str[0] == '(') return str;
  return "(" + str + ")";
}

static std::string getConsStr(mpz_t& val) {
  std::string str = mpz_get_str(NULL, 16, val);
  return legalCppCons(str);
}
bool allOnes(mpz_t& val, int width) {
  mpz_t tmp;
  mpz_init(tmp);
  mpz_set_ui(tmp, 1);
  mpz_mul_2exp(tmp, tmp, width);
  mpz_sub_ui(tmp, tmp, 1);
  return mpz_cmp(val, tmp) == 0;
}
/* return: 0 - same size, positive - the first is larger, negative - the second is larger  */
static int typeCmp(int width1, int width2) {
  int bits1 = widthBits(width1);
  int bits2 = widthBits(width2);
  return bits1 - bits2;
}

static std::string upperCast(int width1, int width2, bool sign) {
  if (typeCmp(width1, width2) > 0) { 
    return Cast(width1, sign);
  }
  return "";
}

static std::string rangeMask(int hi, int lo) {
  return "(" + bitMask(hi +1) +
          shiftBits(lo, ShiftDir::Right) +
          shiftBits(lo, ShiftDir::Left) +
          ")";
}

bool memberValid(valInfo* info, size_t num) {
  if (info->memberInfo.size() != num) return false;
  bool ret = true;
  for (valInfo* member : info->memberInfo) {
    if (!member) ret = false;
  }
  return ret;
}
/* 0 : not all constant; 1 : constant but not equal; 2: equal constant*/
int memberConstant(valInfo* info) {
  int ret = 2;
  mpz_t val;
  mpz_init(val);
  bool isStart = true;
  for (valInfo* member : info->memberInfo) {
    if (member->status != VAL_CONSTANT) return 0; // not all constant
    if (isStart) {
      isStart = false;
      mpz_set(val, member->consVal);
    }
    if (mpz_cmp(val, member->consVal) != 0) ret = 1;
  }
  return ret;
}

static std::string constantAssign(std::string lvalue, int dimIdx, Node* node, valInfo* rinfo) {
  std::string ret;
  if(node->width <= BASIC_WIDTH && mpz_sgn(rinfo->consVal) == 0) {
    ret = std::format("memset({0}, 0, sizeof({0}));", lvalue);
  } else {
    std::string idxStr, bracket;
    for (size_t i = 0; i < node->dimension.size() - dimIdx; i ++) {
      ret += std::format("for(int i{0} = 0; i{0} < {1}; i{0} ++) {{\n", i, node->dimension[i + dimIdx]);
      idxStr += std::format("[i{}]", i);
      bracket += "}\n";
    }
    ret += std::format("{}{} = {};\n", lvalue, idxStr, rinfo->valStr);
    ret += bracket;
  }
  return ret;
}

static std::string arrayCopy(std::string lvalue, Node* node, valInfo* rinfo, int dimIdx = -1) {
  std::string ret;
  int num = 1;
  if (dimIdx < 0) dimIdx = countArrayIndex(lvalue);
  for (size_t i = dimIdx; i < node->dimension.size(); i ++) num *= node->dimension[i];
  if (rinfo->status == VAL_CONSTANT) {
    ret = constantAssign(lvalue, dimIdx, node, rinfo);
  } else if (memberValid(rinfo, num)) {
    int consType = memberConstant(rinfo);
    if (consType == 0 || consType == 1) {
      for (int i = 0; i < num; i ++) {
        valInfo* assignInfo = rinfo->getMemberInfo(i);
        for (std::string inst : assignInfo->insts) ret += inst;
        ret += std::format("{}{} = {};\n", lvalue, idx2Str(node, i, dimIdx), assignInfo->valStr);
      }
    } else if (consType == 2) {
      ret = constantAssign(lvalue, dimIdx, node, rinfo->memberInfo[0]);
    } else Panic();

  } else {
    std::string idxStr, bracket;
    for (size_t i = 0; i < node->dimension.size() - dimIdx; i ++) {
      ret += std::format("for(int i{0} = 0; i{0} < {1}; i{0} ++) {{\n", i, node->dimension[i + dimIdx]);
      idxStr += std::format("[i{}]", i);
      bracket += "}\n";
    }
    ret += std::format("{}{} = {};\n", lvalue, idxStr, rinfo->valStr);
    ret += bracket;
  }
  return ret;
}

static int countArrayIndex(std::string name) {
  int count = 0;
  size_t idx = 0;
  int midSquare = 0;
  for(idx = 0; idx < name.length(); ) {
    if (name[idx] == '[') {
      idx ++;
      while (idx < name.length()) {
        if (name[idx] == '[') midSquare ++;
        if (name[idx] == ']') {
          if (midSquare == 0) {
            count ++;
            break;
          } else {
            midSquare --;
          }
        }
        idx ++;
      }
    }
    idx ++;
  }
  return count;
}

bool resetConsEq(valInfo* dstInfo, Node* regsrc) {
  if (!regsrc->resetTree) return true;
  valInfo* info = regsrc->resetTree->getRoot()->compute(regsrc, regsrc->name.c_str(), true);
  if (info->status == VAL_EMPTY) return true;
  mpz_t consVal;
  mpz_init(consVal);
  mpz_set(consVal, dstInfo->status == VAL_CONSTANT ? dstInfo->consVal : dstInfo->assignmentCons);
  if (info->status == VAL_CONSTANT && mpz_cmp(info->consVal, consVal) == 0) return true;
  if (info->sameConstant && mpz_cmp(info->assignmentCons, consVal) == 0) return true;
  return false;
}

void srcUpdateDst(Node* node) {
  if (node->type == NODE_REG_SRC && node->reset == ASYRESET && node->regSplit && node->getDst()->status == VALID_NODE) {
    std::vector<std::string> newInsts;
    size_t start_pos = 0;
    for (std::string inst : node->insts) {
      std::string replaceStr = node->name;
      std::string newStr = node->getDst()->name;
      while((start_pos = inst.find(replaceStr, start_pos)) != std::string::npos) {
        size_t next_pos = start_pos + replaceStr.length();
        if (next_pos < inst.length() && charInName(inst[next_pos])) { // prefix
          start_pos += replaceStr.length();
        } else {
          inst.replace(start_pos, replaceStr.length(), newStr);
          start_pos += newStr.length();
        }
      }
      newInsts.push_back(inst);
    }
    node->insts.insert(node->insts.end(), newInsts.begin(), newInsts.end());
  }
}

static bool isSubArray(std::string name, Node* node) {
  size_t count = countArrayIndex(name);
  Assert(node->isArrayMember || count <= node->dimension.size(), "invalid array %s in %s", name.c_str(), node->name.c_str());
  if (node->isArrayMember) return false;
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
  if (isSubArray(lvalue, node) || node->width > BASIC_WIDTH) return instsWhen(node, lvalue, isRoot);

  /* not constant */
  valInfo* ret = computeInfo;
  ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  if (width == 1 && !sign) {
    if (ChildInfo(1, status) == VAL_CONSTANT && ChildInfo(2, status) == VAL_CONSTANT) {
      if (mpz_sgn(ChildInfo(1, consVal)) != 0 && mpz_sgn(ChildInfo(2, consVal)) == 0) { // cond ? 1 : 0  = cond
        ret->valStr = ChildInfo(0, valStr);
      } else if (mpz_sgn(ChildInfo(1, consVal)) == 0 && mpz_sgn(ChildInfo(2, consVal)) != 0) { // cond ? 0 : 1  = !cond
        ret->valStr = std::format("(!{})", ChildInfo(0, valStr));
      } else Panic();
    } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
      ret->valStr = std::format("((!{}) & {})", ChildInfo(0, valStr), ChildInfo(2, valStr));
    } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) != 0) {
      ret->valStr = std::format("({} | {})", ChildInfo(0, valStr), ChildInfo(2, valStr));
    } else if (ChildInfo(2, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(2, consVal)) == 0) {
      ret->valStr = std::format("({} & {})", ChildInfo(0, valStr), ChildInfo(1, valStr));
    } else if (ChildInfo(2, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(2, consVal)) != 0) {
      ret->valStr = std::format("((!{}) | {})", ChildInfo(0, valStr), ChildInfo(1, valStr));
    } else {
      if (MUX_OPT) {
        ret->valStr = std::format("(({0} & {1}) | ((!{0}) & {2}))", ChildInfo(0, valStr), ChildInfo(1, valStr), ChildInfo(2, valStr));
      } else {
        std::string trueStr = ChildInfo(1, valStr);
        std::string falseStr = ChildInfo(2, valStr);
        if (ChildInfo(1, status) == VAL_CONSTANT) trueStr = std::format("{}{}", Cast(width, sign), trueStr);
        if (ChildInfo(2, status) == VAL_CONSTANT) falseStr = std::format("{}{}", Cast(width, sign), falseStr);
        ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + trueStr + " : " + falseStr + ")";
      }
    }
  } else {
    if (MUX_OPT && ChildInfo(0, opNum) <= 1 && ChildInfo(1, opNum) <= 1  && ChildInfo(2, opNum) <= 1 && width <= BASIC_WIDTH) {
      ret->valStr = std::format("((-({3}){0} & {1}) | ((-({3})!{0}) & {2}))", ChildInfo(0, valStr), ChildInfo(1, valStr), ChildInfo(2, valStr), widthUType(width));
    } else {
      std::string trueStr = ChildInfo(1, valStr);
      std::string falseStr = ChildInfo(2, valStr);
      if (ChildInfo(1, status) == VAL_CONSTANT) trueStr = std::format("{}{}", Cast(width, sign), trueStr);
      if (ChildInfo(2, status) == VAL_CONSTANT) falseStr = std::format("{}{}", Cast(width, sign), falseStr);
      ret->valStr = std::format("({} ? {} : {})", ChildInfo(0, valStr), trueStr, falseStr);
    }
  }

  return ret;
}

std::string idx2Str(Node* node, int idx, int dim) {
  std::string ret;
  for (int i = node->dimension.size() - 1; i >= dim; i --) {
    ret = "[" + std::to_string(idx % node->dimension[i]) + "]" + ret;
    idx /= node->dimension[i];
  }
  return ret;
}

std::string setCons(std::string lvalue, int width, valInfo* info) {
  Assert(info->status == VAL_CONSTANT, "%s expect constant", lvalue.c_str());
  std::string ret;
  ret = std::format("{} = {};\n", lvalue, info->valStr);
  return ret;
}

valInfo* ENode::instsWhen(Node* node, std::string lvalue, bool isRoot) {
  /* cond is constant */
  if (getChild(0)->computeInfo->status == VAL_CONSTANT) {
    if (mpz_cmp_ui(ChildInfo(0, consVal), 0) == 0) {
      if (getChild(2)) computeInfo = Child(2, computeInfo);
      else computeInfo->status = VAL_EMPTY;
    }
    else {
      if (getChild(1)) computeInfo = Child(1, computeInfo);
      else computeInfo->status = VAL_EMPTY;
    }
    return computeInfo;
  }
  if (getChild(1) && ChildInfo(1, status) == VAL_CONSTANT && getChild(2) && ChildInfo(2, status) == VAL_CONSTANT) {
    if (ChildInfo(1, valStr) == ChildInfo(2, valStr)) {
      computeInfo = getChild(1)->computeInfo;
      return computeInfo;
    }
  }

  if (getChild(1) && ChildInfo(1, status) == VAL_INVALID && (node->type == NODE_OTHERS || node->type == NODE_MEM_MEMBER)) {
    if (getChild(2)) computeInfo = Child(2, computeInfo);
    else computeInfo->status = VAL_EMPTY;
    return computeInfo;
  }
  if (getChild(2) && ChildInfo(2, status) == VAL_INVALID && (node->type == NODE_OTHERS || node->type == NODE_MEM_MEMBER)) {
    if (getChild(1)) computeInfo = Child(1, computeInfo);
    else computeInfo->status = VAL_EMPTY;
    return computeInfo;
  }

  /* not constant */
  bool childBasic = (!getChild(1) || ChildInfo(1, width) <= BASIC_WIDTH) && (!getChild(2) || ChildInfo(2, width) <= BASIC_WIDTH);
  bool enodeBasic = node->width <= BASIC_WIDTH;
  valInfo* ret = computeInfo;

  if (childBasic && enodeBasic && getChild(1) && ChildInfo(1, opNum) >= 0 && (ChildInfo(1, status) == VAL_CONSTANT || ChildInfo(1, status) == VAL_VALID) &&
    getChild(2) && ChildInfo(2, opNum) >= 0 && (ChildInfo(2, status) == VAL_CONSTANT || ChildInfo(2, status) == VAL_VALID) && !isSubArray(lvalue, node))
      return instsMux(node, lvalue, isRoot);

  ret->mergeInsts(getChild(0)->computeInfo);

  bool toMux = false;
  std::string condStr, trueStr, falseStr;
  auto assignment = [lvalue, node](bool isStmt, std::string expr, int width, bool sign, valInfo* info) {
    if (isStmt) return expr;
    if (expr.length() == 0 || expr == lvalue) return std::string("");
    else if (isSubArray(lvalue, node)) return arrayCopy(lvalue, node, info);
    else if (node->width < width) return std::format("{} = ({} & {});", lvalue, expr, bitMask(node->width));
    else if (node->sign && node->width != width) return std::format("{} = {}{};", lvalue, Cast(width, sign), expr);
    return std::format("{} = {};", lvalue, expr);
  };
  if (!isSubArray(lvalue, node) && node->assignTree.size() == 1 && stmtDepth == 0 && isRoot && node->width <= 128) {
    if (!getChild(1) && getChild(2) && ChildInfo(2, opNum) >= 0 && ChildInfo(2, opNum) < 1 && ChildInfo(2, insts).size() == 0 && ChildInfo(2, status) != VAL_INVALID) {
      toMux = true;
      condStr = ChildInfo(0, valStr);
      trueStr = lvalue;
      falseStr = ChildInfo(2, valStr);
    } else if (!getChild(2) && getChild(1) && ChildInfo(1, opNum) >= 0 && ChildInfo(1, opNum) < 1 && ChildInfo(1, insts).size() == 0 && ChildInfo(1, status) != VAL_INVALID) {
      toMux = true;
      condStr = ChildInfo(0, valStr);
      trueStr = ChildInfo(1, valStr);
      falseStr = lvalue;
    }
  }
  if (!toMux) {
    condStr = ChildInfo(0, valStr);
    trueStr = ((getChild(1) && ChildInfo(1, status) != VAL_INVALID) ?
                  assignment(ChildInfo(1, type) == TYPE_STMT, ChildInfo(1, valStr), ChildInfo(1, width), Child(1, sign), Child(1, computeInfo)) : "");
    falseStr = ((getChild(2) && ChildInfo(2, status) != VAL_INVALID) ? assignment(ChildInfo(2, type) == TYPE_STMT, ChildInfo(2, valStr), ChildInfo(2, width), Child(2, sign), Child(2, computeInfo)) : "");
  }

  if (toMux) {
    if (MUX_OPT && !sign) {
      if (width == 1) ret->valStr = std::format("(({} & {}) | ((!{}) & {}))", condStr, trueStr, condStr, falseStr);
      else ret->valStr = std::format("((-({3}){0} & {1}) | ((-({3})!{0}) & {2}))", condStr, trueStr, falseStr, widthUType(width));
    } else {
      ret->valStr = std::format(" ({} ? {} : {})", condStr, trueStr, falseStr);
    }
    ret->opNum = ChildInfo(0, opNum) + (getChild(1) ? ChildInfo(1, opNum) : 0) + (getChild(2) ? ChildInfo(2, opNum) : 0) + 1;
  } else {
    if (getChild(1)) {
      std::string trueInst;
      for (std::string str : getChild(1)->computeInfo->insts) trueInst += str;
      trueStr = trueInst + trueStr;
      getChild(1)->computeInfo->insts.clear();
    }
    if (getChild(2)) {
      std::string falseInst;
      for (std::string str : getChild(2)->computeInfo->insts) falseInst += str;
      falseStr = falseInst + falseStr;
      getChild(2)->computeInfo->insts.clear();
    }

    ret->valStr = std::format("if {} {{ {} }} else {{ {} }}", addBracket(condStr), trueStr, falseStr);
    ret->opNum = -1;
    ret->type = TYPE_STMT;
    ret->directUpdate = false;
    if (trueStr.length() == 0 || falseStr.length() == 0 || !ChildInfo(1, fullyUpdated) || !ChildInfo(2, fullyUpdated)) ret->fullyUpdated = false;
  }

  /* update sameConstant and assignCons*/
  if ((!getChild(1) || ChildInfo(1, status) == VAL_EMPTY) && getChild(2)) {
    if (ChildInfo(2, sameConstant)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildInfo(2, assignmentCons));
    }
    if (isSubArray(lvalue, node)) {
      for (int i = ChildInfo(2, beg); i <= ChildInfo(2, end); i ++) {
        if (ChildInfo(2, getMemberInfo(i)) && ChildInfo(2, getMemberInfo(i))->status == VAL_CONSTANT) {
          valInfo* info = new valInfo();
          info->sameConstant = true;
          mpz_set(info->assignmentCons, ChildInfo(2, getMemberInfo(i))->consVal);
          ret->memberInfo.push_back(info);
        } else {
          ret->memberInfo.push_back(nullptr);
        }
      }
    }
  } else if (getChild(1) && (!getChild(2) || ChildInfo(2, status) == VAL_EMPTY)) {
    if (ChildInfo(1, sameConstant)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildInfo(1, assignmentCons));
    }
    if (isSubArray(lvalue, node) && node->type == NODE_REG_DST) {
      for (int i = ChildInfo(1, beg); i <= ChildInfo(1, end); i ++) {
        if (ChildInfo(1, getMemberInfo(i)) && ChildInfo(1, getMemberInfo(i))->status == VAL_CONSTANT) {
          valInfo* info = new valInfo();
          info->sameConstant = true;
          mpz_set(info->assignmentCons, ChildInfo(1, getMemberInfo(i))->consVal);
          ret->memberInfo.push_back(info);
        } else {
          ret->memberInfo.push_back(nullptr);
        }
      }
    }
  } else if (getChild(1) && getChild(2)) {
    if (ChildInfo(1, sameConstant) && ChildInfo(2, sameConstant) && (mpz_cmp(ChildInfo(1, assignmentCons), ChildInfo(2, assignmentCons)) == 0)) {
      ret->sameConstant = true;
      mpz_set(ret->assignmentCons, ChildInfo(1, assignmentCons));
    }
    if (isSubArray(lvalue, node) && node->type == NODE_REG_DST) {
      for (int i = ChildInfo(1, beg); i <= ChildInfo(1, end); i ++) {
        if (ChildInfo(1, getMemberInfo(i)) && ChildInfo(1, getMemberInfo(i))->status == VAL_CONSTANT &&
          ChildInfo(2, getMemberInfo(i)) && ChildInfo(2, getMemberInfo(i))->status == VAL_CONSTANT &&
          mpz_cmp(ChildInfo(1, getMemberInfo(i))->consVal, ChildInfo(2, getMemberInfo(i))->consVal) == 0){
            valInfo* info = new valInfo();
            info->sameConstant = true;
            mpz_set(info->assignmentCons, ChildInfo(1, getMemberInfo(i))->consVal);
            ret->memberInfo.push_back(info);
        } else {
          ret->memberInfo.push_back(nullptr);
        }
      }
    }
  }
  return ret;
}

valInfo* ENode::instsStmt(Node* node, std::string lvalue, bool isRoot) {
  if (getChildNum() == 1) {
    computeInfo = getChild(0)->compute(node, lvalue, isRoot);
    return computeInfo;
  }
  for (ENode* childNode : child) computeInfo->mergeInsts(childNode->computeInfo);
  bool updated = false;
  bool direct = false;
  for (ENode* childENode : child) {
    updated |= childENode->computeInfo->fullyUpdated;
    direct |= childENode->computeInfo->directUpdate;
    if (childENode->computeInfo->status == VAL_INVALID || childENode->computeInfo->status == VAL_EMPTY) continue;
    else if (childENode->computeInfo->type == TYPE_STMT) {
      computeInfo->valStr += childENode->computeInfo->valStr;
    } else if (childENode->computeInfo->type == TYPE_ARRAY) {
      computeInfo->valStr += arrayCopy(lvalue, node, childENode->computeInfo);
    } else {
      if (isSubArray(lvalue, node)) {
        computeInfo->valStr += arrayCopy(lvalue, node, childENode->computeInfo);
      } else if (node->width < width) {
        computeInfo->valStr += std::format("{} = ({} & {});", lvalue, childENode->computeInfo->valStr, bitMask(node->width));
      } else if (node->sign && node->width != width) {
        computeInfo->valStr += std::format("{} = {}{};", lvalue, Cast(width, sign), childENode->computeInfo->valStr);
      }
      computeInfo->valStr += std::format("{} = {}; /* WHY another time? */", lvalue, childENode->computeInfo->valStr);
    }
  }
  computeInfo->opNum = -1;
  computeInfo->type = TYPE_STMT;
  computeInfo->fullyUpdated = updated;
  computeInfo->directUpdate = direct;
  return computeInfo;
}

valInfo* ENode::instsAdd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_add(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    std::string lstr = ChildInfo(0, valStr);
    std::string rstr = ChildInfo(1, valStr);
    int resWidth = width;
    if (sign) { // signed extension
      int width = MAX(resWidth, MAX(ChildInfo(0, width), ChildInfo(1, width)));
      assert(ChildInfo(0, sign));
      assert(ChildInfo(1, sign));

      int lshiftBits = widthBits(width) - ChildInfo(0, width);
      if (lshiftBits == 0 || ChildInfo(0, status) == VAL_CONSTANT)
        lstr = std::format("{}{}{}", Cast(width, false), Cast(ChildInfo(0, width), true), lstr);
      else
        lstr = std::format("{}({}({}{} << {}) >> {})", Cast(width, false), Cast(width, true), Cast(width, false), lstr, lshiftBits, lshiftBits);

      int rshiftBits = widthBits(width) - ChildInfo(1, width);
      if(rshiftBits == 0 || ChildInfo(1, status) == VAL_CONSTANT)
        rstr = std::format("{}{}{}", Cast(width, false), Cast(ChildInfo(1, width), true), rstr);
      else
        rstr = std::format("({}({}({}{} << {}) >> {}))", Cast(width, false), Cast(width, true), Cast(width, false), rstr, rshiftBits, rshiftBits);
    }

    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    if (sign) {
      ret->valStr = "(" + lstr + " + " + rstr + ")";
    } else {
      ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + lstr + " + " + rstr + ")";
      if (width <= MAX(ChildInfo(0, width), ChildInfo(1, width))) {
        ret->valStr = std::format("({} & {})", ret->valStr, bitMask(width));
      }
    }
  }
  return ret;
}

valInfo* ENode::instsSub(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_sub(ret->consVal, ChildInfo(0, consVal), ChildInfo(1, consVal), width);
    ret->setConsStr();
  } else {
    std::string lstr = ChildInfo(0, valStr);
    std::string rstr = ChildInfo(1, valStr);
    if (sign) {
      int width = MAX(ChildInfo(0, width), ChildInfo(1, width));
      int lshiftBits = widthBits(width) - ChildInfo(0, width);
      if (lshiftBits == 0 || ChildInfo(0, status) == VAL_CONSTANT)
        lstr = std::format("{}{}", Cast(ChildInfo(0, width), sign), lstr);
      else
        lstr = std::format("({}({}{} << {}) >> {})", Cast(width, true), Cast(ChildInfo(0, width), false), lstr, lshiftBits, lshiftBits);
      int rshiftBits = widthBits(width) - ChildInfo(1, width);
      if (rshiftBits == 0 || ChildInfo(1, status) == VAL_CONSTANT)
        rstr = std::format("{}{}", Cast(ChildInfo(1, width), sign), rstr);
      else
        rstr = std::format("({}({}{} << {}) >> {})", Cast(width, true), Cast(ChildInfo(1, width), false), rstr, rshiftBits, rshiftBits);
    }
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + lstr + " - " + rstr + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    if (!sign) {
      ret->valStr = std::format("({} & {})", ret->valStr, bitMask(width));
    }
  }
  return ret;
}

valInfo* ENode::instsMul(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_mul(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + " * " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsDIv(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_div(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    ret->valStr = "gdiv(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + ", " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsRem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_rem(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + " % " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsLt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_lt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
  } else {
    if (Child(0, sign)) {
      ret->valStr = std::format("({}{} < {}{})", Cast(Child(0, width), Child(0, sign)), ChildInfo(0, valStr),
                                                      Cast(Child(1, width), Child(1, sign)), ChildInfo(1, valStr));
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " < " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsLeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_leq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
  } else {
    if (Child(0, sign)) {
      ret->valStr = std::format("({}{} <= {}{})", Cast(Child(0, width), Child(0, sign)), ChildInfo(0, valStr),
                                                       Cast(Child(1, width), Child(1, sign)), ChildInfo(1, valStr));
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " <= " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsGt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_gt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
  } else {
    if (Child(0, sign)) {
      ret->valStr = std::format("({}{} > {}{})", Cast(Child(0, width), Child(0, sign)), ChildInfo(0, valStr),
                                                      Cast(Child(1, width), Child(1, sign)), ChildInfo(1, valStr));
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " > " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsGeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_geq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
  } else {
    if (Child(0, sign)) {
      ret->valStr = std::format("({}{} >= {}{})", Cast(Child(0, width), Child(0, sign)), ChildInfo(0, valStr),
                                                       Cast(Child(1, width), Child(1, sign)), ChildInfo(1, valStr));
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " >= " + ChildInfo(1, valStr) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsEq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_eq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
  } else if ((ChildInfo(0, status) == VAL_CONSTANT && mpzOutOfBound(ChildInfo(0, consVal), Child(1, width)))
      ||(ChildInfo(1, status) == VAL_CONSTANT && mpzOutOfBound(ChildInfo(1, consVal), Child(0, width)))) {
    ret->setConstantByStr("0");
  } else {
    if (Child(1, width) == 1 && ChildInfo(0, status) == VAL_CONSTANT && mpz_cmp_ui(ChildInfo(0, consVal), 1) == 0) {
      computeInfo = Child(1, computeInfo);
    } else if (Child(0, width) == 1 && ChildInfo(1, status) == VAL_CONSTANT && mpz_cmp_ui(ChildInfo(1, consVal), 1) == 0) {
      computeInfo = Child(0, computeInfo);
    } else if (Child(0, sign)) {
      ret->valStr = std::format("({}{} == {}{})", (ChildInfo(0, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(0, width), ChildInfo(0, sign))), ChildInfo(0, valStr),
                                                       (ChildInfo(1, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(1, width), ChildInfo(1, sign))), ChildInfo(1, valStr));
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " == " + ChildInfo(1, valStr) + ")";
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  }
  return ret;
}

valInfo* ENode::instsNeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_neq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
  } else {
    if (Child(1, width) == 1 && ChildInfo(0, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(0, consVal)) == 0) {
      computeInfo = Child(1, computeInfo);
    } else if (Child(0, width) == 1 && ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
      computeInfo = Child(0, computeInfo);
    } else if (Child(0, sign)) {
      ret->valStr = std::format("({}{} != {}{})", (ChildInfo(0, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(0, width), ChildInfo(0, sign))), ChildInfo(0, valStr),
                                             (ChildInfo(1, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(1, width), ChildInfo(1, sign))), ChildInfo(1, valStr));
    } else {
      if (ChildInfo(0, typeWidth) >= ChildInfo(1, typeWidth) && ChildInfo(0, status) != VAL_CONSTANT)
        ret->valStr = "(" + ChildInfo(0, valStr) + " != " + ChildInfo(1, valStr) + ")";
      else
        ret->valStr = std::format("({} != {})", ChildInfo(1, valStr), ChildInfo(0, valStr));
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  }
  return ret;
}

valInfo* ENode::instsDshl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_dshl(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    int castWidth = MAX(width, (1 << ChildInfo(1, width)) + 1);
    ret->valStr = std::format("({}({}{}{}) & {})",
                                Cast(width, sign),
                                upperCast(castWidth, ChildInfo(0, width), sign),
                                ChildInfo(0, valStr),
                                shiftBits(ChildInfo(1, valStr), ShiftDir::Left),
                                bitMask(width));
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsDshr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
    if (width == ChildInfo(0, width)) {
      ret->valStr = ChildInfo(0, valStr);
      ret->opNum = ChildInfo(0, opNum);
    } else {
      ret->valStr = std::format("({} & {})", ChildInfo(0, valStr), bitMask(width));
      ret->opNum = ChildInfo(0, opNum) + 1;
    }
  } else {
    ret->valStr = std::format("(({}{} {}) & {})",
                         Cast(ChildInfo(0, width), Child(0, sign)),
                         ChildInfo(0, valStr),
                         shiftBits(ChildInfo(1, valStr), ShiftDir::Right),
                         bitMask(width));
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  if (ChildInfo(0, typeWidth) > BASIC_WIDTH) {
    ret->typeWidth = ChildInfo(0, typeWidth);
  }
  return ret;
}

valInfo* ENode::instsAnd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_and(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if ((ChildInfo(0, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(0, consVal)) == 0) ||
            (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0)) {
          ret->setConstantByStr("0");
  } else if (ChildInfo(0, status) == VAL_CONSTANT && ChildInfo(0, width) == ChildInfo(1, width) && allOnes(ChildInfo(0, consVal), width)) {
    computeInfo = Child(1, computeInfo);
  } else if (ChildInfo(1, status) == VAL_CONSTANT && ChildInfo(0, width) == ChildInfo(1, width) && allOnes(ChildInfo(1, consVal), width)) {
    computeInfo = Child(0, computeInfo);
  } else {
    if (ChildInfo(0, typeWidth) >= ChildInfo(1, typeWidth))
      ret->valStr = "(" + ChildInfo(0, valStr) + " & " + ChildInfo(1, valStr) + ")";
    else
      ret->valStr = "(" + ChildInfo(1, valStr) + " & " + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  if (ChildInfo(0, typeWidth) > BASIC_WIDTH || ChildInfo(1, typeWidth) > BASIC_WIDTH) {
    ret->typeWidth = MAX(ChildInfo(0, typeWidth), ChildInfo(1, typeWidth));
  }
  return ret;
}

valInfo* ENode::instsOr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_ior(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(0, consVal)) == 0) {
    ret->valStr = ChildInfo(1, valStr);
    ret->opNum = ChildInfo(1, opNum);
  } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
    ret->valStr = ChildInfo(0, valStr);
    ret->opNum = ChildInfo(0, opNum);
  } else {
    if (ChildInfo(0, typeWidth) >= ChildInfo(1, typeWidth))
      ret->valStr = "(" + ChildInfo(0, valStr) + " | " + ChildInfo(1, valStr) + ")";
    else
      ret->valStr = "(" + ChildInfo(1, valStr) + " | " + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  if (ChildInfo(0, typeWidth) > BASIC_WIDTH || ChildInfo(1, typeWidth) > BASIC_WIDTH) {
    ret->typeWidth = MAX(ChildInfo(0, typeWidth), ChildInfo(1, typeWidth));
  }
  return ret;
}

valInfo* ENode::instsXor(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_xor(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    if (ChildInfo(0, typeWidth) >= ChildInfo(1, typeWidth))
      ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + ChildInfo(1, valStr) + ")";
    else
      ret->valStr = "(" + ChildInfo(1, valStr) + " ^ " + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  }
  if (ChildInfo(0, typeWidth) > BASIC_WIDTH || ChildInfo(1, typeWidth) > BASIC_WIDTH) {
    ret->typeWidth = MAX(ChildInfo(0, typeWidth), ChildInfo(1, typeWidth));
  }
  return ret;
}

valInfo* ENode::instsCat(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (ChildInfo(0, status) == VAL_CONSTANT) {
    for (valInfo* info : Child(1, computeInfo)->memberInfo) {
      if (info && info->status == VAL_CONSTANT) {
        valInfo* newMemberInfo = new valInfo(width, sign);
        newMemberInfo->status = VAL_CONSTANT;
        u_cat(newMemberInfo->consVal, ChildInfo(0, consVal), ChildInfo(0, width), info->consVal, info->width);
        newMemberInfo->setConsStr();
        ret->memberInfo.push_back(newMemberInfo);
      } else {
        ret->memberInfo.push_back(nullptr);
      }
    }
  }

  if (isConstant) {
    if (sign) TODO();
    u_cat(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else {
    std::string hi;
    if (ChildInfo(0, status) == VAL_CONSTANT) {
      mpz_t cons_hi;
      mpz_init(cons_hi);
      u_shl(cons_hi, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, width));
      if (Child(0, sign)) TODO();
      if (mpz_sgn(cons_hi)) hi = getConsStr(cons_hi);
    } else
      hi = "(" + upperCast(width, ChildInfo(0, width), false) + ChildInfo(0, valStr) + shiftBits(Child(1, width), ShiftDir::Left) + ")";
    if (hi.length() == 0) { // hi is zero
      ret->valStr = upperCast(width, ChildInfo(1, width), ChildInfo(1, sign)) + ChildInfo(1, valStr);
      ret->opNum = ChildInfo(1, opNum) + 1;
    } else {
      if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
        ret->valStr = hi;
      } else {
        ret->valStr = std::format("({} | {}{})", hi, Cast(Child(1, width), false), ChildInfo(1, valStr));
      }
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  }
  return ret;
}

valInfo* ENode::instsAsUInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asUInt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + Cast(width, false) + ChildInfo(0, valStr) + " & " + bitMask(Child(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsAsSInt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    if (Child(0, sign)) ret->valStr = std::format("{}{}{}", Cast(width, true), Cast(ChildInfo(0, width), true), ChildInfo(0, valStr));
    else {
      int shift = widthBits(width) - ChildInfo(0, width);
      if (shift == 0)
        ret->valStr = std::format("({}{})", Cast(width, true), ChildInfo(0, valStr));
      else
        ret->valStr = std::format("({}({}{} {}) {})",
                                 Cast(width, true),
                                 Cast(width, false),
                                 ChildInfo(0, valStr),
                                 shiftBits(shift, ShiftDir::Left),
                                 shiftBits(shift, ShiftDir::Right));
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsAsClock(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asClock(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsAsAsyncReset(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asAsyncReset(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    if (ChildInfo(0, width) == 1) ret->valStr = ChildInfo(0, valStr);
    else ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsCvt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_cvt : u_cvt)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1; // may keep same ?
  }
  return ret;
}

valInfo* ENode::instsNeg(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_neg(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    /* cast explicitly only if type do not match or argument is UInt */
    std::string castStr = (typeCmp(width, ChildInfo(0, width)) > 0 || !getChild(0)->sign) ? Cast(width, sign) : "";
    ret->valStr = "(-" + castStr + ChildInfo(0, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsNot(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_not(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + bitMask(width) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsAndr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_andr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    if (ChildInfo(0, width) <= BASIC_WIDTH)
      ret->valStr = "(" + ChildInfo(0, valStr) + " == " + bitMask(ChildInfo(0, width)) + ")";
    else
      ret->valStr = std::format("{}.allOnes({})", ChildInfo(0, valStr), width);
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsOrr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_orr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else{
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0 )";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsXorr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_xorr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else {
    ret->opNum = ChildInfo(0, opNum) + 1;
    int width = Child(0, width);
    std::string val = ChildInfo(0, valStr);
    if (width > 256) TODO();
    ret->valStr = std::format("(__builtin_parityl({})", val);
    if (Child(0, width) > 64)  ret->valStr += std::format(" ^ __builtin_parityl({} >> 64)", val);
    if (Child(0, width) > 128) ret->valStr += std::format(" ^ __builtin_parityl({} >> 128)", val);
    if (Child(0, width) > 192) ret->valStr += std::format(" ^ __builtin_parityl({} >> 192)", val);
    ret->valStr += ")";
  }
  return ret;
}

valInfo* ENode::instsPad(Node* node, std::string lvalue, bool isRoot) {
  /* no operation for UInt variable */
  if (!sign || (width <= ChildInfo(0, width))) {
    for (ENode* childNode : child) computeInfo->mergeInsts(childNode->computeInfo);
    if (ChildInfo(0, opNum) >= 0 && (int)widthBits(ChildInfo(0, width)) < width) {
      computeInfo->valStr = std::format("(({}){})", widthUType(width), ChildInfo(0, valStr));
      computeInfo->opNum = ChildInfo(0, opNum) + 1;
    } else {
      computeInfo->valStr = ChildInfo(0, valStr);
      computeInfo->opNum = ChildInfo(0, opNum);
    }
    return computeInfo;
  }
  /* SInt padding */
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_pad : u_pad)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else {
    if (width > BASIC_WIDTH) TODO();
    std::string operandName;
    if (ChildInfo(0, opNum) == 0) operandName = ChildInfo(0, valStr);
    else {
      operandName = newLocalTmp();
      std::string tmp_def = widthType(ChildInfo(0, width), sign) + " " + operandName + " = " + ChildInfo(0, valStr) + ";";
      ret->insts.push_back(tmp_def);
    }
    int shift = widthBits(width) - ChildInfo(0, width);
    std::string lshiftVal = "(" + Cast(width, true) + operandName + shiftBits(shift, ShiftDir::Left) + ")";
    std::string signExtended = "(" + lshiftVal + shiftBits(shift, ShiftDir::Right) + ")";
    ret->valStr = "(" + signExtended + " & " + bitMask(width) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsShl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_shl(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n);
    ret->setConsStr();
  } else {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + ChildInfo(0, valStr) + shiftBits(n, ShiftDir::Left)+ ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

valInfo* ENode::instsShr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    (sign ? s_shr : u_shr)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else {
    ret->valStr = "(" + ChildInfo(0, valStr) + shiftBits(n, ShiftDir::Right) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  if (ChildInfo(0, typeWidth) > BASIC_WIDTH) {
    ret->typeWidth = ChildInfo(0, typeWidth);
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

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];
  Assert(n >= 0, "child width %d is less than current width %d", Child(0, width), values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_head(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n);
    ret->setConsStr();
  } else {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + shiftBits(n, ShiftDir::Right) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + shiftBits(n, ShiftDir::Right) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
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

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = MIN(width, values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_tail(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n); // u_tail remains the last n bits
    ret->setConsStr();
  } else {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  }
  return ret;
}

void infoBits(valInfo* ret, ENode* enode, valInfo* childInfo) {
  bool isConstant = childInfo->status == VAL_CONSTANT;

  int hi = enode->values[0];
  int lo = enode->values[1];
  int w = hi - lo + 1;

  if (isConstant) {
    if (enode->sign) TODO();
    u_bits(ret->consVal, childInfo->consVal, childInfo->width, hi, lo);
    ret->setConsStr();
  } else if (lo >= childInfo->width) {
    ret->setConstantByStr("0");
  } else if (childInfo->width <= BASIC_WIDTH && lo == 0 && w == childInfo->width) {
    ret->valStr = childInfo->width;
    ret->opNum = childInfo->width;
  } else {
    std::string shift;
    if (lo == 0) {
      if (childInfo->sign) shift = Cast(childInfo->width, childInfo->sign) + childInfo->valStr;
      else shift = childInfo->valStr;
    } else {
      if (childInfo->sign) shift = "(" + Cast(childInfo->width, childInfo->sign) + childInfo->valStr + shiftBits(lo, ShiftDir::Right) + ")";
      else shift = "(" + childInfo->valStr + shiftBits(lo, ShiftDir::Right) + ")";
    }
    ret->valStr = "(" + shift + " & " + bitMask(w) + ")";
    ret->opNum = childInfo->opNum + 1;
  }
}

valInfo* ENode::instsBits(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  
  int hi = values[0];
  int lo = values[1];
  int w = hi - lo + 1;

  if (isConstant) {
    if (sign) TODO();
    u_bits(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), hi, lo);
    ret->setConsStr();
  } else if (lo >= Child(0, width) || lo >= ChildInfo(0, width)) {
    ret->setConstantByStr("0");
  } else if (ChildInfo(0, width) <= BASIC_WIDTH && lo == 0 && w == ChildInfo(0, width)) {
    ret->valStr = ChildInfo(0, valStr);
    ret->opNum = ChildInfo(0, opNum);
  } else {
    infoBits(ret, this, Child(0, computeInfo));
    for (valInfo* info : ChildInfo(0, memberInfo)) {
      if (info) {
        valInfo* memInfo = info->dup();
        memInfo->insts.insert(memInfo->insts.end(), info->insts.begin(), info->insts.end());
        memInfo->width = ret->width;
        memInfo->sign = ret->sign;
        memInfo->typeWidth = upperPower2(ret->width);
        infoBits(memInfo, this, info);
        ret->memberInfo.push_back(memInfo);
      } else {
        ret->memberInfo.push_back(nullptr);
      }
    }
  }
  return ret;
}

valInfo* ENode::instsBitsNoShift(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  int hi = values[0];
  int lo = values[1];
  int w = hi + 1;

  if (isConstant) {
    if (sign) TODO();
    u_bits_noshift(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), hi, lo);
    ret->setConsStr();
  } else if (lo >= Child(0, width) || lo >= ChildInfo(0, width)) {
    ret->setConstantByStr("0");
  } else if (ChildInfo(0, width) <= BASIC_WIDTH && lo == 0 && w == ChildInfo(0, width)) {
    ret->valStr = ChildInfo(0, valStr);
    ret->opNum = ChildInfo(0, opNum);
  } else {
    ret->valStr = "(" + ChildInfo(0, valStr) + " & " + rangeMask(hi, lo) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
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
  if (sign) {
    s_asSInt(ret->consVal, ret->consVal, width);
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::instsGroup(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  ret->opNum = 0;
  std::string str = std::format("/*WHAT IS THIS*/({}[]){{", widthUType(node->width));
  for (size_t i = 0; i < getChildNum(); i ++) {
    ret->mergeInsts(Child(i, computeInfo));
    if (i != 0) str += ", ";
    str += ChildInfo(i, valStr);
    ret->opNum = MAX(ret->opNum, ChildInfo(i, opNum));
    ret->memberInfo.push_back(Child(i, computeInfo));
  }
  str += "}";
  ret->valStr = str;
  ret->type = TYPE_ARRAY;
  return ret;
}

valInfo* ENode::instsReadMem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  Assert(node->type == NODE_READER || node->type == NODE_READWRITER, "invalid type %d", node->type);
  Node* memory = memoryNode;
  ret->valStr = memory->name + "[" + ChildInfo(0, valStr) + "]";
  for (size_t i = 0; i < memory->dimension.size(); i ++) {
    computeInfo->valStr += "[i" + std::to_string(i) + "]";
  }

  if (memory->width > width) {
    ret->valStr = std::format("({} & {})", ret->valStr, bitMask(width));
  }
  ret->opNum = 0;
  return ret;
}

valInfo* ENode::instsWriteMem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  Assert(node->type == NODE_WRITER || node->type == NODE_READWRITER, "invalid type %d", node->type);
  Node* memory = memoryNode;

  std::string indexStr;
  if (node->isArray()) {
    Assert(lvalue.compare(0, node->name.length(), node->name) == 0, "writer lvalue %s does not start with %s", lvalue.c_str(), node->name.c_str());
    indexStr = lvalue.substr(node->name.length());
  }
  if (isSubArray(lvalue, node)) {
    std::string arraylvalue = std::format("{}[{}]{}", memory->name, ChildInfo(0, valStr), indexStr);
    ret->valStr = arrayCopy(arraylvalue, node, Child(1, computeInfo), countArrayIndex(arraylvalue) - 1);
  } else {
    if (memory->width < width) {
      ret->valStr = std::format("{}[{}]{} = {} & {};", memory->name, ChildInfo(0, valStr), indexStr, ChildInfo(1, valStr), bitMask(memory->width));
    } else {
      ret->valStr = std::format("{}[{}]{} = {};", memory->name, ChildInfo(0, valStr), indexStr, ChildInfo(1, valStr));
    }
  }
  ret->opNum = -1;
  ret->type = TYPE_STMT;
  return ret;
}

valInfo* ENode::instsInvalid(Node* node, std::string lvalue, bool isRoot) {
  computeInfo->status = VAL_INVALID;
  return computeInfo;
}

valInfo* ENode::instsPrintf() {
  valInfo* ret = computeInfo;
  ret->status = VAL_VALID;
  std::string printfInst = std::format("gprintf({}", strVal);
  for (size_t i = 0; i < getChildNum(); i ++) {
    printfInst += ", " + std::to_string(ChildInfo(i, typeWidth));
    printfInst += ", " + ChildInfo(i, valStr);
  }
  printfInst += "); fflush(stdout);";

  ret->insts.push_back(printfInst);
  ret->valStr = printfInst;
  ret->opNum = -1;
  return ret;
}

valInfo* ENode::instsExit() {
  valInfo* ret = computeInfo;
  ret->status = VAL_FINISH;
  std::string exitInst = std::format("if {} {{ exit({}); }}", addBracket(ChildInfo(0, valStr)), strVal);
  if (ChildInfo(0, status) != VAL_CONSTANT || mpz_cmp_ui(ChildInfo(0, consVal), 0) != 0) {
    ret->insts.push_back(exitInst);
  }
  ret->valStr = exitInst;
  ret->opNum = -1;
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
      assertInst = "gAssert(!" + enStr + ", " + strVal + ");";
    } else { // pred is always satisfied
      assertInst = "";
    }
  } else if (ChildInfo(1, status) == VAL_CONSTANT) { // en is constant pred is not constant
    if (mpz_cmp_ui(ChildInfo(1, consVal), 0) == 0) assertInst = "";
    else {
      assertInst = "gAssert(" + predStr + ", " + strVal + ");";
    }
  } else {
    assertInst = "gAssert(!" + enStr + " || " + predStr + ", " + strVal + ");";
  }
  
  if (assertInst.length() != 0) ret->insts.push_back(assertInst);
  ret->valStr = assertInst;
  ret->opNum = -1;
  return ret;
}

valInfo* ENode::instsReset(Node* node, std::string lvalue, bool isRoot) {
  Assert(node->type == NODE_REG_SRC, "node %s is not reg_src\n", node->name.c_str());

  if (ChildInfo(0, status) == VAL_CONSTANT) {
    if (mpz_sgn(ChildInfo(0, consVal)) == 0) {
      computeInfo->status = VAL_EMPTY_SRC;
      return computeInfo;
    } else {
      computeInfo = Child(1, computeInfo);
    }
    return computeInfo;
  }

  valInfo* resetVal = Child(1, computeInfo);
  std::string ret;
  if (node->isArray() && isSubArray(lvalue, node)) {
      ret = arrayCopy(lvalue, node, resetVal);
  } else {
    ret = std::format("{} = {};", lvalue, resetVal->valStr);
  }
  computeInfo->valStr = std::format("if {} {{ {} }}", addBracket(ChildInfo(0, valStr)), ret);
  computeInfo->fullyUpdated = false;
  computeInfo->opNum = -1;
  computeInfo->type = TYPE_STMT;
  computeInfo->sameConstant = resetVal->sameConstant;
  mpz_set(computeInfo->assignmentCons, resetVal->consVal);
  return computeInfo;
}

valInfo* ENode::instsExtFunc(Node* n) {
  valInfo* ret = computeInfo;
  ret->status = VAL_FINISH;
  std::string extInst = n->name + "(";
  for (size_t i = 0; i < getChildNum(); i ++) {
    if (i != 0) extInst += ", ";
    extInst += ChildInfo(i, valStr);
  }
  ret->valStr = extInst;
  return ret;
}

/* compute enode */
valInfo* ENode::compute(Node* n, std::string lvalue, bool isRoot) {
  if (computeInfo) return computeInfo;
  if (width == 0 && !IS_INVALID_LVALUE(lvalue)) {
    computeInfo = new valInfo();
    computeInfo->setConstantByStr("0");
    return computeInfo;
  }
  if (opType == OP_STMT) stmtDepth ++;
  for (ENode* childNode : child) {
    if (childNode) childNode->compute(n, lvalue, false);
  }
  if (opType == OP_STMT) stmtDepth --;
  if (nodePtr) {
    if (nodePtr->isArray() && nodePtr->arraySplitted()) {
      if (getChildNum() < nodePtr->dimension.size()) {
        int beg, end;
        std::tie(beg, end) = getIdx(nodePtr);
        if (beg >= 0 && beg == end) {
          computeInfo = nodePtr->getArrayMember(beg)->compute()->dup();
        } else {
          computeInfo = new valInfo();
          computeInfo->beg = beg;
          computeInfo->end = end;
          computeInfo->width = nodePtr->width;
          computeInfo->sign = nodePtr->sign;
          computeInfo->valStr = nodePtr->name;
          for (ENode* childENode : child)
            computeInfo->valStr += childENode->computeInfo->valStr;
          if (!IS_INVALID_LVALUE(lvalue)) {
            for (size_t i = 0; i < nodePtr->dimension.size() - getChildNum(); i ++) {
              computeInfo->valStr += "[i" + std::to_string(i) + "]";
            }
          }
          computeInfo->opNum = 0;
          if (computeInfo->beg >= 0) {
            for (int i = computeInfo->beg; i <= computeInfo->end; i ++) {
              nodePtr->getArrayMember(i)->compute();
              computeInfo->memberInfo.push_back(nodePtr->getArrayMember(i)->computeInfo);
            }
          }
        }
      } else {
        int idx = getArrayIndex(nodePtr);
        MUX_DEBUG(printf("node %s %s\n", nodePtr->name.c_str(), nodePtr->getArrayMember(idx)->name.c_str()));
        computeInfo = nodePtr->getArrayMember(idx)->compute()->dup();
      }
    } else if (nodePtr->isArray()) {
      computeInfo = nodePtr->compute()->dup();
      if (child.size() != 0 && computeInfo->status == VAL_VALID) { // TODO: constant array representation
        valInfo* indexInfo = computeInfo->dup();  // TODO: no need
        computeInfo = indexInfo;
        for (ENode* childENode : child)
          computeInfo->valStr += childENode->computeInfo->valStr;
      }
      int beg, end;
      std::tie(beg, end) = getIdx(nodePtr);
      computeInfo->beg = beg;
      computeInfo->end = end;
      if (!IS_INVALID_LVALUE(lvalue) && computeInfo->status == VAL_VALID) {
        for (size_t i = 0; i < nodePtr->dimension.size() - getChildNum(); i ++) {
          computeInfo->valStr += "[i" + std::to_string(i) + "]";
        }
      }
    } else {
      if (nodePtr->status == CONSTANT_NODE) {
        computeInfo = nodePtr->compute()->dup();
      } else {
        computeInfo = new valInfo();
        computeInfo->valStr = nodePtr->name;
        computeInfo->width = nodePtr->width;
        computeInfo->sign = nodePtr->sign;
        computeInfo->typeWidth = upperPower2(nodePtr->width);
        if (child.size() != 0) {
          valInfo* indexInfo = computeInfo->dup();
          computeInfo = indexInfo;
          for (ENode* childENode : child)
            computeInfo->valStr += childENode->computeInfo->valStr;
        }
      }
    }

    if (getChildNum() < nodePtr->dimension.size()) {
      if (computeInfo->width > width) computeInfo->width = width;
      /* do nothing */
    } else if (!IS_INVALID_LVALUE(lvalue) && computeInfo->width <= BASIC_WIDTH) {
      if (computeInfo->width > width) {
        if (computeInfo->status == VAL_CONSTANT) {
          computeInfo->width = width;
          computeInfo->updateConsVal();
        }
        else if (width <= BASIC_WIDTH){
          computeInfo->valStr = std::format("({} & {})", computeInfo->valStr, bitMask(width));
          computeInfo->width = width;
        } else {
          computeInfo->valStr = std::format("{}.bits({})", computeInfo->valStr, width);
          computeInfo->width = width;
        }
      }
      if (computeInfo->status == VAL_CONSTANT) ;
      else if (sign && computeInfo->width < width && computeInfo->status == VAL_VALID) { // sign extend
        if (width > BASIC_WIDTH) TODO();
        int extendedWidth = widthBits(width);
        int shift = extendedWidth - computeInfo->width;
        if (extendedWidth != width)
          computeInfo->valStr = std::format("(({}({}{} {}) {}) & {})",
                                            Cast(width, true),
                                            Cast(computeInfo->width, false),
                                            computeInfo->valStr,
                                            shiftBits(shift, ShiftDir::Left),
                                            shiftBits(shift, ShiftDir::Right), bitMask(width));
        else
          computeInfo->valStr = std::format("({}({}{} {}) {})",
                                            Cast(width, true),
                                            Cast(computeInfo->width, false),
                                            computeInfo->valStr,
                                            shiftBits(shift, ShiftDir::Left),
                                            shiftBits(shift, ShiftDir::Right));
        computeInfo->opNum = 1;
        computeInfo->width = width;
      } else if (sign && (nodePtr->nodeIsRoot && computeInfo->width < (int)widthBits(width))) {
        int shift = widthBits(width) - computeInfo->width;
        computeInfo->valStr = std::format("({}({}{} {}) {})",
                                          Cast(width, true),
                                          Cast(computeInfo->width, false),
                                          computeInfo->valStr,
                                          shiftBits(shift, ShiftDir::Left),
                                          shiftBits(shift, ShiftDir::Right));
      } else if (sign) {
        computeInfo->valStr = std::format("({}{})", Cast(width, true), computeInfo->valStr);
      }
    } else if (!IS_INVALID_LVALUE(lvalue)) { // info->width > BASIC_WIDTH
      if (computeInfo->width > width) {
        if (computeInfo->status == VAL_CONSTANT) {
          computeInfo->width = width;
          computeInfo->updateConsVal();
        }
        else {
          computeInfo->valStr = std::format("({} & {})", computeInfo->valStr, bitMask(width));
          computeInfo->width = width;
        }
      }
      computeInfo->opNum = 1;
    }
    if (nodePtr->status == MERGED_NODE && nodePtr->next.size() <= 1) {
      computeInfo->insts.insert(computeInfo->insts.begin(), nodePtr->insts.begin(), nodePtr->insts.end());
    }
    for (ENode* childNode : child) computeInfo->mergeInsts(childNode->computeInfo);
    computeInfo->setFullyUpdated();
    MUX_DEBUG(printf("  %p(node) width %d info->width %d status %d val %s sameConstant %d opNum %d instsNum %ld %p\n", this, width, computeInfo->width, computeInfo->status, computeInfo->valStr.c_str(), computeInfo->sameConstant, computeInfo->opNum, computeInfo->insts.size(), computeInfo));
    for (size_t i = 0; i < computeInfo->memberInfo.size(); i ++) {
      if (computeInfo->memberInfo[i]) {
        MUX_DEBUG(printf("idx %ld %s instNum %ld %p\n", i, computeInfo->memberInfo[i]->valStr.c_str(), computeInfo->memberInfo[i]->insts.size(), computeInfo->memberInfo[i]));
      }
    }
    return computeInfo;
  }

  valInfo* ret = new valInfo(width, sign);
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
    case OP_SEXT:
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
    case OP_BITS_NOSHIFT: instsBitsNoShift(n, lvalue, isRoot); break;
    case OP_INDEX_INT: instsIndexInt(n, lvalue, isRoot); break;
    case OP_INDEX: instsIndex(n, lvalue, isRoot); break;
    case OP_MUX: instsMux(n, lvalue, isRoot); break;
    case OP_WHEN: instsWhen(n, lvalue, isRoot); break;
    case OP_STMT: instsStmt(n, lvalue, isRoot); break;
    case OP_INT: instsInt(n, lvalue, isRoot); break;
    case OP_READ_MEM: instsReadMem(n, lvalue, isRoot); break;
    case OP_WRITE_MEM: instsWriteMem(n, lvalue, isRoot); break;
    case OP_INVALID: instsInvalid(n, lvalue, isRoot); break;
    case OP_RESET: instsReset(n, lvalue, isRoot); break;
    case OP_GROUP: instsGroup(n, lvalue, isRoot); break;
    case OP_PRINTF: instsPrintf(); break;
    case OP_ASSERT: instsAssert(); break;
    case OP_EXT_FUNC: instsExtFunc(n); break;
    case OP_EXIT: instsExit(); break;
    default:
      Assert(0, "invalid opType %d\n", opType);
      Panic();
  }
  MUX_DEBUG(printf("  %p %s op %d width %d val %s status %d sameConstant %d instsNum %ld opNum %d %p\n", this, n ? n->name.c_str() : "null", opType, width, computeInfo->valStr.c_str(), computeInfo->status, computeInfo->sameConstant, computeInfo->insts.size(), computeInfo->opNum, computeInfo));
  return computeInfo;

}
/*
  compute node
*/
valInfo* Node::compute() {
  if (computeInfo) return computeInfo;
  Assert(status != DEAD_NODE, "compute deadNode %s\n", name.c_str());
  if (width == 0) {
    computeInfo = new valInfo();
    computeInfo->setConstantByStr("0");
    status = CONSTANT_NODE;
    return computeInfo;
  }
  tmp_push();
  localTmpNum = &super->localTmpNum;
  MUX_DEBUG(printf("compute %s lcoalTmp %d\n", name.c_str(), *localTmpNum));
  MUX_DEBUG(display());

  if (isArray()) {
    valInfo* ret = computeArray();
    tmp_pop();
    return ret;
  }

  if (assignTree.size() == 0) {
    computeInfo = new valInfo();
    if (type == NODE_OTHERS) { // invalid nodes
      computeInfo->setConstantByStr("0");
    } else {
      computeInfo->valStr = name;
    }
    computeInfo->width = width;
    computeInfo->sign = sign;
    computeInfo->typeWidth = upperPower2(width);
    computeInfo->opNum = 0;
    tmp_pop();
    return computeInfo;
  }

  bool isRoot = nodeIsRoot;
  valInfo* ret = nullptr;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    ExpTree* tree = assignTree[i];
    std::string lvalue = type == NODE_EXT ? INVALID_LVALUE : name;
    valInfo* info = tree->getRoot()->compute(this, lvalue, isRoot);
    if (info->status == VAL_EMPTY || info->status == VAL_INVALID) continue;
    ret = info->dupWithCons();
    if ((ret->status == VAL_INVALID || ret->status == VAL_CONSTANT) && (i < assignTree.size() - 1) ) {
      fillEmptyWhen(assignTree[i+1], tree->getRoot());
    }
  }
  bool updated = false;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    if (assignTree[i]->getRoot()->computeInfo->fullyUpdated) {
      updated = true;
      break;
    }
  }
  for (size_t i = 0; i < assignTree.size(); i ++) ops += assignTree[i]->getRoot()->computeInfo->opNum >= 0 ? assignTree[i]->getRoot()->computeInfo->opNum : 1000;
  fullyUpdated = updated;
  if (!ret) ret = assignTree.back()->getRoot()->compute(this, name, isRoot)->dup();
  Assert(ret, "empty info in %s\n", name.c_str());
  if (ret->status == VAL_INVALID || ret->status == VAL_EMPTY) ret->setConstantByStr("0");
  if (ret->status == VAL_EMPTY_SRC && assignTree.size() == 1) status = DEAD_SRC;
  MUX_DEBUG(printf("compute [%s(%d), %d] = %s width %d info->width %d status %d sameCons %d insts(%ld, %ld) isRoot %d infoType %d %p\n", name.c_str(), type, ret->status, ret->valStr.c_str(), width, ret->width, status, ret->sameConstant, ret->insts.size(), insts.size(), isRoot, ret->type, this));
  bool needRecompute = false;
  if (ret->status == VAL_CONSTANT) {
    status = CONSTANT_NODE;
    if (type == NODE_REG_DST) {
      if ((getSrc()->status == DEAD_SRC || getSrc()->assignTree.size() == 0 || (getSrc()->status == CONSTANT_NODE && mpz_cmp(ret->consVal, getSrc()->computeInfo->consVal) == 0)) && resetConsEq(ret, getSrc())) {
        getSrc()->status = CONSTANT_NODE;
        getSrc()->computeInfo = ret;
        if (getSrc()->regUpdate) {
          getSrc()->regUpdate->computeInfo = ret;
          getSrc()->regUpdate->status = CONSTANT_NODE;
        }
        /* re-compute nodes depend on src */
        for (Node* next : getSrc()->next) {
          if (next->computeInfo) {
            addRecompute(next);
          }
        }
        needRecompute = true;
      }
    } else if (type == NODE_MEM_MEMBER && mpz_sgn(ret->consVal) == 0) {
      Node* port = parent;
      if (port->type == NODE_READER) {
        if (this == port->get_member(READER_EN)) {
          port->get_member(READER_DATA)->setConstantInfoZero();
          port->get_member(READER_ADDR)->setConstantInfoZero();
          for (Node* next : port->get_member(READER_DATA)->next) {
            if (next->computeInfo) addRecompute(next);
          }
        }
      } else if (port->type == NODE_WRITER) {
        if (this == port->get_member(WRITER_EN) || this == port->get_member(WRITER_MASK)) {
          port->get_member(WRITER_ADDR)->setConstantInfoZero();
          port->get_member(WRITER_DATA)->setConstantInfoZero();
          if (this != port->get_member(WRITER_EN)) port->get_member(WRITER_EN)->setConstantInfoZero();
          if (this != port->get_member(WRITER_MASK)) port->get_member(WRITER_MASK)->setConstantInfoZero();
        }
      }
      needRecompute = true;
    }
  } else if (type == NODE_REG_DST && assignTree.size() == 1 && ret->sameConstant &&
    (getSrc()->assignTree.size() == 0 || (getSrc()->status == CONSTANT_NODE && mpz_cmp(getSrc()->computeInfo->consVal, ret->assignmentCons) == 0)) && resetConsEq(ret, getSrc())) {
    ret->status = VAL_CONSTANT;
    mpz_set(ret->consVal, ret->assignmentCons);
    ret->setConsStr();
    status = CONSTANT_NODE;
    getSrc()->status = CONSTANT_NODE;
    getSrc()->computeInfo = ret;
    if (getSrc()->regUpdate) {
      getSrc()->regUpdate->status = CONSTANT_NODE;
      getSrc()->regUpdate->computeInfo = ret;
    }
    for (Node* next : getSrc()->next) {
      if (next->computeInfo) {
        addRecompute(next);
      }
    }
    needRecompute = true;
  } else if (isRoot || assignTree.size() > 1 || ret->type == TYPE_STMT){  // TODO: count validInfoNum, replace assignTree by validInfuNum
    ret->valStr = name;
    ret->opNum = 0;
    ret->typeWidth = upperPower2(ret->width);
    ret->type = TYPE_NORMAL;
    ret->status = VAL_VALID;
  } else {
    if (ret->width > width) {
      ret->valStr = std::format("({} & {})", ret->valStr, bitMask(width));
    }
    ret->valStr = upperCast(width, ret->width, sign) + ret->valStr;
    status = MERGED_NODE;
  }
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  for (ExpTree* tree : assignTree) {
    for (std::string inst : tree->getRoot()->computeInfo->insts) {
      insts.push_back(inst);
      ret->insts.push_back(inst);
    }
  }
  tmp_pop();
  MUX_DEBUG(printf("compute [%s(%d), %d] = %s status %d %p ret=%p retInst=%ld\n", name.c_str(), type, ret->status, ret->valStr.c_str(), status, this, ret, ret->insts.size()));
  if (needRecompute) recomputeAllNodes();
  return ret;
}

void StmtNode::compute(std::vector<InstInfo>& insts) {
  switch (type) {
    case OP_STMT_SEQ:
      for (StmtNode* stmt : child) {
        stmt->compute(insts);
      }
      break;
    case OP_STMT_WHEN: {
      Assert(getChild(0)->isENode, "invalid when condition\n");
      valInfo* cond = getChild(0)->enode->compute(nullptr, INVALID_LVALUE, false);
      insts.push_back(InstInfo(std::format("if {} {{", addBracket(cond->valStr)), SUPER_INFO_IF));
      getChild(1)->compute(insts);
      insts.push_back(InstInfo("} else {", SUPER_INFO_ELSE));
      getChild(2)->compute(insts);
      insts.push_back(InstInfo("}", SUPER_INFO_DEDENT));
      break;
    }
    case OP_STMT_NODE: {
      Assert(!isENode, "invalid stmt node\n");
      Node* node = tree->getlval()->getNode();
      valInfo* linfo = tree->getlval()->compute(node, INVALID_LVALUE, false);
      valInfo* rinfo = tree->getRoot()->compute(node, linfo->valStr, true);
      if (rinfo->status == VAL_FINISH || node->type == NODE_SPECIAL) { // printf / assert
        insts.push_back(InstInfo(rinfo->valStr));
      } else if (rinfo->status == VAL_INVALID) {
      } else if (rinfo->opNum >= 0) {
        if (rinfo->valStr != linfo->valStr) {
          if (belong) insts.push_back(InstInfo(SUPER_INFO_ASSIGN_BEG, belong));
          if (isSubArray(linfo->valStr, node)) {
            insts.push_back(InstInfo(arrayCopy(linfo->valStr, node, rinfo)));
          } else {
            insts.push_back(InstInfo(std::format("{} = {};", linfo->valStr, rinfo->valStr)));
          }
          if (belong) insts.push_back(InstInfo(SUPER_INFO_ASSIGN_END, belong));
        }
      } else {
        if (belong) insts.push_back(InstInfo(SUPER_INFO_ASSIGN_BEG, belong));
        insts.push_back(InstInfo(rinfo->valStr));
        if (belong) insts.push_back(InstInfo(SUPER_INFO_ASSIGN_END, belong));
      }
      break;
    }
    default: Panic();
  }
}

void StmtTree::compute(std::vector<InstInfo>& insts) {
  root->compute(insts);
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
static void recomputeAllNodes() {
  while (!recomputeQueue.empty()) {
    Node* node = recomputeQueue.top();
    recomputeQueue.pop();
    uniqueRecompute.erase(node);
    node->recompute();
  }
}

void Node::recompute() {
  if (!computeInfo) return;
  valInfo* prevVal = computeInfo;
  std::vector<valInfo*> prevArrayVal (computeInfo->memberInfo);
  computeInfo = nullptr;
  insts.clear();
  for (ExpTree* tree : assignTree) tree->clearInfo();
  compute();
  bool recomputeNext = false;
  if (prevVal->valStr != computeInfo->valStr) {
    recomputeNext = true;
  }
  if (prevArrayVal.size() != computeInfo->memberInfo.size()) {
    recomputeNext = true;
  } else {
    for (size_t i = 0; i < prevArrayVal.size(); i ++) {
      if ((!prevArrayVal[i] && computeInfo->memberInfo[i]) || (prevArrayVal[i] && !computeInfo->memberInfo[i])) {
        recomputeNext = true;
        break;
      }
      if (prevArrayVal[i] && computeInfo->memberInfo[i] && prevArrayVal[i]->valStr != computeInfo->memberInfo[i]->valStr) {
        recomputeNext = true;
        break;
      }
    }
  }
  if (recomputeNext) {
    for (Node* nextNode : next) {
      if (uniqueRecompute.find(nextNode) == uniqueRecompute.end()) {
        recomputeQueue.push(nextNode);
        uniqueRecompute.insert(nextNode);
      }
    }
  }
}

void Node::finalConnect(std::string lvalue, valInfo* info) {
  if (info->valStr.length() == 0) return; // empty, used for when statment with constant condition
  if (info->valStr == name) return; // no need to connect
  if (info->type == TYPE_STMT) {
    insts.push_back(info->valStr);
  } else if (info->type == TYPE_ARRAY) {
    insts.push_back(arrayCopy(lvalue, this, info));
  } else if (isSubArray(lvalue, this)) {
    if (width <= BASIC_WIDTH && info->width <= width) {
      if (info->status == VAL_CONSTANT)
        insts.push_back(std::format("memset({0}, {1}, sizeof({0}));", lvalue, info->valStr));
      else {
        insts.push_back(arrayCopy(lvalue, this, info));
      }
    } else if (width < BASIC_WIDTH && info->width > width) {
      TODO();
    } else {
      insts.push_back(arrayCopy(lvalue, this, info));;
    }
  } else {
    if (info->width > width) {
      info->valStr = std::format("({} & {})", info->valStr, bitMask(width));
    }
    if (sign && width != info->width) insts.push_back(std::format("{} = {}{};", lvalue, Cast(info->width, info->sign), info->valStr));
    else insts.push_back(std::format("{} = {};", lvalue, info->valStr));
  }
}

valInfo* Node::computeArray() {
  if (computeInfo) return computeInfo;
  MUX_DEBUG(printf("compute Array %s\n", name.c_str()));
  MUX_DEBUG(display());
  if (width == 0) {
    computeInfo = new valInfo();
    computeInfo->setConstantByStr("0");
    status = CONSTANT_NODE;
    return computeInfo;
  }
  computeInfo = new valInfo();
  computeInfo->valStr = name;
  computeInfo->width = width;
  computeInfo->sign = sign;
  std::set<int> allIdx;
  bool anyVarIdx = false;
  for (ExpTree* tree : assignTree) {
    std::string lvalue = name;
    valInfo* lindex = tree->getlval()->compute(this, INVALID_LVALUE, false);
    lvalue = lindex->valStr;
    std::tie(lindex->beg, lindex->end) = tree->getlval()->getIdx(this);
    if (lindex->beg < 0) anyVarIdx = true;
    else {
      for (int i = lindex->beg; i <= lindex->end; i ++) {
        if (allIdx.find(i) != allIdx.end()) anyVarIdx = true;
        allIdx.insert(i);
      }
    }
    valInfo* info = tree->getRoot()->compute(this, lvalue, true);
    for (std::string inst : lindex->insts) insts.push_back(inst);
    for (std::string inst : info->insts) insts.push_back(inst);
    info->insts.clear();
    finalConnect(lvalue, info);
  }

  for (size_t i = 0; i < assignTree.size(); i ++) ops += assignTree[i]->getRoot()->computeInfo->opNum >= 0 ? assignTree[i]->getRoot()->computeInfo->opNum : 1000;
  bool updated = false;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    if (assignTree[i]->getRoot()->computeInfo->fullyUpdated) {
      updated = true;
      break;
    }
  }
  fullyUpdated = updated;

  if (!anyVarIdx) {
    int num = arrayEntryNum();
    computeInfo->memberInfo.resize(num, nullptr);
    for (ExpTree* tree : assignTree) {
      int infoIdxBeg = tree->getlval()->computeInfo->beg;
      int infoIdxEnd = tree->getlval()->computeInfo->end;
      if (tree->getRoot()->computeInfo->valStr.find("TMP$") == tree->getRoot()->computeInfo->valStr.npos) {
        if (infoIdxBeg == infoIdxEnd) {
          computeInfo->memberInfo[infoIdxBeg] = tree->getRoot()->computeInfo;
        } else if (tree->getRoot()->computeInfo->memberInfo.size() != 0) {
          for (int i = 0; i <= infoIdxEnd - infoIdxBeg; i ++) {
            computeInfo->memberInfo[infoIdxBeg + i] = tree->getRoot()->computeInfo->getMemberInfo(i);
          }
        }
      }
    }

    if (type == NODE_REG_DST && getSrc()->assignTree.size() == 0) {
      for (int i = 0; i < num; i ++) {
        if (computeInfo->memberInfo[i] && computeInfo->memberInfo[i]->sameConstant) {
          computeInfo->memberInfo[i]->status = VAL_CONSTANT;
          mpz_set(computeInfo->memberInfo[i]->consVal, computeInfo->memberInfo[i]->assignmentCons);
          computeInfo->memberInfo[i]->setConsStr();
          insts.push_back(std::format("{}{} = {};", name, idx2Str(this, i, 0), computeInfo->memberInfo[i]->valStr));
        }
      }
    }
    bool allConstant = true;
    for (int i = 0; i < num; i ++) {
      if (!computeInfo->memberInfo[i] || computeInfo->memberInfo[i]->status != VAL_CONSTANT) {
        allConstant = false;
        break;
      }
    }
    if (allConstant) {
      initInsts.insert(initInsts.end(), insts.begin(), insts.end());
      insts.clear();
    }
  }
  /* to avoid update register using previous updated registers */
  if (type == NODE_REG_DST) computeInfo->memberInfo.clear();
  for (size_t i = 0; i < computeInfo->memberInfo.size(); i ++) {
    if (computeInfo->memberInfo[i] && computeInfo->memberInfo[i]->valStr.find("TMP$") != computeInfo->memberInfo[i]->valStr.npos) computeInfo->memberInfo[i] = nullptr;
  }

  return computeInfo;
}

void Node::updateIsRoot() {
  if (anyExtEdge() || next.size() != 1 || isReset() || isExt()) nodeIsRoot = true;
  if (isArrayMember) {
    for (Node* nextNode : next) {
      if (nextNode->isArray()) nodeIsRoot = true;
    }
  }
  for (Node* prevNode : prev) {
    if (prevNode->type == NODE_REG_SRC && !prevNode->regSplit && prevNode->getDst()->super == super) {
      nodeIsRoot = true;
    }
  }
}

std::string computeExtMod(SuperNode* super) {
  Assert(super->member[0]->type == NODE_EXT && super->member[0]->assignTree.size() == 1, "invalid extmod\n");

  std::string funcName = (super->member[0]->extraInfo.length() ? super->member[0]->extraInfo : super->member[0]->name);

  std::string funcDecl = "void " + funcName + "(";
  std::string inst = funcName + "(";
  int argIdx = 0;
  for (size_t i = 0; i < super->member[0]->member.size(); i ++) {
    Node* arg = super->member[0]->member[i];
    if (i != 0) {
      funcDecl += ", ";
      inst += ", ";
    }

    if (arg->type == NODE_EXT_IN) {
      if (arg->isArray()) {
        for (size_t j = 0; j < arg->arrayEntryNum(); j ++) {
          if (j != 0) {
            funcDecl += ", ";
            inst += ", ";
          }
          if (arg->status == CONSTANT_NODE) {
            inst += arg->computeInfo->valStr;
          } else {
            inst += arg->name + "[" + std::to_string(j) + "]";
          }
          funcDecl += widthUType(arg->width) + " _" + std::to_string(argIdx ++);
        }
      } else {
        funcDecl += widthUType(arg->width) + " _" + std::to_string(argIdx ++);
        inst += arg->compute()->valStr;
      }
    } else {
      if (arg->isArray()) {
        TODO();
      } else {
        funcDecl += widthUType(arg->width) + "& _" + std::to_string(argIdx ++);
        inst += arg->name;
      }
    }
  }
  funcDecl += ");";
  inst += ");";
  super->member[0]->insts.push_back(inst);
  return funcDecl;
}

void graph::instsGenerator() {
  maxConcatNum = 0;
  std::set<Node*> s;
  std::set<Node*> s_array;
  for (SuperNode* super : sortedSuper) {
    for (Node* n : super->member) n->updateIsRoot();
  }
  for (SuperNode* super : sortedSuper) {
    if (super->superType == SUPER_EXTMOD) {
      extDecl.push_back(computeExtMod(super));
    } else {
      super->stmtTree->compute(super->insts);
    }
  }

  for (Node* reg : regsrc) {
    if (reg->status != VALID_NODE || !reg->resetTree) continue;
    if (reg->resetTree) {
      reg->resetTree->getRoot()->compute(reg, reg->name, true);
    }
    valInfo* info = reg->resetTree->getRoot()->computeInfo;
    for (std::string inst : info->insts) {
      reg->resetInsts.push_back(inst);
    }
    if (info->status == VAL_EMPTY) info->setConstantByStr("0");
    if (info->type == TYPE_STMT) {
      reg->resetInsts.push_back(info->valStr);
    } else if (info->type == TYPE_ARRAY) {
      reg->resetInsts.push_back(arrayCopy(reg->name, reg, info));
    } else if (reg->isArray()) {
      if (info->status == VAL_CONSTANT && reg->width <= BASIC_WIDTH)
        reg->resetInsts.push_back(std::format("memset({0}, {1}, sizeof({0}));", reg->name, info->valStr));
      else {
        reg->resetInsts.push_back(arrayCopy(reg->name, reg, info));
      }
    } else {
      reg->resetInsts.push_back(reg->name + " = " + info->valStr + ";");
    }
    if (reg->regSplit && reg->getDst()->status == VALID_NODE) {
      std::vector<std::string> newInsts;
      size_t start_pos = 0;
      for (std::string inst : reg->resetInsts) {
        while((start_pos = inst.find(reg->name, start_pos)) != std::string::npos) {
          inst.replace(start_pos, reg->name.length(), reg->getDst()->name);
          start_pos += reg->name.length();
        }
        newInsts.push_back(inst);
      }
      reg->resetInsts.insert(reg->resetInsts.end(), newInsts.begin(), newInsts.end());
    }
  }

  /* remove constant nodes */
  size_t totalNodes = countNodes();
  size_t totalSuper = sortedSuper.size();
  /* TODO: remove constant nodes */

  size_t optimizeNodes = countNodes();
  size_t optimizeSuper = sortedSuper.size();
  printf("[instGenerator] remove %ld constantNodes (%ld -> %ld)\n", totalNodes - optimizeNodes, totalNodes, optimizeNodes);
  printf("[instGenerator] remove %ld superNodes (%ld -> %ld)\n",  totalSuper - optimizeSuper, totalSuper, optimizeSuper);

}
