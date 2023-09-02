/**
 * @file instsGenerator.cpp
 * @brief instsGenerator
 */

#include <map>
#include <gmp.h>
#include <stack>
#include <tuple>

#include "common.h"
#include "graph.h"
#include "opFuncs.h"

/* internal computing functions, used for constant nodes or intermediate values */
#define EXPR1INT_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt, mp_bitcnt_t n)
#define EXPR1_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt)
#define EXPR2_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src1, mp_bitcnt_t bitcnt1, mpz_t & src2, mp_bitcnt_t bitcnt2)

static std::map<std::string, std::tuple<EXPR1INT_FUNC_TYPE, EXPR1INT_FUNC_TYPE>> expr1int1Map = {
    {"pad",   {u_pad, s_pad}},
    {"shl",   {u_shl, invalidExpr1Int1}},
    {"shr",   {u_shr, s_shr}},
    {"head",  {u_head, invalidExpr1Int1}},
    {"tail",  {u_tail, invalidExpr1Int1}},
};

static std::map<std::string, std::tuple<EXPR1_FUNC_TYPE, EXPR1_FUNC_TYPE>> expr1Map = {
    {"asUInt",    {u_asUInt, invalidExpr1}},
    {"asSInt",    {invalidExpr1, s_asSInt}},
    {"asClock",   {u_asClock, invalidExpr1}},
    {"asAsyncReset", {invalidExpr1, invalidExpr1}},
    {"cvt",       {u_cvt, s_cvt}},
    {"neg",       {invalidExpr1, s_neg}},
    {"not",       {u_not, invalidExpr1}},
    {"andr",      {u_andr, invalidExpr1}},
    {"orr",       {u_orr, invalidExpr1}},
    {"xorr",      {u_xorr, invalidExpr1}},
};

static std::map<std::string, std::tuple<EXPR2_FUNC_TYPE, EXPR2_FUNC_TYPE>> expr2Map = {
    {"add",   {us_add, us_add}},
    {"sub",   {us_sub, us_sub}},
    {"mul",   {us_mul, us_mul}},
    {"div",   {us_div, us_div}},
    {"rem",   {us_rem, us_rem}},
    {"lt",    {us_lt, us_lt}},
    {"leq",   {us_leq, us_leq}},
    {"gt",    {us_gt, us_gt}},
    {"geq",   {us_geq, us_geq}},
    {"eq",    {us_eq, us_eq}},
    {"neq",   {us_neq, us_neq}},
    {"dshl",  {u_dshl, invalidExpr2}},
    {"dshr",  {u_dshr, s_dshr}},
    {"and",   {u_and, invalidExpr2}},
    {"or",    {u_ior, invalidExpr2}},
    {"xor",   {u_xor, invalidExpr2}},
    {"cat",   {u_cat, invalidExpr2}},
};

class mpz_wrap {
 public:
  mpz_wrap() { mpz_init(a); }
  mpz_t a;
};

#define add_insts_1expr(n, func, dst, expr1) n->insts.push_back(func + "(" + dst + ", " + expr1 + ");")

#define add_insts_2expr(n, func, dst, expr1, expr2) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ");")

#define add_insts_3expr(n, func, dst, expr1, expr2, expr3) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ");")

#define add_insts_4expr(n, func, dst, expr1, expr2, expr3, expr4) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ", " + expr4 + ");")

#define NEW_TMP ("__tmp__" + std::to_string(++tmpIdx))
#define FUNC_NAME(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s)
#define FUNC_BASIC(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s + "_basic")
#define FUNC_UI(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s + "_ui")
#define FUNC_UI_R(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s + "_ui_r")
#define FUNC_UI2(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s + "_ui2")
#define FUNC_I(sign, s) ((sign ? std::string("si_") : std::string("ui_")) + s)
#define FUNC_I_UI(sign, s) ((sign ? std::string("si_") : std::string("ui_")) + s + "_ui")
#define FUNC_SUI_R(fsign, vsign, s) ((fsign ? std::string("s_") : std::string("u_")) + s + (vsign ? "_si_r" : "_ui_r"))
#define FUNC_SUI_L(fsign, vsign, s) ((fsign ? std::string("s_") : std::string("u_")) + s + (vsign ? "_si_l" : "_ui_l"))
#define MAX_U64 0xffffffffffffffff
#define valEqualsZero(width, str) (width > 64 ? "mpz_sgn(" + str + ")" : str)
#define valUI(width, str) (width > 64 ? "mpz_get_ui(" + str + ")" : str)
#define MPZ_SET(sign) std::string(sign ? "mpz_set_si" : "mpz_set_ui")
#define VAR_NAME(node) (node->name)
class InterVal {
  public:
    InterVal(int _w = -1, bool _sign = 0, bool _cons = 0, int _opNum = -1, std::string _value = "", const char* _mpzStr = NULL, int _base = 16) {
      mpz_init(mpzVal);
      width = _w;
      sign = _sign;
      isCons = _cons;
      opNum = _opNum;
      value = _value;
      if (_mpzStr) mpz_set_str(mpzVal, _mpzStr, _base);
      indexEnd = false;
    }
    void alignValueMpz() {
      char* str = mpz_get_str(NULL, 16, mpzVal);
      value = std::string("0x") + str;
      free(str);
    }
    int width;
    bool sign;
    bool isCons;
    bool indexEnd;
    bool isArray = false;
    int entryNum = 0;
    int opNum;
    std::string value;
    mpz_t mpzVal;
    std::vector<int> index;
};

#define valType std::tuple<int, bool, bool, int, std::string, mpz_wrap*>

#define valWidth(entry) std::get<0>(entry)
#define valSign(entry)  std::get<1>(entry)
#define valCons(entry)  std::get<2>(entry)
#define valOpNum(entry) std::get<3>(entry)
#define valValue(entry) std::get<4>(entry)
#define valMpzWrap(entry) std::get<5>(entry)
#define valMpz(entry)   std::get<5>(entry)->a
#define valIsIndex(entry) (valOpNum(entry) == -1)

