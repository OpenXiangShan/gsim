/*
  Generator instructions for every nodes in sortedSuperNodes
  merge constantNode into here
*/

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
#define ChildInfo(id, name) getChild(id)->computeInfo->name

#define newLocalTmp() ("TMP$" + std::to_string((*localTmpNum) ++))
#define newMpzTmp() ("MPZ_TMP$" + std::to_string((*mpzTmpNum) ++))
static int *localTmpNum = nullptr;
static int *mpzTmpNum = nullptr;

#define INVALID_LVALUE "INVALID_STR"
#define IS_INVALID_LVALUE(name) (name == INVALID_LVALUE)

static int countArrayIndex(std::string name);
static bool isSubArray(std::string name, Node* node);
void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);
std::string idx2Str(Node* node, int idx, int dim);
static void recomputeAllNodes();

static std::stack<std::pair<int*, int*>> tmpStack;

std::set<std::pair<int, int>> allMask;

static int stmtDepth = 0;

static std::string addMask(int width) {
  allMask.insert(std::make_pair(width-1, 0));
  return format("MPZ_MASK$%d_%d", width-1, 0);
}
static std::string addMask(int hi, int lo) {
  allMask.insert(std::make_pair(hi, lo));
  return format("MPZ_MASK$%d_%d", hi, lo);
}

static void tmp_push() {
  tmpStack.push(std::make_pair(localTmpNum, mpzTmpNum));
}

static void tmp_pop() {
  std::tie(localTmpNum, mpzTmpNum) = tmpStack.top();
  tmpStack.pop();
}

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

std::string legalCppCons(std::string str) {
  if (str.length() <= 16) return "0x" + str;
  else if (str.length() <= 32) return format("UINT128(0x%s, 0x%s)", str.substr(0, str.length() - 16).c_str(), str.substr(str.length()-16, 16).c_str());
  else if (str.length() <= 48) return format("UINT256_3(0x%s, 0x%s, 0x%s)", str.substr(0, str.length() - 32).c_str(), str.substr(str.length()-32, 16).c_str(), str.substr(str.length()-16, 16).c_str());
  else return format("UINT256_4(0x%s, 0x%s, 0x%s, 0x%s)", str.substr(0, str.length() - 48).c_str(), str.substr(str.length()-48, 16).c_str(), str.substr(str.length()-32, 16).c_str(), str.substr(str.length()-16, 16).c_str());
}