int p_stoi(const char* str);
std::string cons2str(std::string s);
std::pair<int, std::string> strBaseAll(std::string s);
std::string to_hex_string(unsigned long x);
void computeNode(Node* node, bool nodeEnd);

static std::vector<InterVal*> interVals;
static std::vector<std::string> whenStr;
static bool topValid = false;
static bool stmtValid = false;
static int tmpIdx = 0;
static int valSize = 0;

static std::map<std::string, std::string> opMap = {
  {"add", "+"},
  {"sub", "-"},
  {"mul", "*"},
  {"div", "/"},
  {"rem", "%"},
  {"lt", "<"},
  {"leq", "<="},
  {"gt", ">"},
  {"geq", ">="},
  {"eq", "=="},
  {"neq", "!="},
  // {"dshl", "<<"},
  {"dshr", ">>"},
  {"and", "&"},
  {"or", "|"},
  {"xor", "^"},
  // {"shl", "<<"},
  {"shr", ">>"},
  {"asClock", "0!="},
  {"neg", "-"},
  // {"not", "~"},
  {"cvt", ""},
  {"orr", "0 != "}
};

static void deleteAndPop() {
  delete interVals.back();
  interVals.pop_back();
}

static void setConsValName(PNode* op) {
  interVals.back()->width = op->width;
  interVals.back()->sign = op->sign;
  interVals.back()->isCons = true;
  interVals.back()->opNum = 1;
  char* str = mpz_get_str(NULL, 16, interVals.back()->mpzVal);
  interVals.back()->value = std::string("0x") + str;
  free(str);
}

static void inline setConstantNode(Node* node) {
  node->status = CONSTANT_NODE;
  char * str = mpz_get_str(NULL, 16, interVals.back()->mpzVal);
  node->workingVal->consVal = std::string("0x") + str;
  free(str);
}

void setConsArrayPrev(Node* node) {
  int startIdx = 0, len = 1;
  std::string consStr;
  if (interVals.size() != 0 && interVals.back()->indexEnd) {
    for (size_t i = 0; i < interVals.back()->index.size(); i ++) {
      int idx = interVals.back()->index[i];
      Assert(idx >= 0, "Invalid fix idx for node %s\n", node->name.c_str());
      startIdx = startIdx * node->dimension[i] + idx;
    }
    for (size_t i = interVals.back()->index.size(); i < node->dimension.size(); i ++) {
      startIdx *= node->dimension[i];
      len *= node->dimension[i];
    }
    for (int i = 0; i < len; i ++) {
      if (i != 0) consStr += ", ";
      consStr += node->consArray[startIdx + i];
    }
    deleteAndPop();
  } else {
    len = node->entryNum;
    consStr = node->workingVal->consVal;
  }
  interVals.push_back(new InterVal(node->width, node->sign, true, 1, consStr, len == 1 ? consStr.c_str() : NULL));
  interVals.back()->isArray = len > 1;
  interVals.back()->entryNum = len;
}

static void setPrev(Node* node, int& prevIdx) {
  Operand* operand = node->workingVal->operands[prevIdx];
  if (!operand) {     // for empty when/else stmt
    TODO(); // not needed
    // valName.push_back(valType(node->width, node->sign, true, 1, node->name, NULL));
  } else {
    Node* operandNode = operand->node;
    // std::cout << "operand: " << prevIdx << " " << operandNode->name << "(" << operandNode->status << ")" << " node: " << node->name << " " << operandNode->clusId << " " << operandNode->id << std::endl;
    if (operandNode->status == CONSTANT_NODE && operandNode->dimension.size() == 0) {
      if (operandNode->workingVal->consVal.length() == 0) {
        if (operandNode->workingVal->ops.size() == 0 && operandNode->workingVal->operands.size() == 0) {
          interVals.push_back(new InterVal(operandNode->width, operandNode->sign, true, 1, "0x0", "0"));
        } else {
          size_t size = interVals.size();
          computeNode(operandNode, false);
          Assert(interVals.size() == size + 1, "Invalid size for %s (%ld -> %ld) in %s\n", operandNode->name.c_str(), size, interVals.size(), node->name.c_str());
        }
      } else {
        interVals.push_back(new InterVal(operandNode->width, operandNode->sign, true, 1, "0x" + operandNode->workingVal->consVal, operandNode->workingVal->consVal.c_str()));
      }
    } else if (operandNode->status == CONSTANT_NODE) { // constant array
      setConsArrayPrev(operandNode);
    } else if (operandNode->id != operandNode->clusId) {
      size_t size = interVals.size();
      computeNode(operandNode, false);
      Assert(interVals.size() == size + 1, "Invalid size for %s (%ld -> %ld) in %s\n", operandNode->name.c_str(), size, interVals.size(), node->name.c_str());
    } else {
      std::string name = operandNode->name;
      Node* nextNode = NULL;
      if (interVals.size() > 0 && interVals.back()->indexEnd && interVals.back()->index.size() != 0) {
        if (operandNode->arraySplit) {
          bool isFixIndex = true;
          int fixIdx = 0;
          int memberIdx = 0;
          for (size_t idx = 0; idx < interVals.back()->index.size(); idx ++) {
            if(idx < 0) {
              isFixIndex = false;
              break;
            } else {
              fixIdx = fixIdx * (operandNode->dimension[idx]) + interVals.back()->index[idx];
              memberIdx = memberIdx * (operandNode->dimension[idx] + 1) + interVals.back()->index[idx];
            }
          }
          if (isFixIndex) {
            name += "$__" + std::to_string(fixIdx);
            nextNode = operandNode->member[memberIdx];
          } else TODO();
        } else {
          name += interVals.back()->value;
        }
        deleteAndPop();
      }
      if (nextNode) computeNode(nextNode, false);
      else interVals.push_back(new InterVal(operandNode->width, operandNode->sign, false, 1, name, NULL));
    }
  }
  topValid = true;
  prevIdx--;
}

void insts_1expr1int(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  int valWidth = interVals.back()->width;
  if (op->name == "head" || op->name == "tail") n = valWidth - n;
  std::string dstName;
  int dstOpNum = interVals.back()->opNum;
  bool dstCons = interVals.back()->isCons;
  bool result_mpz  = op->width > 64;
  bool operand_mpz = valWidth > 64;

  if (dstCons) {
    std::tuple<EXPR1INT_FUNC_TYPE, EXPR1INT_FUNC_TYPE> funcs = expr1int1Map[op->name];
    (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(interVals.back()->mpzVal, interVals.back()->mpzVal, valWidth, n);
    setConsValName(op);
    if (opIdx == 0) setConstantNode(node);
    return;
  }
  if (result_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (valWidth > 64)
      add_insts_3expr(node, FUNC_NAME(op->sign, op->name), dstName, interVals.back()->value,
                      std::to_string(interVals.back()->width), std::to_string(n));
    else
      add_insts_2expr(node, FUNC_UI(op->sign, op->name), dstName, interVals.back()->value, std::to_string(n));
  } else if (!operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstOpNum ++;
      dstName = "(" + interVals.back()->value + opMap[op->name] + std::to_string(n) + ")";
    } else if (op->name == "pad") {
      if (!op->sign)
        dstName = interVals.back()->value;
      else {
        dstOpNum ++;
        std::string operandName;
        if(interVals.back()->opNum != 1) {
          operandName = op->getChild(0)->name;
          node->insts.push_back(widthUType(valWidth) + " " + operandName + " = " + interVals.back()->value + ";");
        } else
          operandName = interVals.back()->value;
        dstName = "(-((" + nodeType(op) + ")(" + operandName + " >> " + std::to_string(valWidth - 1) +
                  ") << " + std::to_string(valWidth) + ") | " + operandName + ")";
      }
    } else if (op->name == "tail") {
      dstOpNum ++;
      unsigned long mask = n == 64 ? MAX_U64 : (((unsigned long)1 << n) - 1);
      dstName = "(" + interVals.back()->value + " & 0x" + to_hex_string(mask) + ")";
    } else if (op->name == "shl") {
      dstOpNum ++;
      dstName = "(" + UCast(op->width) + interVals.back()->value + " << " + std::to_string(n) + ")";
    } else {
      dstOpNum ++;
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + interVals.back()->value + ", " + std::to_string(n) + ")";
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  } else if (operand_mpz) {
    dstOpNum ++;
    dstName = FUNC_I(op->sign, op->name) + "(" + interVals.back()->value + ", " +
              std::to_string(interVals.back()->width) + ", " + std::to_string(n) + ")";
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  }
  deleteAndPop();
  interVals.push_back(new InterVal(op->width, op->sign, false, dstOpNum, dstName, NULL));
}

void insts_1expr2int(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = interVals.back()->isCons;
  Assert (!op->sign, "signed bits in line %d\n", op->lineno);
  if (dstCons) {
    u_bits(interVals.back()->mpzVal, interVals.back()->mpzVal, interVals.back()->width, p_stoi(op->getExtra(0).c_str()),
                  p_stoi(op->getExtra(1).c_str()));
    setConsValName(op);
    if (opIdx == 0) setConstantNode(node);
    return;
  }
  std::string dstName;
  int dstOpNum = interVals.back()->opNum;
  if (op->width <= 64) {
    if (interVals.back()->width <= 64) {
      int l = p_stoi(op->getExtra(0).c_str()), r = p_stoi(op->getExtra(1).c_str()), w = l - r + 1;
      unsigned long mask = w == 64 ? MAX_U64 : ((unsigned long)1 << w) - 1;
      dstName = "((" + interVals.back()->value + " >> " + std::to_string(r) + ") & 0x" + to_hex_string(mask) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + interVals.back()->value + ", " +
                std::to_string(interVals.back()->width) + ", " + cons2str(op->getExtra(0)) + ", " +
                cons2str(op->getExtra(1)) + ")";
    }
    dstOpNum ++;
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  } else {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    add_insts_4expr(node, FUNC_NAME(op->sign, op->name), dstName, interVals.back()->value,
                    std::to_string(interVals.back()->width), cons2str(op->getExtra(0)), cons2str(op->getExtra(1)));
  }
  deleteAndPop();
  interVals.push_back(new InterVal(op->width, op->sign, false, dstOpNum, dstName, NULL));
}

void insts_2expr(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  int funcSign = op->sign;
  if (op->name == "add" || op->name == "mul" || op->name == "sub" || op->name == "div" || op->name == "rem")
    funcSign = 0;

  int dstOpNum = interVals.back()->opNum + interVals[interVals.size()-2]->opNum;
  int dstCons = interVals.back()->isCons && interVals[interVals.size()-2]->isCons;
  if (dstCons) {
    std::tuple<EXPR2_FUNC_TYPE, EXPR2_FUNC_TYPE> funcs = expr2Map[op->name];
    (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(interVals[interVals.size() - 2]->mpzVal, interVals.back()->mpzVal,
                  interVals.back()->width, interVals[interVals.size() - 2]->mpzVal, interVals[interVals.size() - 2]->width);
    deleteAndPop();
    setConsValName(op);
    if (opIdx == 0) setConstantNode(node);
    return;
  }
  std::string dstName;
  bool result_mpz  = op->width > 64;
  bool left_mpz    = interVals.back()->width > 64;
  bool right_mpz   = interVals[interVals.size() - 2]->width > 64;
  bool operand_mpz = left_mpz && right_mpz;
  if (result_mpz && operand_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (!interVals.back()->isCons && !interVals[interVals.size() - 2]->isCons) {
      add_insts_4expr(node, FUNC_NAME(funcSign, op->name), dstName, interVals.back()->value,
                      std::to_string(interVals.back()->width), interVals[interVals.size() - 2]->value,
                      std::to_string(interVals[interVals.size() - 2]->width));
    } else if (!interVals.back()->isCons && interVals[interVals.size() - 2]->isCons) {
      add_insts_3expr(node, FUNC_UI_R(funcSign, op->name), dstName, interVals.back()->value,
                      interVals[interVals.size() - 2]->value, std::to_string(interVals[interVals.size() - 2]->width));
    } else if (interVals.back()->isCons && !interVals[interVals.size() - 2]->isCons) {
      add_insts_4expr(node, FUNC_SUI_L(funcSign, interVals.back()->sign, op->name), dstName,
                      interVals.back()->value, std::to_string(interVals.back()->width),
                      interVals[interVals.size() - 2]->value, std::to_string(interVals[interVals.size() - 2]->width));
    } else {
      add_insts_4expr(node, FUNC_UI2(funcSign, op->name), dstName, interVals.back()->value,
                      std::to_string(interVals.back()->width), interVals[interVals.size() - 2]->value,
                      std::to_string(interVals[interVals.size() - 2]->width));
    }
  } else if (!result_mpz && !operand_mpz) {
    dstOpNum ++;
    if (opMap.find(op->name) != opMap.end()) {
      if (interVals.back()->sign) {
        std::string typeStr = "(" + widthSType(interVals.back()->width) + ")";
        dstName = "(" + typeStr + interVals.back()->value + opMap[op->name] + typeStr +
                  interVals[interVals.size() - 2]->value + ")";
      } else {
        dstName = "(" + interVals.back()->value + opMap[op->name] + interVals[interVals.size() - 2]->value + ")";
      }
    } else if (op->name == "cat") {
      dstName = "((uint64_t)" + interVals.back()->value + " << " +
                std::to_string(interVals[interVals.size() - 2]->width) + " | " +
                interVals[interVals.size() - 2]->value + ")";
    } else if (op->name == "dshl") {
      dstName = "(" + UCast(op->width) + interVals.back()->value + " << " + interVals[interVals.size() - 2]->value + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + interVals.back()->value + ", " +
                interVals[interVals.size() - 2]->value + ")";
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  } else if (result_mpz && !operand_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (left_mpz) {
      add_insts_3expr(node, FUNC_SUI_R(funcSign, interVals[interVals.size() - 2]->sign, op->name),
                    dstName, interVals.back()->value, interVals[interVals.size() - 2]->value,
                    std::to_string(interVals[interVals.size() - 2]->width));
    } else if (right_mpz) {
      add_insts_4expr(node, FUNC_SUI_L(funcSign, interVals.back()->sign, op->name), dstName,
                    interVals.back()->value, std::to_string(interVals.back()->width),
                    interVals[interVals.size() - 2]->value, std::to_string(interVals[interVals.size() - 2]->width));
    } else {
      node->insts.push_back(MPZ_SET(interVals.back()->sign) + "(" + dstName + ", " + interVals.back()->value + ");");
      add_insts_3expr(node, FUNC_SUI_R(funcSign, interVals[interVals.size() - 2]->sign, op->name), dstName, dstName,
                      interVals[interVals.size() - 2]->value, std::to_string(interVals[interVals.size() - 2]->width));
    }
  } else if (!result_mpz && operand_mpz) {
    dstOpNum ++;
    dstName = FUNC_I(op->sign, op->name) + "(" + interVals.back()->value + ", " + std::to_string(interVals.back()->width) +
              ", " + interVals[interVals.size() - 2]->value + ", " + std::to_string(interVals[interVals.size() - 2]->width) + ")";
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  }
  deleteAndPop();
  deleteAndPop();
  interVals.push_back(new InterVal(op->width, op->sign, false, dstOpNum, dstName, NULL));
}

void insts_1expr(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = interVals.back()->isCons;
  if (dstCons) {
    std::tuple<EXPR1_FUNC_TYPE, EXPR1_FUNC_TYPE> funcs = expr1Map[op->name];
    (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(interVals.back()->mpzVal, interVals.back()->mpzVal, interVals.back()->width);
    setConsValName(op);
    if (opIdx == 0) setConstantNode(node);
    return;
  }
  int dstOpNum = interVals.back()->opNum;
  std::string dstName;
  bool result_mpz  = op->width > 64;
  bool operand_mpz = interVals.back()->width > 64;
  if (result_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    add_insts_2expr(node, (operand_mpz ? FUNC_NAME(op->sign, op->name) : FUNC_UI(op->sign, op->name)),
                    dstName, interVals.back()->value, std::to_string(interVals.back()->width));
  } else if (!operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstName = "(" + opMap[op->name] + interVals.back()->value + ")";
      dstOpNum ++;
    } else if (op->name == "not") {
      unsigned long mask = op->width == 64 ? MAX_U64 : (((unsigned long)1 << op->width) - 1);
      dstName = "(" + interVals.back()->value + " ^ 0x" + to_hex_string(mask) + ")";
      dstOpNum ++;
    } else if (op->name == "asSInt") {
      int shiftBits = widthBits(op->width) - op->width;
      std::string shiftStr = std::to_string(shiftBits);
      dstName = "(((" + widthSType(op->width) + ")(" + interVals.back()->value + " << " + shiftStr + ")) >> " + shiftStr + ")";
    } else if (op->name == "asUInt") {
      unsigned long mask = interVals.back()->width == 64 ? MAX_U64 : (((unsigned long)1 << interVals.back()->width) - 1);
      dstName = "((" + widthUType(op->width) + ")" + interVals.back()->value + " & 0x" + to_hex_string(mask) + ")";
    } else if (op->name == "andr") {
      unsigned long mask = interVals.back()->width == 64 ? MAX_U64 : (((unsigned long)1 << interVals.back()->width) - 1);
      dstName = "(" + interVals.back()->value + " == 0x" + to_hex_string(mask) + ")";
    } else if (op->name == "xorr") {
      dstName = "(__builtin_parity(" + interVals.back()->value + ") & 1)";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + interVals.back()->value + ")";
      dstOpNum ++;
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName + ";");
  } else {
    std::cout << interVals.back()->value << " " << interVals.back()->width << std::endl;
    std::cout << node->name << " " << op->name << std::endl;
    TODO();
  }
  deleteAndPop();
  interVals.push_back(new InterVal(op->width, op->sign, false, dstOpNum, dstName, NULL));
}

bool insts_index(Node* node, int opIdx, int& prevIdx, int rindexBegin) {
  if (!topValid) setPrev(node, prevIdx);

  std::string rindexStr = "[" + interVals.back()->value + "]";
  int index = -1;
  if (interVals.back()->isCons) index = std::stoi(interVals.back()->value);
  deleteAndPop();
  if (rindexBegin) interVals.push_back(new InterVal(-1, -1, false, -1, rindexStr, NULL));
  else interVals.back()->value = rindexStr + interVals.back()->value;
  interVals.back()->index.insert(interVals.back()->index.begin(), index);   // FIXME: 检查一下index，在为变量时用-1表示
  interVals.back()->indexEnd = node->workingVal->ops[opIdx]->lineno == 0;
  topValid = false;
  return node->workingVal->ops[opIdx]->lineno == 0;

}

bool insts_cons_index(Node* node, int opIdx, int& prevIdx, bool rindexBegin) {
  PNode* op = node->workingVal->ops[opIdx];
  if (rindexBegin) interVals.push_back(new InterVal(-1, -1, false, -1, "[" + op->name + "]", NULL));
  else interVals.back()->value = "[" + op->name + "]" + interVals.back()->value;
  topValid = false;
  interVals.back()->index.insert(interVals.back()->index.begin(), std::stoi(op->name));
  interVals.back()->indexEnd = node->workingVal->ops[opIdx]->lineno == 0;

  return node->workingVal->ops[opIdx]->lineno == 0;

}

bool insts_l_index(Node* node, int opIdx, int& prevIdx, std::vector<std::string>& index) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);

  index.push_back(interVals.back()->value);

  deleteAndPop();
  return op->lineno == 0;
}

bool insts_l_cons_index(Node* node, int opIdx, int& prevIdx, std::vector<std::string>& index) {
  PNode* op = node->workingVal->ops[opIdx];
  index.push_back(op->name);
  return op->lineno == 0;
}


void insts_mux(Node* node, int opIdx, int& prevIdx, bool nodeEnd, bool isIf) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = interVals.back()->isCons && interVals[interVals.size() - 2]->isCons && interVals[interVals.size()-3]->isCons;
  if (dstCons) {
    us_mux(interVals[interVals.size() - 3]->mpzVal, interVals.back()->mpzVal, interVals[interVals.size() - 2]->mpzVal, interVals[interVals.size() - 3]->mpzVal);
    deleteAndPop();
    deleteAndPop();
    if (opIdx == 0) setConstantNode(node);
    return;
  }
  int muxType = interVals.back()->isCons ? (interVals.back()->value == "0x0" ? 1 : 2) : 0;
  Assert(!interVals.back()->isCons || interVals.back()->value == "0x0" || interVals.back()->value == "0x1",
              "invalid constant cond %s for node %s\n", interVals.back()->value.c_str(), node->name.c_str());
  std::string cond = interVals.back()->isCons ? interVals.back()->value :
                      valEqualsZero(interVals.back()->width, interVals.back()->value);
  deleteAndPop();
  std::string dstName;
  int dstOpNum = interVals.back()->opNum + interVals[interVals.size() - 2]->opNum;
  int dstWidth = op->type == P_WHEN ? node->width : op->width;
  if (dstWidth > 64) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    std::string cond_true, cond_false;
    if (!interVals.back()->isCons) {
      cond_true = (interVals.back()->opNum == 1 ? "mpz_set(" : "mpz_set_ui(") + dstName + ", " + interVals.back()->value + ")";
    } else {
      if (interVals.back()->value.length() <= 18)
        cond_true = "mpz_set_ui(" + dstName + ", " + interVals.back()->value + ")";
      else
        cond_true = "(void)mpz_set_str(" + dstName + ", \"" + interVals.back()->value.substr(2) + "\", 16)";
    }
    deleteAndPop();

    if (!interVals.back()->isCons) {
      cond_false = (interVals.back()->opNum == 1 ? "mpz_set(" : "mpz_set_ui(") + dstName + ", " + interVals.back()->value + ")";
    } else {
      if (interVals.back()->value.length() <= 18)
        cond_false = "mpz_set_ui(" + dstName + ", " + interVals.back()->value + ")";
      else
        cond_false = "(void)mpz_set_str(" + dstName + ", \"" + interVals.back()->value.substr(2) + "\", 16)";
    }
    if(muxType == 0) {
      if(isIf) {
        std::cout << cond_true << " " << cond_false << " " << node->name << std::endl;
        if (cond_true == node->name && cond_false == node->name) {

        } else if (cond_true == node->name) {
          node->insts.push_back("if (!" + cond + "){\n" + cond_false + ";\n}");
        } else if (cond_false == node->name) {
          node->insts.push_back("if (" + cond + "){\n" + cond_true + ";\n}");
        } else
          node->insts.push_back("if (" + cond + "){\n" + cond_true + ";\n} else {\n" + cond_false + ";\n}");
      } else {
        node->insts.push_back(cond + "? " + cond_true + " : " + cond_false + ";");
      }
    } else if(muxType == 1) node->insts.push_back(cond_false + ";");
    else if(muxType == 2) node->insts.push_back(cond_true + ";");
  } else {
    std::string cond_true, cond_false;
    if (interVals.back()->width > 64) {
      if (interVals.back()->isArray && interVals.back()->isCons) TODO();
      cond_true = ("mpz_get_ui(" + interVals.back()->value + ")");
    } else {
      if (interVals.back()->isArray && interVals.back()->isCons) {
        cond_true = ("memcpy(" + node->name + ", (const " + widthUType(node->width) + "[]){" + interVals.back()->value + "}, "
                    + std::to_string(interVals.back()->entryNum) + "*sizeof(" + widthUType(node->width) + "))");
      } else {
        cond_true = interVals.back()->value;
      }
    }
    deleteAndPop();
    if (interVals.back()->width > 64) {
      if (interVals.back()->isArray && interVals.back()->isCons) TODO();
      cond_false = ("mpz_get_ui(" + interVals.back()->value + ")");
    } else {
      if (interVals.back()->isArray && interVals.back()->isCons) {
        Assert(nodeEnd && opIdx == 0, "%s array copy\n", node->name.c_str());
        cond_false = ("memcpy(" + node->name + ", (" + widthUType(node->width) + "[]){" + interVals.back()->value + "})");
      } else {
        cond_false = interVals.back()->value;
      }
    }

    std::string value;
    if(muxType == 0) {
      if (isIf) {
        if (cond_true == node->name && cond_false == node->name) {

        } else if (cond_true == node->name) {
          value = "if (!" + cond + "){\n" + cond_false + ";\n}";
        } else if (cond_false == node->name) {
          value = "if (" + cond + "){\n" + cond_true + ";\n}";
        } else
          value = "if (" + cond + "){\n" + cond_true + ";\n} else {\n" + cond_false + ";\n}";
      } else {
        value = "(" + cond + "? " + cond_true + " : \n" + cond_false + ")";
      }
    }
    else if(muxType == 1) value = cond_false;
    else if(muxType == 2) value = cond_true;
    if (opIdx == 0 && nodeEnd) {
      if (isIf) {
        if (value.length() > 0)
          node->insts.push_back(value);
      } else
        node->insts.push_back(VAR_NAME(node) + " = " + value + ";");
    }
    dstName = value;
    dstOpNum += muxType == 0;
  }
  deleteAndPop();
  interVals.push_back(new InterVal(op->width, op->sign, false, dstOpNum, dstName, NULL));
}