static std::string getConsStr(mpz_t& val) {
  std::string str = mpz_get_str(NULL, 16, val);
  return legalCppCons(str);
}
static bool allOnes(mpz_t& val, int width) {
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

static std::string bitMask(int width) {
  Assert(width > 0, "invalid width %d", width);
  std::string ret = std::string(width/4, 'f');
  const char* headTable[] = {"", "1", "3", "7"};
  ret = headTable[width % 4] + ret;
  ret = legalCppCons(ret);
  return ret;
}

static std::string rangeMask(int hi, int lo) {
  mpz_t mask;
  mpz_init(mask);
  mpz_set_ui(mask, 1);
  mpz_mul_2exp(mask, mask, hi - lo + 1);
  mpz_sub_ui(mask, mask, 1);
  mpz_mul_2exp(mask, mask, lo);
  std::string ret = getConsStr(mask);
  return ret;
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

static std::string arrayCopy(std::string lvalue, Node* node, valInfo* rinfo) {
  std::string ret;
  int num = 1;
  int dimIdx = countArrayIndex(lvalue);
  for (size_t i = dimIdx; i < node->dimension.size(); i ++) num *= node->dimension[i];
  if (memberValid(rinfo, num)) {
    int consType = memberConstant(rinfo);
    if (consType == 0 || consType == 1) {
      for (int i = 0; i < num; i ++) {
        valInfo* assignInfo = rinfo->getMemberInfo(i);
        if (node->width > BASIC_WIDTH) {
          if (assignInfo->status == VAL_CONSTANT) {
            if (mpz_cmp_ui(assignInfo->consVal, MAX_U64) > 0) {
              ret += format("mpz_set_str(%s%s, \"%s\", 16);\n", lvalue.c_str(), idx2Str(node, i, dimIdx).c_str(), mpz_get_str(NULL, 16, assignInfo->consVal));
            } else {
              ret += format("mpz_set_ui(%s%s, %s);\n", lvalue.c_str(), idx2Str(node, i, dimIdx).c_str(), assignInfo->valStr.c_str());
            }
          } else {
            ret += format("mpz_set(%s%s, %s);\n", lvalue.c_str(), idx2Str(node, i, dimIdx).c_str(), assignInfo->valStr.c_str());
          }
        } else {
          ret += format("%s%s = %s;\n", lvalue.c_str(), idx2Str(node, i, dimIdx).c_str(), assignInfo->valStr.c_str());
        }
      }
    } else if (consType == 2) {
      if(node->width <= BASIC_WIDTH && mpz_sgn(rinfo->memberInfo[0]->consVal) == 0) {
        ret = format("memset(%s, 0, sizeof(%s));", lvalue.c_str(), lvalue.c_str());
      } else {
        std::string idxStr, bracket;
        for (int i = 0; i < node->dimension.size() - dimIdx; i ++) {
          ret += format("for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i + dimIdx], i);
          idxStr += "[i" + std::to_string(i) + "]";
          bracket += "}\n";
        }
        if (node->width > BASIC_WIDTH) {
          if (mpz_cmp_ui(rinfo->memberInfo[0]->consVal, MAX_U64) <= 0)
            ret += format("mpz_set_ui(%s%s, 0x%s);\n", lvalue.c_str(), idxStr.c_str(), rinfo->memberInfo[0]->valStr.c_str());
          else
            TODO();
        } else { // not mpz
          ret += format("%s%s = %s;\n", lvalue.c_str(), idxStr.c_str(), rinfo->memberInfo[0]->valStr.c_str());
        }
        ret += bracket;
      }
    } else Panic();

  } else {
    std::string idxStr, bracket;
    for (int i = 0; i < node->dimension.size() - dimIdx; i ++) {
      ret += format("for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, node->dimension[i + dimIdx], i);
      idxStr += "[i" + std::to_string(i) + "]";
      bracket += "}\n";
    }
    if (node->width > BASIC_WIDTH)
      ret += format("mpz_set(%s%s, %s);\n", lvalue.c_str(), idxStr.c_str(), rinfo->valStr.c_str());
    else
      ret += format("%s%s = %s;\n", lvalue.c_str(), idxStr.c_str(), rinfo->valStr.c_str());
    ret += bracket;
  }
  return ret;
}

static std::string setMpz(std::string dstName, ENode* enode, valInfo* dstInfo, Node* node) {
  std::string ret;
  if (enode->computeInfo->status == VAL_CONSTANT) {
    if (mpz_cmp_ui(enode->computeInfo->consVal, MAX_U64) > 0) ret = format("mpz_set_str(%s, \"%s\", 16);", dstName.c_str(), mpz_get_str(NULL, 16, enode->computeInfo->consVal));
    else ret = format("mpz_set_ui(%s, %s);", dstName.c_str(), enode->computeInfo->valStr.c_str());
  } else if (isSubArray(dstName, node)) {
    ret = arrayCopy(dstName, node, enode->computeInfo);
  } else if (enode->width > BASIC_WIDTH) {
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
  return format("((uint128_t)mpz_getlimbn(%s, 1) << 64 | mpz_get_ui(%s))",
              name.c_str(), name.c_str(), name.c_str(), name.c_str());
}
static std::string get256(std::string name, int width) { // TODO: replace by getFromMpz
  if (width <= 128) return get128(name);
  return format("((uint256_t)mpz_getlimbn(%s, 3) << 192 | "
                "(uint256_t)mpz_getlimbn(%s, 2) << 128 | "
                "(uint256_t)mpz_getlimbn(%s, 1) << 64 | "
                "(uint256_t)mpz_get_ui(%s))",
              name.c_str(), name.c_str(), name.c_str(), name.c_str(), name.c_str(), name.c_str(), name.c_str(), name.c_str());
}
static std::string set256_tmp(valInfo* info, valInfo* parent, std::string str = "") {
  std::string localName = info->valStr;
  std::string ret = str.length() == 0 ? newMpzTmp() : str;
  if (info->status == VAL_CONSTANT) {
    if (mpz_cmp_ui(info->consVal, MAX_U64) > 0) parent->insts.push_back(format("mpz_set_str(%s, \"%s\", 16);", ret.c_str(), mpz_get_str(NULL, 16, info->consVal)));
    else parent->insts.push_back(format("mpz_set_ui(%s, %s);", ret.c_str(), info->valStr.c_str()));
    return ret;
  }
  if (info->width <= 64) {
    parent->insts.push_back(format("mpz_set_ui(%s, %s);", ret.c_str(), info->valStr.c_str()));
  } else {
    if (info->opNum > 0) {
      localName = newLocalTmp();
      parent->insts.push_back(format("%s %s = %s;", widthUType(info->width).c_str(), localName.c_str(), info->valStr.c_str()));
    }
    int count = info->width > 128 ? 4 : 2;
    parent->insts.push_back(format("mpz_import(%s, %d, -1, 8, 0, 0, (mp_limb_t*)&%s);", ret.c_str(), count, localName.c_str()));
  }
  return ret;
}

static int countArrayIndex(std::string name) {
  int count = 0;
  int idx = 0;
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
      while((start_pos = inst.find(node->name, start_pos)) != std::string::npos) {
        inst.replace(start_pos, node->name.length(), node->getDst()->name);
        start_pos += node->name.length();
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
  if (isSubArray(lvalue, node)) return instsWhen(node, lvalue, isRoot);

  /* not constant */
  bool childBasic = ChildInfo(1, width) <= BASIC_WIDTH && ChildInfo(2, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  valInfo* ret = computeInfo;
  ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + ChildInfo(2, opNum) + 1;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  if (childBasic && enodeBasic) {
    if (width == 1 && !sign) {
      if (ChildInfo(1, status) == VAL_CONSTANT && ChildInfo(2, status) == VAL_CONSTANT) {
        if (mpz_sgn(ChildInfo(1, consVal)) != 0 && mpz_sgn(ChildInfo(2, consVal)) == 0) { // cond ? 1 : 0  = cond
          ret->valStr = ChildInfo(0, valStr);
        } else if (mpz_sgn(ChildInfo(1, consVal)) == 0 && mpz_sgn(ChildInfo(2, consVal)) != 0) { // cond ? 0 : 1  = !cond
          ret->valStr = format("(!%s)", ChildInfo(0, valStr).c_str());
        } else Panic();
      } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
        ret->valStr = format("((!%s) & %s)", ChildInfo(0, valStr).c_str(), ChildInfo(2, valStr).c_str());
      } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) != 0) {
        ret->valStr = format("(%s | %s)", ChildInfo(0, valStr).c_str(), ChildInfo(2, valStr).c_str());
      } else if (ChildInfo(2, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(2, consVal)) == 0) {
        ret->valStr = format("(%s & %s)", ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str());
      } else if (ChildInfo(2, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(2, consVal)) != 0) {
        ret->valStr = format("((!%s) | %s)", ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str());
      } else {
        ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + ChildInfo(1, valStr) + " : " + ChildInfo(2, valStr) + ")";
      }
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " ? " + ChildInfo(1, valStr) + " : " + ChildInfo(2, valStr) + ")";
    }
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string trueAssign = setMpz(dstName, getChild(1), ret, node);
    std::string falseAssign = setMpz(dstName, getChild(2), ret, node);
    ret->insts.push_back(format("if (%s) { %s } else { %s }", ChildInfo(0, valStr).c_str(), trueAssign.c_str(), falseAssign.c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (enodeBasic && !childBasic) {
    if (width <= 64) {
      std::string lname = ChildInfo(1, width) <= 64 ? ChildInfo(1, valStr) : format("mpz_get_ui(%s)", ChildInfo(1, valStr).c_str());
      std::string rname = ChildInfo(2, width) <= 64 ? ChildInfo(2, valStr) : format("mpz_get_ui(%s)", ChildInfo(2, valStr).c_str());
      ret->valStr = format("(%s ? %s : %s)", ChildInfo(0, valStr).c_str(), lname.c_str(), rname.c_str());
    } else {
      TODO();
    }
  } else {
    printf("width %d = mux %s(%d) %s(%d)\n", width, ChildInfo(1, valStr).c_str(), ChildInfo(1, width), ChildInfo(2, valStr).c_str(), ChildInfo(2, width));
    TODO();
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
  if (width > BASIC_WIDTH) {
    if (mpz_cmp_ui(info->consVal, MAX_U64) > 0) ret = format("mpz_set_str(%s, \"%s\", 16);", lvalue.c_str(), mpz_get_str(NULL, 16, info->consVal));
    else ret = format("mpz_set_ui(%s, %s);", lvalue.c_str(), info->valStr.c_str());
  } else {
    ret = format("%s = %s;\n", lvalue.c_str(), info->valStr.c_str());
  }
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
  if (childBasic && enodeBasic) {
    auto assignment = [lvalue, node](bool isStmt, std::string expr, int width, bool sign, valInfo* info) {
      if (isStmt) return expr;
      if (expr.length() == 0 || expr == lvalue) return std::string("");
      else if (isSubArray(lvalue, node)) return arrayCopy(lvalue, node, info);
      else if (node->width < width) return format("%s = (%s & %s);", lvalue.c_str(), expr.c_str(), bitMask(node->width).c_str());
      else if (node->sign && node->width != width) return format("%s = %s%s;", lvalue.c_str(), Cast(width, sign).c_str(), expr.c_str());
      return lvalue + " = " + expr + ";";
    };
    if (!isSubArray(lvalue, node) && node->assignTree.size() == 1 && stmtDepth == 0 && isRoot) {
      if (!getChild(1) && getChild(2) && ChildInfo(2, opNum) >= 0 && ChildInfo(2, opNum) < 1 && ChildInfo(2, insts).size() == 0) {
        toMux = true;
        condStr = ChildInfo(0, valStr);
        trueStr = lvalue;
        falseStr = ChildInfo(2, valStr);
      } else if (!getChild(2) && getChild(1) && ChildInfo(1, opNum) >= 0 && ChildInfo(1, opNum) < 1 && ChildInfo(1, insts).size() == 0) {
        toMux = true;
        condStr = ChildInfo(0, valStr);
        trueStr = ChildInfo(1, valStr);
        falseStr = lvalue;
      }
    }
    if (!toMux) {
      condStr = ChildInfo(0, valStr);
      trueStr = ((getChild(1) && ChildInfo(1, status) != VAL_INVALID) ?
                    assignment(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr), ChildInfo(1, width), Child(1, sign), Child(1, computeInfo)) : "");
      falseStr = ((getChild(2) && ChildInfo(2, status) != VAL_INVALID) ? assignment(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr), ChildInfo(2, width), Child(2, sign), Child(2, computeInfo)) : "");
    }

  } else if (!childBasic && !enodeBasic) { // can merge into childBasic && enodeBasic
    auto assignmentMpz = [lvalue, node, ret](bool isStmt, std::string expr, int width, bool sign, valInfo* info, ENode* child) {
      if (isStmt) return expr;
      if (expr.length() == 0 || expr == lvalue) return std::string("");
      else if (isSubArray(lvalue, node)) {
        return arrayCopy(lvalue, node, info);
      }
      else if (width <= 64) return format(sign ? "mpz_set_si(%s, %s);" : "mpz_set_ui(%s, %s);", lvalue.c_str(), expr.c_str());
      else if (info->status == VAL_CONSTANT && info->consLength <= 16) return format(sign ? "mpz_set_si(%s, %s);" : "mpz_set_ui(%s, %s);", lvalue.c_str(), expr.c_str());
      else if (info->status == VAL_CONSTANT) TODO();
      else if (width <= BASIC_WIDTH) return setMpz(lvalue, child, ret, node);
      return format("mpz_set(%s, %s);", lvalue.c_str(), expr.c_str());
    };
    condStr = ChildInfo(0, valStr).c_str();
    trueStr = ((getChild(1) && ChildInfo(1, status) != VAL_INVALID) ? assignmentMpz(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr), ChildInfo(1, width), Child(1, sign), Child(1, computeInfo), getChild(1)) : "");
    falseStr = ((getChild(2) && ChildInfo(2, status) != VAL_INVALID) ? assignmentMpz(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr), ChildInfo(2, width), Child(2, sign), Child(2, computeInfo), getChild(2)) : "");
  } else if (!enodeBasic && childBasic) {
    auto assignment = [lvalue, node](bool isStmt, std::string expr, int width, bool sign) {
      if (isStmt) return expr;
      if (expr.length() == 0 || expr == lvalue) return std::string("");
      else if (isSubArray(lvalue, node)) TODO();
      else if (width <= 64) return format(sign ? "mpz_set_si(%s, %s);" : "mpz_set_ui(%s, %s);", lvalue.c_str(), expr.c_str());
      else if (width <= BASIC_WIDTH) TODO();
      else TODO();
      return std::string("INALID");
    };
    condStr = ChildInfo(0, valStr).c_str();
    trueStr = ((getChild(1) && ChildInfo(1, status) != VAL_INVALID) ? assignment(ChildInfo(1, opNum) < 0, ChildInfo(1, valStr), ChildInfo(1, width), Child(1, sign)) : "");
    falseStr = ((getChild(2) && ChildInfo(2, status) != VAL_INVALID) ? assignment(ChildInfo(2, opNum) < 0, ChildInfo(2, valStr), ChildInfo(2, width), Child(2, sign)) : "");

  } else {
    TODO();
  }
  if (toMux) {
    ret->valStr = format(" (%s ? %s : %s)", condStr.c_str(), trueStr.c_str(), falseStr.c_str());
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
    if (node->isArray()) {
      if (getChild(1) && ChildInfo(1, directUpdate)) trueStr += ASSIGN_LABLE;
      if (getChild(2) && ChildInfo(2, directUpdate)) falseStr += ASSIGN_LABLE;
    }
    ret->valStr = format("if(%s) { %s } else { %s }", condStr.c_str(), trueStr.c_str(), falseStr.c_str());
    ret->opNum = -1;
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
    else if (childENode->computeInfo->opNum < 0) {
      computeInfo->valStr += childENode->computeInfo->valStr;
    } else if (width > BASIC_WIDTH){
      computeInfo->valStr += setMpz(lvalue, childENode, computeInfo, node);
    } else {
      if (isSubArray(lvalue, node)) {
        computeInfo->valStr += arrayCopy(lvalue, node, childENode->computeInfo);
      } else if (node->width < width) {
        computeInfo->valStr += format("%s = (%s & %s);", lvalue.c_str(), childENode->computeInfo->valStr.c_str(), bitMask(node->width).c_str());
      } else if (node->sign && node->width != width) {
        computeInfo->valStr += format("%s = %s%s;", lvalue.c_str(), Cast(width, sign).c_str(), childENode->computeInfo->valStr.c_str());
      }
      computeInfo->valStr += lvalue + " = " + childENode->computeInfo->valStr + ";";
    }
  }
  computeInfo->opNum = -1;
  computeInfo->fullyUpdated = updated;
  computeInfo->directUpdate = direct;
  return computeInfo;
}

valInfo* ENode::instsAdd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_add(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    std::string lstr = ChildInfo(0, valStr);
    std::string rstr = ChildInfo(1, valStr);
    if (sign) { // signed extension
      if(ChildInfo(0, width) < ChildInfo(1, width) && ChildInfo(0, status) != VAL_CONSTANT) {
        int extendedWidth = widthBits(ChildInfo(1, width));
        int shiftBits = extendedWidth - ChildInfo(0, width);
        lstr = format("((%s%s%s << %d) >> %d)", Cast(ChildInfo(1, width), true).c_str(), Cast(ChildInfo(0, width), true).c_str(), lstr.c_str(), shiftBits, shiftBits);
      } else if (ChildInfo(0, width) > ChildInfo(1, width) && ChildInfo(1, status) != VAL_CONSTANT) {
        int extendedWidth = widthBits(ChildInfo(0, width));
        int shiftBits = extendedWidth - ChildInfo(1, width);
        rstr = format("((%s%s%s << %d) >> %d)", Cast(ChildInfo(0, width), true).c_str(), Cast(ChildInfo(1, width), true).c_str(), rstr.c_str(), shiftBits, shiftBits);
      }
    }
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + lstr + " + " + rstr + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    if (!sign && width <= MAX(ChildInfo(0, width), ChildInfo(1, width))) {
      ret->valStr = format("(%s & %s)", ret->valStr.c_str(), bitMask(width).c_str());
    }
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_sub(ret->consVal, ChildInfo(0, consVal), ChildInfo(1, consVal), width);
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    std::string lstr = ChildInfo(0, valStr);
    std::string rstr = ChildInfo(1, valStr);
    if (sign) {
      if(ChildInfo(0, width) < ChildInfo(1, width) && ChildInfo(0, status) != VAL_CONSTANT) {
        int extendedWidth = widthBits(ChildInfo(1, width));
        int shiftBits = extendedWidth - ChildInfo(0, width);
        lstr = format("((%s%s%s << %d) >> %d)", Cast(ChildInfo(1, width), true).c_str(), Cast(ChildInfo(0, width), true).c_str(), lstr.c_str(), shiftBits, shiftBits);
      } else if (ChildInfo(0, width) > ChildInfo(1, width) && ChildInfo(1, status) != VAL_CONSTANT) {
        int extendedWidth = widthBits(ChildInfo(0, width));
        int shiftBits = extendedWidth - ChildInfo(1, width);
        rstr = format("((%s%s%s << %d) >> %d)", Cast(ChildInfo(0, width), true).c_str(), Cast(ChildInfo(1, width), true).c_str(), rstr.c_str(), shiftBits, shiftBits);
      }
    }
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + lstr + " - " + rstr + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    if (!sign) {
      ret->valStr = format("(%s & %s)", ret->valStr.c_str(), bitMask(width).c_str());
    }
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsMul(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_mul(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + " * " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string lname = newMpzTmp();
    std::string rname = newMpzTmp();
    /* assign left & right values to mpz variables */
    if (ChildInfo(0, width) > 64) {
      std::string localVal = ChildInfo(0, valStr);
      if (ChildInfo(0, opNum) > 0) {
        localVal = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(ChildInfo(0, width)).c_str(), localVal.c_str(), ChildInfo(0, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);", lname.c_str(), localVal.c_str()));
    } else {
      std::string funcName = sign ? "mpz_set_si" : "mpz_set_ui";
      ret->insts.push_back(format("%s(%s, %s);", funcName.c_str(), lname.c_str(), ChildInfo(0, valStr).c_str()));
    }
    if (ChildInfo(1, width) > 64) {
      std::string localVal = ChildInfo(0, valStr);
      if (ChildInfo(1, opNum) > 0) {
        localVal = newLocalTmp();
        ret->insts.push_back(format("%s %s = %s;", widthUType(ChildInfo(1, width)).c_str(), localVal.c_str(), ChildInfo(1, valStr).c_str()));
      }
      ret->insts.push_back(format("mpz_import(%s, 2, -1, 8, 0, 0, (mp_limb_t*)&%s);", rname.c_str(), localVal.c_str()));
    } else {
      std::string funcName = sign ? "mpz_set_si" : "mpz_set_ui";
      ret->insts.push_back(format("%s(%s, %s);", funcName.c_str(), rname.c_str(), ChildInfo(1, valStr).c_str()));
    }
    ret->insts.push_back(format("mpz_mul(%s, %s, %s);", dstName.c_str(), lname.c_str(), rname.c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDIv(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_div(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + " / " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsRem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_rem(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign)+ ChildInfo(0, valStr) + " % " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsLt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_lt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_leq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_gt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_geq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_eq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("1");
  } else if ((ChildInfo(0, status) == VAL_CONSTANT && mpzOutOfBound(ChildInfo(0, consVal), Child(1, width)))
      ||(ChildInfo(1, status) == VAL_CONSTANT && mpzOutOfBound(ChildInfo(1, consVal), Child(0, width)))) {
    ret->setConstantByStr("0");
  } else if (childBasic && enodeBasic) {
    if (Child(1, width) == 1 && ChildInfo(0, status) == VAL_CONSTANT && mpz_cmp_ui(ChildInfo(0, consVal), 1) == 0) {
      computeInfo = Child(1, computeInfo);
    } else if (Child(0, width) == 1 && ChildInfo(1, status) == VAL_CONSTANT && mpz_cmp_ui(ChildInfo(1, consVal), 1) == 0) {
      computeInfo = Child(0, computeInfo);
    } else if (Child(0, sign)) {
      ret->valStr = format("(%s%s == %s%s)", (ChildInfo(0, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(0, width), ChildInfo(0, sign)).c_str()), ChildInfo(0, valStr).c_str(),
                                             (ChildInfo(1, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(1, width), ChildInfo(1, sign)).c_str()), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " == " + ChildInfo(1, valStr) + ")";
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  } else if (!childBasic && enodeBasic) {
    if (ChildInfo(0, width) > BASIC_WIDTH && ChildInfo(1, width) > BASIC_WIDTH) {
      ret->valStr = format("(mpz_cmp(%s, %s) == 0)", ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str());
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    } else if (ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) > BASIC_WIDTH){
      std::string tmp;
      if (ChildInfo(0, width) > 64) {
        std::string tmp = set256_tmp(Child(0, computeInfo), computeInfo);
      } else {
        tmp = newMpzTmp();
        computeInfo->insts.push_back(format("mpz_set_ui(%s, %s);", tmp.c_str(), ChildInfo(0, valStr).c_str()));
      }
      ret->valStr = format("(mpz_cmp(%s, %s) == 0)", tmp.c_str(), ChildInfo(1, valStr).c_str());
      ret->opNum = 1;
    } else if (ChildInfo(0, width) > BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH){
      std::string tmp;
      if (ChildInfo(1, width) > 64) {
        tmp = set256_tmp(Child(1, computeInfo), computeInfo);
      } else {
        tmp = newMpzTmp();
        computeInfo->insts.push_back(format("mpz_set_ui(%s, %s);", tmp.c_str(), ChildInfo(1, valStr).c_str()));
      }
      ret->valStr = format("(mpz_cmp(%s, %s) == 0)", tmp.c_str(), ChildInfo(0, valStr).c_str());
      ret->opNum = 1;
    }
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsNeq(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    us_neq(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(0, valStr) == ChildInfo(1, valStr)) {
    ret->setConstantByStr("0");
  } else if (childBasic && enodeBasic) {
    if (Child(1, width) == 1 && ChildInfo(0, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(0, consVal)) == 0) {
      computeInfo = Child(1, computeInfo);
    } else if (Child(0, width) == 1 && ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
      computeInfo = Child(0, computeInfo);
    } else if (Child(0, sign)) {
      ret->valStr = format("(%s%s != %s%s)", (ChildInfo(0, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(0, width), ChildInfo(0, sign)).c_str()), ChildInfo(0, valStr).c_str(),
                                             (ChildInfo(1, status) == VAL_CONSTANT ? "" : Cast(ChildInfo(1, width), ChildInfo(1, sign)).c_str()), ChildInfo(1, valStr).c_str());
    } else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " != " + ChildInfo(1, valStr) + ")";
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  } else if (!childBasic && enodeBasic) {
    if (ChildInfo(0, status) == VAL_CONSTANT && ChildInfo(0, consLength) > 64) TODO();
    if (ChildInfo(1, status) == VAL_CONSTANT && ChildInfo(1, consLength) > 64) TODO();
    std::string lvalue = ChildInfo(0, valStr);
    std::string rvalue = ChildInfo(1, valStr);
    if (ChildInfo(0, status) == VAL_CONSTANT) {
      rvalue = ChildInfo(0, valStr);
      lvalue = ChildInfo(1, valStr);
    }
    bool halfConstant = ChildInfo(0, status) == VAL_CONSTANT || ChildInfo(1, status) == VAL_CONSTANT;
    std::string formatStr = halfConstant ? (sign ? "(mpz_cmp_si(%s, %s) != 0)" : "(mpz_cmp_ui(%s, %s) != 0)")
                                        : "(mpz_cmp(%s, %s) != 0)";
    ret->valStr = format(formatStr.c_str(), lvalue.c_str(), rvalue.c_str());
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshl(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_dshl(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    int castWidth = ChildInfo(0, width) + (1 << ChildInfo(1, width)) - 1;
    ret->valStr = format("(%s(%s%s << %s) & %s)", Cast(width, sign).c_str(), upperCast(castWidth, ChildInfo(0, width), sign).c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str(), bitMask(width).c_str());
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    if (ChildInfo(0, width) < 64 && ChildInfo(1, width) < 64) {
      std::string midName = newMpzTmp();
      ret->insts.push_back(format("mpz_set_ui(%s, %s);", midName.c_str(), ChildInfo(0, valStr).c_str()));
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
      ret->valStr = dstName;
      ret->opNum = 0;
    } else if (ChildInfo(1, width) < 64) {
      std::string midName = set256_tmp(Child(0, computeInfo), ret);
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
      ret->valStr = dstName;
      ret->opNum = 0;
    } else {
      TODO();
    }
  } else if (!childBasic && !enodeBasic) {
    if (ChildInfo(1, width) > 64) TODO();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsDshr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    (sign ? s_dshr : u_dshr)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
    if (width == ChildInfo(0, width)) {
      ret->valStr = ChildInfo(0, valStr);
      ret->opNum = ChildInfo(0, opNum);
    } else {
      ret->valStr = format("(%s & %s)", ChildInfo(0, valStr).c_str(), bitMask(width).c_str());
      ret->opNum = ChildInfo(0, opNum) + 1;
    }
  } else if (childBasic && enodeBasic) {
    ret->valStr = format("((%s%s >> %s) & %s)", Cast(ChildInfo(0, width), Child(0, sign)).c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str(), bitMask(width).c_str());
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    if (ChildInfo(1, width) > 64) TODO();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
    if (width < ChildInfo(0, width)) {
      std::string tmpMpz = newMpzTmp();
      ret->insts.push_back(format("mpz_and(%s, %s, %s);", dstName.c_str(), addMask(width).c_str(), dstName.c_str()));
    }
  } else if (enodeBasic && !childBasic) {
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %s);", tmp.c_str(), ChildInfo(0, valStr).c_str(), ChildInfo(1, valStr).c_str()));
    if (width <= 64) {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", tmp.c_str(), bitMask(width).c_str());
      ret->opNum = 1;
    } else TODO();
  } else {
    printf("%d = %s(%d) >> %s(%d)\n", width, ChildInfo(0, valStr).c_str(), ChildInfo(0, width), ChildInfo(1, valStr).c_str(), ChildInfo(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAnd(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
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
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " & " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    if (width <= 64) {
      std::string lname = ChildInfo(0, width) <= 64 ? ChildInfo(0, valStr) : format("mpz_get_ui(%s)", ChildInfo(0, valStr).c_str());
      std::string rname = ChildInfo(1, width) <= 64 ? ChildInfo(1, valStr) : format("mpz_get_ui(%s)", ChildInfo(1, valStr).c_str());
      ret->valStr = format("(%s & %s)", lname.c_str(), rname.c_str());
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    } else {
      TODO();
    }
  } else if (!childBasic && !enodeBasic) {
    std::string lvalue = ChildInfo(0, valStr), rvalue = ChildInfo(1, valStr);
    if (ChildInfo(0, status) == VAL_CONSTANT) {
      lvalue = newMpzTmp();
      setMpz(lvalue, getChild(0), ret, node);
    }
    if (ChildInfo(1, status) == VAL_CONSTANT) {
      rvalue = newMpzTmp();
      setMpz(rvalue, getChild(1), ret, node);
    }
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", dstName.c_str(), lvalue.c_str(), rvalue.c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("%d = %s(%d) & %s(%d)\n", width, ChildInfo(0, valStr).c_str(), ChildInfo(0, width), ChildInfo(1, valStr).c_str(), ChildInfo(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
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
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " | " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    valInfo* linfo = Child(0, computeInfo);
    valInfo* rinfo = Child(1, computeInfo);
    if (ChildInfo(0, status) == VAL_CONSTANT) {
      rinfo = Child(0, computeInfo);
      linfo = Child(1, computeInfo);
    }
    if(rinfo->status == VAL_CONSTANT) {
      if (mpz_sgn(rinfo->consVal) != 0) TODO();
      ret->valStr = linfo->valStr;
      ret->opNum = linfo->opNum;
    } else {
      std::string lname = ChildInfo(0, valStr).c_str();
      std::string rname = ChildInfo(1, valStr).c_str();
      if (ChildInfo(0, width) <= BASIC_WIDTH) lname = set256_tmp(Child(0, computeInfo), ret);
      if (ChildInfo(1, width) <= BASIC_WIDTH) rname = set256_tmp(Child(1, computeInfo), ret);
      ret->insts.push_back(format("mpz_ior(%s, %s, %s);", dstName.c_str(), lname.c_str(), rname.c_str()));
      ret->valStr = dstName;
      ret->opNum = 0;
    }
  } else {
    printf("%d = %s(%d) | %s(%d)\n", width, ChildInfo(0, valStr).c_str(), ChildInfo(0, width), ChildInfo(1, valStr).c_str(), ChildInfo(1, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXor(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (isConstant) {
    if (sign) TODO();
    u_xor(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, consVal), ChildInfo(1, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + ChildInfo(1, valStr) + ")";
    ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    if (ChildInfo(0, width) > BASIC_WIDTH && ChildInfo(1, width) > BASIC_WIDTH) {
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH && ChildInfo(1, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = (ChildInfo(0, status) == VAL_CONSTANT) && (ChildInfo(1, status) == VAL_CONSTANT);

  if (ChildInfo(0, status) == VAL_CONSTANT) {
    for (valInfo* info : Child(1, computeInfo)->memberInfo) {
      if (info && info->status == VAL_CONSTANT) {
        valInfo* newMemberInfo = new valInfo();
        newMemberInfo->status = VAL_CONSTANT;
        newMemberInfo->width = width;
        newMemberInfo->sign = sign;
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
  } else if (childBasic && enodeBasic) {
    std::string hi;
    if (ChildInfo(0, status) == VAL_CONSTANT) {
      mpz_t cons_hi;
      mpz_init(cons_hi);
      u_shl(cons_hi, ChildInfo(0, consVal), ChildInfo(0, width), ChildInfo(1, width));
      if (Child(0, sign)) TODO();
      if (mpz_sgn(cons_hi)) hi = getConsStr(cons_hi);
    } else
      hi = "(" + upperCast(width, ChildInfo(0, width), false) + ChildInfo(0, valStr) + " << " + std::to_string(Child(1, width)) + ")";
    if (hi.length() == 0) { // hi is zero
      ret->valStr = upperCast(width, ChildInfo(1, width), ChildInfo(1, sign)) + ChildInfo(1, valStr);
      ret->opNum = ChildInfo(1, opNum) + 1;
    } else {
      if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
        ret->valStr = hi;
      } else {
        ret->valStr = "(" + hi + " | " + ChildInfo(1, valStr) + ")";
      }
      ret->opNum = ChildInfo(0, opNum) + ChildInfo(1, opNum) + 1;
    }
  } else if (childBasic && !enodeBasic) { // child <= BASIC_WIDTH, cur > BASIC_WIDTH
    Assert(ChildInfo(0, opNum) >= 0 && ChildInfo(1, opNum) >= 0, "invalid opNum (%d %s) (%d, %s)", ChildInfo(0, opNum), ChildInfo(0, valStr).c_str(), ChildInfo(1, opNum), ChildInfo(1, valStr).c_str());
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string midName;
    /* set first value */
    midName = set256_tmp(Child(0, computeInfo), ret);
    /* concat second value*/
    if (ChildInfo(1, width) > 64) {
      std::string rightName = set256_tmp(Child(1, computeInfo), ret);
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);\n", midName.c_str(), midName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add(%s, %s, %s);", dstName.c_str(), midName.c_str(), rightName.c_str()));
    } else {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), midName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    }
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (!childBasic && !enodeBasic) { // child > BASIC, cur > BASIC
    std::string midName = newMpzTmp();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    std::string firstName = ChildInfo(0, valStr);
    /* one of child is basic */
    if (ChildInfo(0, width) <= BASIC_WIDTH || ChildInfo(0, status) == VAL_CONSTANT) {
      firstName = set256_tmp(Child(0, computeInfo), ret);
    }
    if (ChildInfo(1, status) == VAL_CONSTANT && mpz_sgn(ChildInfo(1, consVal)) == 0) {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), firstName.c_str(), Child(1, width)));
    } else if (Child(1, width) <= 64) {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), firstName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add_ui(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    } else if (Child(1, width) <= BASIC_WIDTH) {
      set256_tmp(Child(1, computeInfo), ret, midName);
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);\n", midName.c_str(), firstName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add(%s, %s, %s);", dstName.c_str(), midName.c_str(), midName.c_str()));
    } else {
      ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", midName.c_str(), firstName.c_str(), Child(1, width)));
      ret->insts.push_back(format("mpz_add(%s, %s, %s);", dstName.c_str(), midName.c_str(), ChildInfo(1, valStr).c_str()));
    }
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asUInt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_asSInt(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    if (Child(0, sign)) ret->valStr = format("%s%s%s", Cast(width, true).c_str(), Cast(ChildInfo(0, width), true).c_str(), ChildInfo(0, valStr).c_str());
    else {
      int shiftBits = widthBits(width) - ChildInfo(0, width);
      if (shiftBits == 0)
        ret->valStr = format("(%s%s)", Cast(width, true).c_str(), ChildInfo(0, valStr).c_str(), shiftBits, shiftBits);
      else
        ret->valStr = format("(%s(%s << %d) >> %d)", Cast(width, true).c_str(), ChildInfo(0, valStr).c_str(), shiftBits, shiftBits);
    }
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asClock(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_asAsyncReset(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    if (ChildInfo(0, width) == 1) ret->valStr = ChildInfo(0, valStr);
    else ret->valStr = "(" + ChildInfo(0, valStr) + " != 0)";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsCvt(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;

  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_cvt : u_cvt)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (!sign) TODO();
    s_neg(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    /* cast explicitly only if type do not match or argument is UInt */
    std::string castStr = (typeCmp(width, ChildInfo(0, width)) > 0 || !getChild(0)->sign) ? Cast(width, sign) : "";
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_not(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " ^ " + bitMask(width) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string tmpMpz = newMpzTmp();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_xor(%s, %s, %s);", dstName.c_str(), addMask(width).c_str(), ChildInfo(0, valStr).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    printf("%d = ~ %s(%d)\n", width, ChildInfo(0, valStr).c_str(), ChildInfo(0, width));
    TODO();
  }
  return ret;
}

valInfo* ENode::instsAndr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_andr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " == " + bitMask(ChildInfo(0, width)) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsOrr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  if (Child(0, width) != ChildInfo(0, width)) TODO();
  if (isConstant) {
    if (sign) TODO();
    u_orr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " != 0 )";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    ret->valStr = format("(mpz_sgn(%s) != 0)", ChildInfo(0, valStr).c_str());
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsXorr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    if (sign) TODO();
    u_xorr(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width));
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    if (Child(0, width) <= 64) {
      ret->valStr = "__builtin_parityl(" + ChildInfo(0, valStr) + ")";
      ret->opNum = ChildInfo(0, opNum) + 1;
    } else if (Child(0, width) <= 128) {
      ret->valStr = format("(__builtin_parityl(%s >> 64) ^ __builtin_parityl(%s))", ChildInfo(0, valStr).c_str(), ChildInfo(0, valStr).c_str());
      ret->opNum = ChildInfo(0, opNum) + 1;
    } else TODO();
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsPad(Node* node, std::string lvalue, bool isRoot) {
  /* no operation for UInt variable */
  if (!sign || (width <= ChildInfo(0, width))) {
    for (ENode* childNode : child) computeInfo->mergeInsts(childNode->computeInfo);
    if (width > BASIC_WIDTH && ChildInfo(0, width) <= BASIC_WIDTH) {
      computeInfo->valStr = set256_tmp(Child(0, computeInfo), computeInfo);
      computeInfo->opNum = 0;
    } else {
      if (ChildInfo(0, opNum) >= 0 && widthBits(ChildInfo(0, width)) < width && width <= BASIC_WIDTH) {
        computeInfo->valStr = format("(%s)%s", widthUType(width).c_str(), ChildInfo(0, valStr).c_str());
        computeInfo->opNum = ChildInfo(0, opNum) + 1;
      } else {
        computeInfo->valStr = ChildInfo(0, valStr);
        computeInfo->opNum = ChildInfo(0, opNum);
      }
    }
    return computeInfo;
  }
  /* SInt padding */
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;

  if (isConstant) {
    (sign ? s_pad : u_pad)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), values[0]);  // n(values[0]) == width
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    std::string operandName;
    if (ChildInfo(0, opNum) == 0) operandName = ChildInfo(0, valStr);
    else {
      operandName = newLocalTmp();
      std::string tmp_def = widthType(ChildInfo(0, width), sign) + " " + operandName + " = " + ChildInfo(0, valStr) + ";";
      ret->insts.push_back(tmp_def);
    }
    int shiftBits = widthBits(width) - ChildInfo(0, width);
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    if (sign) TODO();
    u_shl(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n);
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + upperCast(width, ChildInfo(0, width), sign) + ChildInfo(0, valStr) + " << " + std::to_string(n) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), ChildInfo(0, valStr).c_str(), n));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else if (childBasic && !enodeBasic){
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    if (Child(0, width) <= 64) {
      ret->insts.push_back(format("mpz_set_ui(%s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str()));
    } else {
      set256_tmp(Child(0, computeInfo), ret, dstName);
    }
    ret->insts.push_back(format("mpz_mul_2exp(%s, %s, %d);", dstName.c_str(), dstName.c_str(), n));
    ret->valStr = dstName;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsShr(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];

  if (isConstant) {
    (sign ? s_shr : u_shr)(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), values[0]);  // n(values[0]) == width
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
      ret->valStr = get256(tmp, width);
      ret->opNum = 1;
    } else {
      ret->valStr = format("mpz_get_ui(%s)", tmp.c_str());
      ret->opNum = 1;
    }
  } else {
    printf("%d = %s(%d) >> %d\n", width, ChildInfo(0, valStr).c_str(), ChildInfo(0, width), n);
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = values[0];
  Assert(n >= 0, "child width %d is less than current width %d", Child(0, width), values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_head(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n);
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(n) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    std::string tmp = newMpzTmp();
    ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", tmp.c_str(), ChildInfo(0, valStr).c_str(), n));
    if (width > 64) ret->valStr = get256(tmp, width);
    else ret->valStr = format("mpz_get_ui(%s)", tmp.c_str());
    ret->opNum = 1;
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

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
  bool isConstant = ChildInfo(0, status) == VAL_CONSTANT;
  int n = MIN(width, values[0]);

  if (isConstant) {
    if (sign) TODO();
    u_tail(ret->consVal, ChildInfo(0, consVal), ChildInfo(0, width), n); // u_tail remains the last n bits
    ret->setConsStr();
  } else if (childBasic && enodeBasic) {
    if (Child(0, sign)) {
      ret->valStr = "(" + Cast(width, sign) + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }  else {
      ret->valStr = "(" + ChildInfo(0, valStr) + " & " + bitMask(n) + ")";
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    if (width <= 64) {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", ChildInfo(0, valStr).c_str(), bitMask(n).c_str());
    } else {
      ret->valStr = format("(%s & %s)", get256(ChildInfo(0, valStr), width).c_str(), bitMask(n).c_str());
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic) {
    std::string tmpMpz = newMpzTmp();
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", dstName.c_str(), addMask(width).c_str(), ChildInfo(0, valStr).c_str()));
    ret->valStr = dstName;
  } else {
    TODO();
  }
  return ret;
}

void infoBits(valInfo* ret, ENode* enode, valInfo* childInfo) {
  bool childBasic = childInfo->width <= BASIC_WIDTH;
  bool enodeBasic = enode->width <= BASIC_WIDTH;
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
  } else if (childBasic && enodeBasic) {
    std::string shift;
    if (lo == 0) {
      if (childInfo->sign) shift = Cast(childInfo->width, childInfo->sign) + childInfo->valStr;
      else shift = childInfo->valStr;
    } else {
      if (childInfo->sign) shift = "(" + Cast(childInfo->width, childInfo->sign) + childInfo->valStr + " >> " + std::to_string(lo) + ")";
      else shift = "(" + childInfo->valStr + " >> " + std::to_string(lo) + ")";
    }
    ret->valStr = "(" + shift + " & " + bitMask(w) + ")";
    ret->opNum = childInfo->opNum + 1;
  } else if (!childBasic && enodeBasic) {
    std::string mpzTmp1 = newMpzTmp();
    std::string mpzTmp2 = newMpzTmp(); // mask
    std::string shiftVal = mpzTmp1;
    if (lo != 0) {
      ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", mpzTmp1.c_str(), childInfo->valStr.c_str(), lo));
    } else {
      shiftVal = childInfo->valStr;
    }
    if (w > 64) {
      ret->insts.push_back(format("mpz_and(%s, %s, %s);", mpzTmp2.c_str(), shiftVal.c_str(), addMask(enode->width).c_str()));
      ret->valStr = get256(mpzTmp2, w);
    } else {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", shiftVal.c_str(), bitMask(w).c_str());
    }
    ret->opNum = 1;
  } else if (!childBasic && !enodeBasic){
    std::string mpzTmp1 = newMpzTmp();
    std::string shiftVal = mpzTmp1;
    if (lo != 0) {
      ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", mpzTmp1.c_str(), childInfo->valStr.c_str(), lo));
    } else {
      shiftVal = childInfo->valStr;
    }
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", mpzTmp1.c_str(), shiftVal.c_str(), addMask(enode->width).c_str()));
    ret->valStr = mpzTmp1;
    ret->opNum = 0;
  } else {
    TODO();
  }
}

valInfo* ENode::instsBits(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
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
  } else if (childBasic && enodeBasic) {
    std::string shift;
    if (lo == 0) {
      if (Child(0, sign)) shift = Cast(ChildInfo(0, width), Child(0, sign)) + ChildInfo(0, valStr);
      else shift = ChildInfo(0, valStr);
    } else {
      if (Child(0, sign)) shift = "(" + Cast(ChildInfo(0, width), Child(0, sign)) + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
      else shift = "(" + ChildInfo(0, valStr) + " >> " + std::to_string(lo) + ")";
    }
    ret->valStr = "(" + shift + " & " + bitMask(w) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
    for (valInfo* info : ChildInfo(0, memberInfo)) {
      if (info) {
        valInfo* memInfo = info->dup();
        memInfo->width = ret->width;
        memInfo->sign = ret->sign;
        infoBits(memInfo, this, info);
        ret->memberInfo.push_back(memInfo);
      } else {
        ret->memberInfo.push_back(nullptr);
      }
    }
  } else if (!childBasic && enodeBasic) {
    std::string mpzTmp1 = newMpzTmp();
    std::string shiftVal = mpzTmp1;
    if (lo != 0) {
      ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", mpzTmp1.c_str(), ChildInfo(0, valStr).c_str(), lo));
    } else {
      shiftVal = ChildInfo(0, valStr);
    }
    if (w > 64) {
      std::string mpzTmp2 = newMpzTmp(); // mask
      ret->insts.push_back(format("mpz_and(%s, %s, %s);", mpzTmp2.c_str(), shiftVal.c_str(), addMask(width).c_str()));
      ret->valStr = get256(mpzTmp2, w);
    } else {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", shiftVal.c_str(), bitMask(w).c_str());
    }
    ret->opNum = 1;
  } else if (!childBasic && !enodeBasic){
    std::string mpzTmp1 = newMpzTmp();
    std::string shiftVal = mpzTmp1;
    if (lo != 0) {
      ret->insts.push_back(format("mpz_tdiv_q_2exp(%s, %s, %d);", mpzTmp1.c_str(), ChildInfo(0, valStr).c_str(), lo));
    } else {
      shiftVal = ChildInfo(0, valStr);
    }
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", mpzTmp1.c_str(), shiftVal.c_str(), addMask(width).c_str()));
    ret->valStr = mpzTmp1;
    ret->opNum = 0;
  } else {
    TODO();
  }
  return ret;
}

valInfo* ENode::instsBitsNoShift(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  for (ENode* childNode : child) ret->mergeInsts(childNode->computeInfo);

  bool childBasic = ChildInfo(0, width) <= BASIC_WIDTH;
  bool enodeBasic = width <= BASIC_WIDTH;
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
  } else if (childBasic && enodeBasic) {
    ret->valStr = "(" + ChildInfo(0, valStr) + " & " + rangeMask(hi, lo) + ")";
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && enodeBasic) {
    if (w > 64) {
      ret->valStr = format ("(%s & %s)", get256(ChildInfo(0, valStr), w).c_str(), rangeMask(hi, lo).c_str());
    } else {
      ret->valStr = format("(mpz_get_ui(%s) & %s)", ChildInfo(0, valStr).c_str(), rangeMask(hi, lo).c_str());
    }
    ret->opNum = ChildInfo(0, opNum) + 1;
  } else if (!childBasic && !enodeBasic){
    std::string dstName = isRoot ? lvalue : newMpzTmp();
    ret->insts.push_back(format("mpz_and(%s, %s, %s);", dstName.c_str(), ChildInfo(0, valStr).c_str(), addMask(hi, lo).c_str()));
    ret->valStr = dstName;
    ret->opNum = 0;
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
  if (sign) {
    s_asSInt(ret->consVal, ret->consVal, width);
    ret->setConsStr();
  }
  return ret;
}

valInfo* ENode::instsReadMem(Node* node, std::string lvalue, bool isRoot) {
  valInfo* ret = computeInfo;
  Assert(node->type == NODE_MEM_MEMBER, "invalid type %d", node->type);
  Node* memory = node->parent->parent;
  ret->valStr = memory->name + "[" + ChildInfo(0, valStr) + "]";
  for (int i = 0; i < memory->dimension.size(); i ++) {
    computeInfo->valStr += "[i" + std::to_string(i) + "]";
  }

  if (memory->width > width) {
    if (memory->width > 128 && width < 64)
      ret->valStr = format("(mpz_get_ui(%s) & %s)", ret->valStr.c_str(), bitMask(width).c_str());
    else if (memory->width < 128)
      ret->valStr = format("(%s & %s)", ret->valStr.c_str(), bitMask(width).c_str());
    else TODO();
  }
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

valInfo* ENode::instsReset(Node* node, std::string lvalue, bool isRoot) {
  Assert(node->type == NODE_REG_SRC, "node %s is not reg_src\n", node->name.c_str());

  if (ChildInfo(0, status) == VAL_CONSTANT) {
    if (mpz_sgn(ChildInfo(0, consVal)) == 0) {
      computeInfo->status = VAL_EMPTY_SRC;
      return computeInfo;
    } else {
      computeInfo = Child(1, computeInfo); //node->resetVal->getRoot()->compute(node, node->name, isRoot);
    }
    return computeInfo;
  }

  valInfo* resetVal = Child(1, computeInfo);//node->resetVal->getRoot()->compute(node, node->name, isRoot);
  std::string ret;
  if (node->isArray() && isSubArray(lvalue, node)) {
      ret = arrayCopy(lvalue, node, resetVal);
  } else {
    if (node->width > BASIC_WIDTH) {
      if (resetVal->status == VAL_CONSTANT) {
        if (mpz_cmp_ui(resetVal->consVal, MAX_U64) > 0) TODO();
        ret = format("mpz_set_ui(%s, %s);", lvalue.c_str(), resetVal->valStr.c_str());
      } else {
        ret = format("mpz_set(%s, %s);", lvalue.c_str(), resetVal->valStr.c_str());
      }
    } else {
      ret = format("%s = %s;", lvalue.c_str(), resetVal->valStr.c_str());
    }
  }

  computeInfo->valStr = format("if(%s) { %s %s }", ChildInfo(0, valStr).c_str(), ret.c_str(), ASSIGN_LABLE.c_str());
  computeInfo->fullyUpdated = false;
  computeInfo->opNum = -1;
  computeInfo->sameConstant = resetVal->sameConstant;
  return computeInfo;
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
            for (int i = 0; i < nodePtr->dimension.size() - getChildNum(); i ++) {
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
        for (int i = 0; i < nodePtr->dimension.size() - getChildNum(); i ++) {
          computeInfo->valStr += "[i" + std::to_string(i) + "]";
        }
      }
    } else {
      computeInfo = nodePtr->compute()->dup();
      if (child.size() != 0) {
        valInfo* indexInfo = computeInfo->dup();
        computeInfo = indexInfo;
        for (ENode* childENode : child)
          computeInfo->valStr += childENode->computeInfo->valStr;
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
        else {
          computeInfo->valStr = format("(%s & %s)", computeInfo->valStr.c_str(), bitMask(width).c_str());
          computeInfo->width = width;
        }
      }
      if (computeInfo->status == VAL_CONSTANT) ;
      else if (sign && computeInfo->width < width && computeInfo->status == VAL_VALID) { // sign extend
        int extendedWidth = widthBits(width);
        int shiftBits = extendedWidth - computeInfo->width;
        if (extendedWidth != width)
          computeInfo->valStr = format("(((%s%s%s << %d) >> %d) & %s)", Cast(width, true).c_str(), Cast(computeInfo->width, true).c_str(), computeInfo->valStr.c_str(), shiftBits, shiftBits, bitMask(width).c_str());
        else
          computeInfo->valStr = format("((%s%s%s << %d) >> %d)", Cast(width, true).c_str(), Cast(computeInfo->width, true).c_str(), computeInfo->valStr.c_str(), shiftBits, shiftBits);
        computeInfo->opNum = 1;
        computeInfo->width = width;
      } else if (width > BASIC_WIDTH) {
        std::string mpz = newMpzTmp();
        if (computeInfo->width <= 64) {
          computeInfo->insts.push_back(format("mpz_set_ui(%s, %s);", mpz.c_str(), computeInfo->valStr.c_str()));
          computeInfo->width = width;
        } else if (computeInfo->width <= BASIC_WIDTH) {
          set256_tmp(computeInfo, computeInfo, mpz);
          computeInfo->width = width;
        }
        computeInfo->valStr = mpz;
      }
    } else if (!IS_INVALID_LVALUE(lvalue)) { // info->width > BASIC_WIDTH
      if (computeInfo->status == VAL_CONSTANT) {
        /* do nothing */
      } else if (width <= 64) {
        computeInfo->valStr = format("mpz_get_ui(%s)", computeInfo->valStr.c_str());
      } else if (width <= BASIC_WIDTH) {
        computeInfo->valStr = get256(computeInfo->valStr, width);
      }
      if (computeInfo->width > width && width <= BASIC_WIDTH) {
        if (computeInfo->status == VAL_CONSTANT) {
          computeInfo->width = width;
          computeInfo->updateConsVal();
        }
        else {
          computeInfo->valStr = format("(%s & %s)", computeInfo->valStr.c_str(), bitMask(width).c_str());
          computeInfo->width = width;
        }
      }
      computeInfo->opNum = 1;
    }

    for (ENode* childNode : child) computeInfo->mergeInsts(childNode->computeInfo);
    MUX_DEBUG(printf("  %p(node) %s width %d info->width %d status %d val %s sameConstant %d opNum %d instsNum %ld\n", this, n->name.c_str(), width, computeInfo->width, computeInfo->status, computeInfo->valStr.c_str(), computeInfo->sameConstant, computeInfo->opNum, computeInfo->insts.size()));
    for (size_t i = 0; i < computeInfo->memberInfo.size(); i ++) {
      if (computeInfo->memberInfo[i]) {
        MUX_DEBUG(printf("idx %ld %s\n", i, computeInfo->memberInfo[i]->valStr.c_str()));
      }
    }
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
    case OP_INVALID: instsInvalid(n, lvalue, isRoot); break;
    case OP_RESET: instsReset(n, lvalue, isRoot); break;
    case OP_PRINTF: instsPrintf(); break;
    case OP_ASSERT: instsAssert(); break;
    default:
      Panic();
  }
  MUX_DEBUG(printf("  %p %s op %d width %d val %s status %d sameConstant %d instsNum %ld opNum %d\n", this, n->name.c_str(), opType, width, computeInfo->valStr.c_str(), computeInfo->status, computeInfo->sameConstant, computeInfo->insts.size(), computeInfo->opNum));
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
  mpzTmpNum = &super->mpzTmpNum;
  MUX_DEBUG(printf("compute %s lcoalTmp %d mpzTmp %d\n", name.c_str(), *localTmpNum, *mpzTmpNum));
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
    tmp_pop();
    return computeInfo;
  }

  bool isRoot = anyExtEdge() || next.size() != 1 || isReset();
  if (isArrayMember) {
    for (Node* nextNode : next) {
      if (nextNode->isArray()) isRoot = true;
    }
  }
  valInfo* ret = nullptr;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    ExpTree* tree = assignTree[i];
    valInfo* info = tree->getRoot()->compute(this, name, isRoot);
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
  fullyUpdated = updated;
  if (!ret) ret = assignTree.back()->getRoot()->compute(this, name, isRoot)->dup();
  Assert(ret, "empty info in %s\n", name.c_str());
  if (ret->status == VAL_INVALID || ret->status == VAL_EMPTY) ret->setConstantByStr("0");
  if (ret->status == VAL_EMPTY_SRC && assignTree.size() == 1) status = DEAD_SRC;
  MUX_DEBUG(printf("compute [%s(%d), %d] = %s width %d info->width %d status %d sameCons %d insts(%ld, %ld) isRoot %d %p\n", name.c_str(), type, ret->status, ret->valStr.c_str(), width, ret->width, status, ret->sameConstant, ret->insts.size(), insts.size(), isRoot, this));
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
          port->get_member(READER_DATA)->setConstantZero();
          port->get_member(READER_ADDR)->setConstantZero();
          for (Node* next : port->get_member(READER_DATA)->next) {
            if (next->computeInfo) addRecompute(next);
          }
        }
      } else if (port->type == NODE_WRITER) {
        if (this == port->get_member(WRITER_EN) || this == port->get_member(WRITER_MASK)) {
          port->get_member(WRITER_ADDR)->setConstantZero();
          port->get_member(WRITER_DATA)->setConstantZero();
          if (this != port->get_member(WRITER_EN)) port->get_member(WRITER_EN)->setConstantZero();
          if (this != port->get_member(WRITER_MASK)) port->get_member(WRITER_MASK)->setConstantZero();
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
  } else if (isRoot || assignTree.size() > 1 || ret->opNum < 0){  // TODO: count validInfoNum, replace assignTree by validInfuNum
    ret->valStr = name;
    ret->opNum = 0;
    ret->status = VAL_VALID;
  } else {
    if (ret->width > width) ret->valStr = format("(%s & %s)", ret->valStr.c_str(), bitMask(width).c_str());
    ret->valStr = upperCast(width, ret->width, sign) + ret->valStr;
    status = MERGED_NODE;
  }
  ret->width = width;
  ret->sign = sign;
  computeInfo = ret;
  for (ExpTree* tree : assignTree) {
    for (std::string inst : tree->getRoot()->computeInfo->insts) insts.push_back(inst);
  }
  tmp_pop();
  MUX_DEBUG(printf("compute [%s(%d), %d] = %s status %d %p\n", name.c_str(), type, ret->status, ret->valStr.c_str(), status, this));
  if (needRecompute) recomputeAllNodes();
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
  for (ExpTree* tree : arrayVal) {
    if (tree)
      tree->clearInfo();
  }
  insts.clear();
  for (ExpTree* tree : assignTree) tree->clearInfo();
  compute();
  bool recomputeNext = false;
  if (prevVal->valStr != computeInfo->valStr) {
    recomputeNext = true;
  }
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
  if (info->opNum < 0) {
    insts.push_back(info->valStr);
  } else if (isSubArray(lvalue, this)) {
    if (width <= BASIC_WIDTH && info->width <= width) {
      if (info->status == VAL_CONSTANT)
        insts.push_back(format("memset(%s, %s, sizeof(%s));", lvalue.c_str(), info->valStr.c_str(), lvalue.c_str()));
      else {
        insts.push_back(arrayCopy(lvalue, this, info));
      }
    } else if (width < BASIC_WIDTH && info->width > width) {
      TODO();
    } else {
      insts.push_back(arrayCopy(lvalue, this, info));;
    }
  } else {
    if (width <= BASIC_WIDTH) {
      if (info->width > width) info->valStr = format("(%s & %s)", info->valStr.c_str(), bitMask(width).c_str());
      if (sign && width != info->width) insts.push_back(format("%s = %s%s;", lvalue.c_str(), Cast(info->width, info->sign).c_str(), info->valStr.c_str()));
      else insts.push_back(format("%s = %s;", lvalue.c_str(), info->valStr.c_str()));

    } else {
      if (info->status == VAL_CONSTANT) {
        if (mpz_cmp_ui(info->consVal, MAX_U64) > 0) TODO();
        insts.push_back(format("mpz_set_ui(%s, %s);", lvalue.c_str(), info->valStr.c_str()));
      } else
        insts.push_back(format("mpz_set(%s, %s);", lvalue.c_str(), info->valStr.c_str()));
    }
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
    if (tree->getlval()) {
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
      for (std::string inst : lindex->insts) insts.push_back(inst);
    } else {
      display();
      TODO();
    }
    valInfo* info = tree->getRoot()->compute(this, lvalue, true);
    for (std::string inst : info->insts) insts.push_back(inst);
    info->insts.clear();
    finalConnect(lvalue, info);
  }

  for (ExpTree* tree : arrayVal) {
      valInfo* lindex = nullptr;
      if (tree->getlval()) {
        lindex = tree->getlval()->compute(this, INVALID_LVALUE, false);
        std::string lvalue = lindex->valStr;
        valInfo* info = tree->getRoot()->compute(this, lvalue, false);
        for (std::string inst : lindex->insts) insts.push_back(inst);
        for (std::string inst : info->insts) insts.push_back(inst);
        info->insts.clear();
        finalConnect(lindex->valStr, info);
        std::tie(lindex->beg, lindex->end) = tree->getlval()->getIdx(this);
        if (lindex->beg < 0) anyVarIdx = true;
        else {
          for (int i = lindex->beg; i <= lindex->end; i ++) {
            if (allIdx.find(i) != allIdx.end()) anyVarIdx = true;
            allIdx.insert(i);
          }
        }
      } else {
        TODO();
      }
  }
  bool updated = false;
  for (size_t i = 0; i < assignTree.size(); i ++) {
    if (assignTree[i]->getRoot()->computeInfo->fullyUpdated) {
      updated = true;
      break;
    }
  }
  for (ExpTree* tree : arrayVal) updated |= tree->getRoot()->computeInfo->fullyUpdated;
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

    for (ExpTree* tree : arrayVal) {
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
          if (width > BASIC_WIDTH) {
            if (mpz_cmp_ui(computeInfo->memberInfo[i]->consVal, MAX_U64) > 0)
              insts.push_back(format("mpz_set_str(%s%s, \"%s\", 16);", name.c_str(), idx2Str(this, i, 0).c_str(), mpz_get_str(NULL, 16, computeInfo->memberInfo[i]->consVal)));
            else
              insts.push_back(format("mpz_set_ui(%s%s, %s);", name.c_str(), idx2Str(this, i, 0).c_str(), computeInfo->memberInfo[i]->valStr.c_str()));
          } else {
            insts.push_back(format("%s%s = %s;", name.c_str(), idx2Str(this, i, 0).c_str(), computeInfo->memberInfo[i]->valStr.c_str()));
          }
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

void graph::instsGenerator() {
  std::set<Node*> s;
  std::set<Node*> s_array;
  for (SuperNode* super : sortedSuper) {
    for (Node* n : super->member) {
      if (n->dimension.size() != 0) {
        n->compute();
        s_array.insert(n);
      } else {
        n->compute();
        s.insert(n);
      }
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
    if (info->opNum < 0) {
      reg->resetInsts.push_back(info->valStr);
    } else if (reg->isArray()) {
      if (info->status == VAL_CONSTANT && reg->width <= BASIC_WIDTH)
        reg->resetInsts.push_back(format("memset(%s, %s, sizeof(%s));", reg->name.c_str(), info->valStr.c_str(), reg->name.c_str()));
      else {
        reg->resetInsts.push_back(arrayCopy(reg->name, reg, info));
      }
    } else {
      if (reg->width <= BASIC_WIDTH)
        reg->resetInsts.push_back(reg->name + " = " + info->valStr + ";");
      else
        reg->resetInsts.push_back(format("mpz_set(%s, %s);", reg->name.c_str(), info->valStr.c_str()));
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

  for (SuperNode* super : sortedSuper) maxTmp = MAX(maxTmp, super->mpzTmpNum);

  for (Node* n: s_array) srcUpdateDst(n);

  /* generate assignment instrs */
  for (Node* n : s) {
    if (n->status == CONSTANT_NODE) {
      if ((n->type == NODE_REG_DST && !n->getSrc()->regSplit && n->getSrc()->status != CONSTANT_NODE) || (n->type == NODE_REG_UPDATE && n->regNext->status != CONSTANT_NODE)) {
        if (n->width <= BASIC_WIDTH)
          n->insts.push_back(n->name + " = " + n->computeInfo->valStr + ";");
        else
          n->insts.push_back(format("mpz_set_str(%s, \"%s\", 16);", n->name.c_str(), mpz_get_str(NULL, 16, n->computeInfo->consVal)));
        n->status = VALID_NODE;
      }
    }
    if (n->status == MERGED_NODE || n->status == CONSTANT_NODE) continue;
    for (ExpTree* tree : n->assignTree) {
      valInfo* assignInfo = tree->getRoot()->computeInfo;
      if (assignInfo->status == VAL_VALID) {
        if (assignInfo->opNum < 0) {
            n->insts.push_back(assignInfo->valStr);
        } else if (assignInfo->opNum > 0 || assignInfo->valStr != n->name) {
          if (n->width <= BASIC_WIDTH)
            n->insts.push_back(n->name + " = " + assignInfo->valStr + ";");
          else
            n->insts.push_back(format("mpz_set(%s, %s);", n->name.c_str(), assignInfo->valStr.c_str()));
        }
        if (n->type == NODE_REG_SRC && n->reset == ASYRESET && n->regSplit && n->getDst()->status == VALID_NODE) {
          srcUpdateDst(n);
        }
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