void insts_intInit(Node* node, int opIdx) {
  int base;
  std::string cons;
  std::tie(base, cons) = strBaseAll(node->workingVal->ops[opIdx]->getExtra(0));
  interVals.push_back(new InterVal(node->workingVal->ops[opIdx]->width, node->workingVal->ops[opIdx]->sign, true, 1, "", cons.c_str(), base));
  interVals.back()->alignValueMpz();
  topValid = true;
}

void insts_printf(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string cond = interVals.back()->isCons ? interVals.back()->value
                                    : valEqualsZero(interVals.back()->width, interVals.back()->value);
  std::string inst = "if(" + cond + ") printf(" + node->workingVal->ops[opIdx]->getExtra(0);
  deleteAndPop();
  for (int i = 0; i < node->workingVal->ops[opIdx]->getChild(2)->getChildNum(); i++) {
    inst += ", " + (interVals.back()->isCons ? interVals.back()->value :
          valUI(interVals.back()->width, interVals.back()->value));
    deleteAndPop();
  }
  node->insts.push_back(inst + ");");
  interVals.push_back(new InterVal(0, 0, 0, 0, "", NULL));
}

void insts_assert(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string pred = interVals.back()->isCons? interVals.back()->value
                  : valEqualsZero(interVals.back()->width, interVals.back()->value);
  deleteAndPop();

  std::string en = interVals.back()->isCons? interVals.back()->value
                  : valEqualsZero(interVals.back()->width, interVals.back()->value);
  deleteAndPop();

  std::string inst = "Assert(!" + en + " || " + pred + ", " + node->workingVal->ops[opIdx]->getExtra(0) + ");";
  node->insts.push_back(inst);
  interVals.push_back(new InterVal(0, 0, 0, 0, "", NULL));
}

void insts_when(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  node->workingVal->ops[opIdx]->width = node->width;
  insts_mux(node, opIdx, prevIdx, nodeEnd, true);
  return;
}

void combineVal(int num) {
  std::string comb;
  for (int j = 0; j < num; j ++) {
    comb = interVals.back()->value + ";\n" + comb;
    deleteAndPop();
  }
  interVals.back()->value += ";\n" + comb;
}

void computeSplitArray(Node* node, bool nodeEnd) {
  if (node->workingVal->ops.size() == 0 || node->workingVal->operands.size() == 0) {
    return;
  }
  TODO();
}

void computeArrayMember(Node* node) {
  if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 0) return;

  int prevIdx = node->workingVal->operands.size() - 1;
  bool indexEnd = false;
  bool rindexBegin = true;
  std::vector<std::string> index;
  for (int i = node->workingVal->ops.size() - 1; i >= 0; i--) {
    if (!node->workingVal->ops[i]) {
      if (!topValid && ! stmtValid) {
        setPrev(node, prevIdx);
      }
      topValid = false;
      stmtValid = false;
      valSize += 1;
      int delta = interVals.size() - valSize;
      Assert(delta >= 0, "interVals.size(%ld) < valSize(%d) %s %d", interVals.size(), valSize, node->name.c_str(), i);
      if (delta > 0) {
        combineVal(delta);
      }
      continue;
    }
    switch (node->workingVal->ops[i]->type) {
      case P_1EXPR1INT: insts_1expr1int(node, i, prevIdx, false); break;
      case P_1EXPR2INT: insts_1expr2int(node, i, prevIdx, false); break;
      case P_2EXPR: insts_2expr(node, i, prevIdx, false); valSize -= 1; break;
      case P_1EXPR: insts_1expr(node, i, prevIdx, false); break;
      case P_EXPR_MUX: insts_mux(node, i, prevIdx, false, false); valSize -= 2; break;
      case P_EXPR_INT_INIT: insts_intInit(node, i); break;
      case P_PRINTF: insts_printf(node, i, prevIdx); break;
      case P_ASSERT: insts_assert(node, i, prevIdx); break;
      case P_WHEN : insts_when(node, i, prevIdx, false); valSize -= 2; break;
      case P_INDEX : rindexBegin = insts_index(node, i, prevIdx, rindexBegin); valSize -=1; break;
      case P_CONS_INDEX : rindexBegin = insts_cons_index(node, i, prevIdx, rindexBegin); break;
      case P_L_INDEX: indexEnd = insts_l_index(node, i, prevIdx, index); valSize -=1; break;
      case P_L_CONS_INDEX: indexEnd = insts_l_cons_index(node, i, prevIdx, index); break;
      default: Assert(0, "Invalid op(%s) with type %d\n", node->workingVal->ops[i]->name.c_str(), node->workingVal->ops[i]->type);
    }

    if (indexEnd) {
      if(!topValid) setPrev(node, prevIdx);
      std::string newValue = node->name;
      for (int i = index.size() - 1; i >= 0; i --) newValue += "[" + index[i] + "]";
      Assert(interVals.size() > 0, "empty interVals in node %s\n", node->name.c_str());
      if (node->width <= 64)
        interVals.back()->value = newValue + " = " + interVals.back()->value;
      else {
        if (interVals.back()->isCons) {
          if (interVals.back()->value.length() <= 18)
            interVals.back()->value = "mpz_set_ui(" + newValue + ", " + interVals.back()->value + ")";
          else
            interVals.back()->value = "mpz_set_str(" + newValue + ", \"" + interVals.back()->value.substr(2) + "\", 16)";
        }
        else
          interVals.back()->value = "mpz_set(" + newValue + ", " + interVals.back()->value + ")";
      }
      indexEnd = false;
      index.erase(index.begin(), index.end());
      topValid = false;
      stmtValid = true;
    }
  }
  node->insts.push_back(interVals.back()->value + ";");
  deleteAndPop();
  stmtValid = false;
  topValid = false;
}

void computeArray(Node* node, bool nodeEnd) {
  if (node->arraySplit) return computeSplitArray(node, nodeEnd);
  for (Node* member : node->member) {
    computeArrayMember(member);
  }
  if (node->workingVal->ops.size() == 0) { // TODO: dirty[matchWay][blockIdx] when_cond_6363
    if (node->workingVal->operands.size() == 0) return;
    Assert(node->workingVal->operands.size() == 1, "Invalid operands size(%ld) for %s\n", node->workingVal->operands.size(),
           node->name.c_str());
    int prevIdx = 0;
    setPrev(node, prevIdx);
    if (!nodeEnd) return;
    if (!interVals.back()->isCons) {
      if (node->width > 64) {
        TODO();
        node->insts.push_back("mpz_set(" + node->name + ", " + interVals.back()->value + ");");
      } else if (nodeEnd) {
        node->insts.push_back("memcpy(" + VAR_NAME(node) + ", " + interVals.back()->value + ", sizeof(" + VAR_NAME(node) + "));");
      }
    } else {
      TODO();
      // Assert(0, "invalid constant\n");
    }
    if (nodeEnd) {
      deleteAndPop();
      topValid = false;
    }
    return;
  }

  int prevIdx = node->workingVal->operands.size() - 1;
  bool indexEnd = false;
  bool rindexBegin = true;
  std::vector<std::string> index;
  for (int i = node->workingVal->ops.size() - 1; i >= 0; i--) {
    if (!node->workingVal->ops[i]) {
      if (!topValid && ! stmtValid) {
        setPrev(node, prevIdx);
      }
      topValid = false;
      stmtValid = false;
      valSize += 1;
      int delta = interVals.size() - valSize;
      Assert(delta >= 0, "interVals.size(%ld) < valSize(%d) %s %d", interVals.size(), valSize, node->name.c_str(), i);
      if (delta > 0) {
        combineVal(delta);
      }
      continue;
    }
    switch (node->workingVal->ops[i]->type) {
      case P_1EXPR1INT: insts_1expr1int(node, i, prevIdx, nodeEnd); break;
      case P_1EXPR2INT: insts_1expr2int(node, i, prevIdx, nodeEnd); break;
      case P_2EXPR: insts_2expr(node, i, prevIdx, nodeEnd); valSize -= 1; break;
      case P_1EXPR: insts_1expr(node, i, prevIdx, nodeEnd); break;
      case P_EXPR_MUX: insts_mux(node, i, prevIdx, nodeEnd, false); valSize -= 2; break;
      case P_EXPR_INT_INIT: insts_intInit(node, i); break;
      case P_PRINTF: insts_printf(node, i, prevIdx); break;
      case P_ASSERT: insts_assert(node, i, prevIdx); break;
      case P_WHEN : insts_when(node, i, prevIdx, nodeEnd); valSize -= 2; break;
      case P_INDEX : rindexBegin = insts_index(node, i, prevIdx, rindexBegin); valSize -=1; break;
      case P_CONS_INDEX : rindexBegin = insts_cons_index(node, i, prevIdx, rindexBegin); break;
      case P_L_INDEX: indexEnd = insts_l_index(node, i, prevIdx, index); valSize -=1; break;
      case P_L_CONS_INDEX: indexEnd = insts_l_cons_index(node, i, prevIdx, index); break;
      default: Assert(0, "Invalid op(%s) with type %d\n", node->workingVal->ops[i]->name.c_str(), node->workingVal->ops[i]->type);
    }

    if (indexEnd) {
      if(!topValid) setPrev(node, prevIdx);
      std::string newValue = node->name;
      for (int i = index.size() - 1; i >= 0; i --) newValue += "[" + index[i] + "]";
      Assert(interVals.size() > 0, "empty interVals in node %s\n", node->name.c_str());
      if (node->width <= 64)
        interVals.back()->value = newValue + " = " + interVals.back()->value;
      else {
        if (interVals.back()->isCons) {
          if (interVals.back()->value.length() <= 18)
            interVals.back()->value = "mpz_set_ui(" + newValue + ", " + interVals.back()->value + ")";
          else
            interVals.back()->value = "mpz_set_str(" + newValue + ", \"" + interVals.back()->value.substr(2) + "\", 16)";
        }
        else
          interVals.back()->value = "mpz_set(" + newValue + ", " + interVals.back()->value + ")";
      }
      indexEnd = false;
      index.erase(index.begin(), index.end());
      topValid = false;
      stmtValid = true;
    }

  }
  int delta = interVals.size() - valSize - 1;
  Assert(delta >= 0, "interVals.size(%ld) < valSize(%d)", interVals.size(), valSize);

  if (delta > 0) {
    combineVal(delta);
  }
  if (nodeEnd) {
    node->insts.push_back(interVals.back()->value + ";");
    deleteAndPop();
    // valSize --;
    topValid = false;
    stmtValid = false;
    Assert(interVals.size() == 0 && valSize == 0, "interVals %ld valSize %d in node %s\n", interVals.size(), valSize, node->name.c_str());
  }
}

void computeNode(Node* node, bool nodeEnd) {
  // std::cout << "compute " << node->name << "(" << node->id << ") " << nodeEnd << std::endl;
  if (node->status == CONSTANT_NODE && node->dimension.size() == 0) {
    Assert(node->workingVal->consVal.length() > 0, "consVal is not set in node %s\n", node->name.c_str());
    return;
  }
  if (node->dimension.size() != 0) {
    computeArray(node, nodeEnd);
    return;
  }
  if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 0) {
    return;
  }

  int prevIdx = node->workingVal->operands.size() - 1;
  bool rindexBegin = true;
  for (int i = node->workingVal->ops.size() - 1; i >= 0; i--) {
    if (!node->workingVal->ops[i]) {
      if (!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch (node->workingVal->ops[i]->type) {
      case P_1EXPR1INT: insts_1expr1int(node, i, prevIdx, nodeEnd); break;
      case P_1EXPR2INT: insts_1expr2int(node, i, prevIdx, nodeEnd); break;
      case P_2EXPR: insts_2expr(node, i, prevIdx, nodeEnd); break;
      case P_1EXPR: insts_1expr(node, i, prevIdx, nodeEnd); break;
      case P_EXPR_MUX: insts_mux(node, i, prevIdx, nodeEnd, false); break;
      case P_EXPR_INT_INIT: insts_intInit(node, i); break;
      case P_PRINTF: insts_printf(node, i, prevIdx); break;
      case P_ASSERT: insts_assert(node, i, prevIdx); break;
      case P_WHEN : node->workingVal->ops[i]->width = node->width;
                    insts_mux(node, i, prevIdx, nodeEnd, false); break;
      case P_INDEX : rindexBegin = insts_index(node, i, prevIdx, rindexBegin); break;
      case P_CONS_INDEX : rindexBegin = insts_cons_index(node, i, prevIdx, rindexBegin); break;
      case P_L_CONS_INDEX: continue;
      // case P_L_INDEX: insts_l_index(node, i, prevIdx); break;
      default: Assert(0, "Invalid op(%s) with type %d\n", node->workingVal->ops[i]->name.c_str(), node->workingVal->ops[i]->type);
    }
  }
  if (!topValid) {
    setPrev(node, prevIdx);
    if (!nodeEnd) return;
    if (node->width > 64 && interVals.back()->width > 64) {
      node->insts.push_back("mpz_set(" + node->name + ", " + interVals.back()->value + ");");
    } else if (node->width > 64 && interVals.back()->width <= 64) {
      node->insts.push_back("mpz_set_ui(" + node->name + ", " + interVals.back()->value + ");");
    } else if (node->width <= 64 && interVals.back()->width > 64) {
      node->insts.push_back(node->name + " = mpz_get_ui(" + interVals.back()->value + ");");
    } else {
      node->insts.push_back(VAR_NAME(node) + " = " + interVals.back()->value + ";");
    }
  }
  Assert(!interVals.back()->isCons, "Invalid constant %s in node %s\n", interVals.back()->value.c_str(), node->name.c_str());
  if (nodeEnd) {
    Assert(interVals.size() > 0, "empty interVals for %s\n", node->name.c_str());
    deleteAndPop();
    topValid = false;
    Assert(interVals.size() == 0 && valSize == 0, "interVals %ld valSize %d\n", interVals.size(), valSize);
  }
}

void instsGenerator(graph* g) {
  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (g->sorted[i]->status == CONSTANT_NODE) continue;

    switch (g->sorted[i]->type) {
      case NODE_READER:
      case NODE_WRITER: {
        for (Node* node : g->sorted[i]->member) {
          if (node->status == CONSTANT_NODE) continue;
          computeNode(node, true);
        }
        break;
      }
      default: computeNode(g->sorted[i], true);
    }

    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx    = 0;
  }

  for (Node* node : g->active) {
    computeNode(node, true);
    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx    = 0;
  }
}
