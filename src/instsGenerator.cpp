/**
 * @file instsGenerator.cpp
 * @brief instsGenerator
 */

#include <map>
#include <gmp.h>

#include "common.h"
#include "PNode.h"
#include "Node.h"
#include "graph.h"

#define add_insts_1expr(n, func, dst, expr1) n->insts.push_back(func + "(" + dst + ", " + expr1 + ")")

#define add_insts_2expr(n, func, dst, expr1, expr2) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ")")

#define add_insts_3expr(n, func, dst, expr1, expr2, expr3) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ")")

#define add_insts_4expr(n, func, dst, expr1, expr2, expr3, expr4) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ", " + expr4 + ")")

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
#define MPZ_SET(sign) std::string(sign ? "mpz_set_si" : "mpz_set_ui")
#define VAR_NAME(node) (node->name)
#define valType std::tuple<int, bool, bool, int, std::string>

#define valWidth(entry) std::get<0>(entry)
#define valSign(entry) std::get<1>(entry)
#define valCons(entry) std::get<2>(entry)
#define valOpNum(entry) std::get<3>(entry)
#define valValue(entry) std::get<4>(entry)

int p_stoi(const char* str);
std::string cons2str(std::string s);
std::pair<int, std::string> strBaseAll(std::string s);
std::string to_hex_string(unsigned long x);
void computeNode(Node* node, bool nodeEnd);
/*                            width, sign(0-UInt, 1-SInt), isCons, opNums, value*/
static std::vector<valType> valName;  // width(pos for val & neg for cons) & name
static bool topValid = false;
static int tmpIdx    = 0;
static mpz_t val;

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
  {"dshl", "<<"},
  {"dshr", ">>"},
  {"and", "&"},
  {"or", "|"},
  {"xor", "^"},
  {"shl", "<<"},
  {"shr", ">>"},
  {"asClock", "0!="},
  {"neg", "-"},
  // {"not", "~"},
  {"cvt", ""},
  {"orr", "0 != "}
};

static void setPrev(Node* node, int& prevIdx) {
  if (node->operands[prevIdx]->status == CONSTANT_NODE) {
    valName.push_back(valType(node->operands[prevIdx]->width, node->operands[prevIdx]->sign,
    true, 1, "0x" + node->operands[prevIdx]->consVal));
  } else if (node->operands[prevIdx]->id != node->operands[prevIdx]->clusId) {
    size_t size = valName.size();
    computeNode(node->operands[prevIdx], false);
    Assert(valName.size() == size + 1, "Invalid size for %s (%ld -> %ld)\n", node->name.c_str(), size, valName.size());
  } else {
    valName.push_back(valType(node->operands[prevIdx]->width, node->operands[prevIdx]->sign,
    false, 1, node->operands[prevIdx]->name));
  }
  topValid = true;
  prevIdx--;
}

void insts_1expr1int(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  int valWidth     = valWidth(valName.back());
  if (op->name == "head" || op->name == "tail") n = valWidth - n;
  std::string dstName;
  int dstOpNum = valOpNum(valName.back());
  bool dstCons = valCons(valName.back());
  bool result_mpz  = op->width > 64;
  bool operand_mpz = valWidth > 64;
  Assert(!dstCons, "constant operand %s %s\n", op->name.c_str(), valValue(valName.back()).c_str());
  if (result_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (valWidth > 64)
      add_insts_3expr(node, FUNC_NAME(op->sign, op->name), dstName, valValue(valName.back()),
                      std::to_string(valWidth(valName.back())), std::to_string(n));
    else
      add_insts_2expr(node, FUNC_UI(op->sign, op->name), dstName, valValue(valName.back()), std::to_string(n));
  } else if (!result_mpz && !operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstOpNum ++;
      dstName = "(" + valValue(valName.back()) + opMap[op->name] + std::to_string(n) + ")";
    } else if (op->name == "pad") {
      if (!op->sign)
        dstName = valValue(valName.back());
      else {
        dstOpNum ++;
        std::string operandName;
        if(valOpNum(valName.back()) != 1) {
          operandName = op->getChild(0)->name;
          node->insts.push_back(widthUType(valWidth) + " " + operandName + " = " + valValue(valName.back()));
        } else
          operandName = valValue(valName.back());
        dstName = "(-((" + nodeType(op) + ")(" + operandName + " >> " + std::to_string(valWidth - 1) +
                  ") << " + std::to_string(valWidth) + ") | " + operandName + ")";
      }
    } else if (op->name == "tail") {
      dstOpNum ++;
      unsigned long mask = n == 64 ? MAX_U64 : (((unsigned long)1 << n) - 1);
      dstName = "(" + valValue(valName.back()) + " & 0x" + to_hex_string(mask) + ")";
    } else {
      dstOpNum ++;
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valValue(valName.back()) + ", " + std::to_string(n) + ")";
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  } else if (!result_mpz && operand_mpz) {
    dstOpNum ++;
    dstName = FUNC_I(op->sign, op->name) + "(" + valValue(valName.back()) + ", " +
              std::to_string(valWidth(valName.back())) + ", " + std::to_string(n) + ")";
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  }
  valName.pop_back();
  valName.push_back(valType(op->width, op->sign, dstCons, dstOpNum, dstName));
}

void insts_1expr2int(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = valCons(valName.back());
  Assert(!dstCons, "constant operand %s %s\n", op->name.c_str(), valValue(valName.back()).c_str());
  std::string dstName;
  int dstOpNum = valOpNum(valName.back());
  if (op->width <= 64) {
    if (valWidth(valName.back()) <= 64) {
      int l = p_stoi(op->getExtra(0).c_str()), r = p_stoi(op->getExtra(1).c_str()), w = l - r + 1;
      unsigned long mask = w == 64 ? MAX_U64 : ((unsigned long)1 << w) - 1;
      dstName = "((" + valValue(valName.back()) + " >> " + std::to_string(r) + ") & 0x" + to_hex_string(mask) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valValue(valName.back()) + ", " +
                std::to_string(valWidth(valName.back())) + ", " + cons2str(op->getExtra(0)) + ", " +
                cons2str(op->getExtra(1)) + ")";
    }
    dstOpNum ++;
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  } else {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    add_insts_4expr(node, FUNC_NAME(op->sign, op->name), dstName, valValue(valName.back()),
                    std::to_string(valWidth(valName.back())), cons2str(op->getExtra(0)), cons2str(op->getExtra(1)));
  }
  valName.pop_back();
  valName.push_back(valType(op->width, op->sign, dstCons, dstOpNum, dstName));
}

void insts_2expr(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  int funcSign = op->sign;
  if (op->name == "add" || op->name == "mul" || op->name == "sub" || op->name == "div" || op->name == "rem")
    funcSign = 0;

  int dstOpNum = valOpNum(valName.back()) + valOpNum(valName[valName.size()-2]);
  int dstCons = valCons(valName.back()) && valCons(valName[valName.size()-2]);
  Assert(!dstCons, "constant operand %s %s %s\n", op->name.c_str(), valValue(valName.back()).c_str(),
                                                  valValue(valName[valName.size()-2]).c_str());
  std::string dstName;
  bool result_mpz  = op->width > 64;
  bool left_mpz    = valWidth(valName.back()) > 64;
  bool right_mpz   = valWidth(valName[valName.size() - 2]) > 64;
  bool operand_mpz = left_mpz && right_mpz;
  if (result_mpz && operand_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (!valCons(valName.back()) && !valCons(valName[valName.size() - 2])) {
      add_insts_4expr(node, FUNC_NAME(funcSign, op->name), dstName, valValue(valName.back()),
                      std::to_string(valWidth(valName.back())), valValue(valName[valName.size() - 2]),
                      std::to_string(valWidth(valName[valName.size() - 2])));
    } else if (!valCons(valName.back()) && valCons(valName[valName.size() - 2])) {
      add_insts_3expr(node, FUNC_UI_R(funcSign, op->name), dstName, valValue(valName.back()),
                      "0x" + valValue(valName[valName.size() - 2]), std::to_string(valWidth(valName[valName.size() - 2])));
    } else if (valCons(valName.back()) && !valCons(valName[valName.size() - 2])) {
      add_insts_4expr(node, FUNC_SUI_L(funcSign, valSign(valName.back()), op->name), dstName,
                      "0x" + valValue(valName.back()), std::to_string(valWidth(valName.back())),
                      valValue(valName[valName.size() - 2]), std::to_string(valWidth(valName[valName.size() - 2])));
    } else {
      add_insts_4expr(node, FUNC_UI2(funcSign, op->name), dstName, valValue(valName.back()),
                      std::to_string(valWidth(valName.back())), valValue(valName[valName.size() - 2]),
                      std::to_string(valWidth(valName[valName.size() - 2])));
    }
  } else if (!result_mpz && !operand_mpz) {
    dstOpNum ++;
    if (opMap.find(op->name) != opMap.end()) {
      if (valSign(valName.back())) {
        std::string typeStr = "(" + widthSType(valWidth(valName.back())) + ")";
        dstName = "(" + typeStr + valValue(valName.back()) + opMap[op->name] + typeStr +
                  valValue(valName[valName.size() - 2]) + ")";
      } else {
        dstName = "(" + valValue(valName.back()) + opMap[op->name] + valValue(valName[valName.size() - 2]) + ")";
      }
    } else if (op->name == "cat") {
      dstName = "((uint64_t)" + valValue(valName.back()) + " << " +
                std::to_string(valWidth(valName[valName.size() - 2])) + " | " +
                valValue(valName[valName.size() - 2]) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valValue(valName.back()) + ", " +
                valValue(valName[valName.size() - 2]) + ")";
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  } else if (result_mpz && !operand_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    if (left_mpz) {
      add_insts_3expr(node, FUNC_SUI_R(funcSign, valSign(valName[valName.size() - 2]), op->name),
                    dstName, valValue(valName.back()), valValue(valName[valName.size() - 2]),
                    std::to_string(valWidth(valName[valName.size() - 2])));
    } else if (right_mpz) {
      add_insts_4expr(node, FUNC_SUI_L(funcSign, valSign(valName.back()), op->name), dstName,
                    valValue(valName.back()), std::to_string(valWidth(valName.back())),
                    valValue(valName[valName.size() - 2]), std::to_string(valWidth(valName[valName.size() - 2])));
    } else {
      node->insts.push_back(MPZ_SET(valSign(valName.back())) + "(" + dstName + ", " + valValue(valName.back()) + ")");
      add_insts_3expr(node, FUNC_SUI_R(funcSign, valSign(valName[valName.size()-2]), op->name), dstName, dstName,
                      valValue(valName[valName.size() - 2]), std::to_string(valWidth(valName[valName.size() - 2])));
    }
  } else if (!result_mpz && operand_mpz) {
    dstOpNum ++;
    dstName = FUNC_I(op->sign, op->name) + "(" + valValue(valName.back()) + ", " + std::to_string(valWidth(valName.back())) +
              ", " + valValue(valName[valName.size() - 2]) + ", " + std::to_string(valWidth(valName[valName.size() - 2])) + ")";
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  }
  valName.pop_back();
  valName.pop_back();
  valName.push_back(valType(op->width, op->sign, dstCons, dstOpNum, dstName));
}

void insts_1expr(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = valCons(valName.back());
  Assert(!dstCons, "constant operand %s %s\n", op->name.c_str(), valValue(valName.back()).c_str());
  int dstOpNum = valOpNum(valName.back());
  std::string dstName;
  bool result_mpz  = op->width > 64;
  bool operand_mpz = valWidth(valName.back()) > 64;
  if (result_mpz) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    add_insts_2expr(node, (operand_mpz ? FUNC_NAME(op->sign, op->name) : FUNC_UI(op->sign, op->name)),
                    dstName, valValue(valName.back()), std::to_string(valWidth(valName.back())));
  } else if (!operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstName = "(" + opMap[op->name] + valValue(valName.back()) + ")";
      dstOpNum ++;
    } else if (op->name == "not") {
      unsigned long mask = op->width == 64 ? MAX_U64 : (((unsigned long)1 << op->width) - 1);
      dstName = "(" + valValue(valName.back()) + " ^ 0x" + to_hex_string(mask) + ")";
      dstOpNum ++;
    } else if (op->name == "asSInt") {
      dstName = "(" + widthSType(op->width) + ")" + valValue(valName.back());
    } else if (op->name == "asUInt") {
      dstName = "(" + widthUType(op->width) + ")" + valValue(valName.back());
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valValue(valName.back()) + ")";
      dstOpNum ++;
    }
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + dstName);
  } else {
    std::cout << op->name << " " << op->getChild(0)->name << std::endl;
    TODO();
  }
  valName.pop_back();
  valName.push_back(valType(op->width, op->sign, dstCons, dstOpNum, dstName));
}

void insts_mux(Node* node, int opIdx, int& prevIdx, bool nodeEnd) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  bool dstCons = valCons(valName.back()) && valCons(valName[valName.size()-2]) && valCons(valName[valName.size()-3]);
  int muxType = valCons(valName.back()) ? (valValue(valName.back()) == "0x0" ? 1 : 2) : 0;
  Assert(!valCons(valName.back()) || valValue(valName.back()) == "0x0" || valValue(valName.back()) == "0x1", "invalid constant cond %s\n", valValue(valName.back()).c_str());
  std::string cond = valCons(valName.back()) ? valValue(valName.back()) :
                      valEqualsZero(valWidth(valName.back()), valValue(valName.back()));
  valName.pop_back();
  std::string dstName;
  int dstOpNum = valOpNum(valName.back()) + valOpNum(valName[valName.size()-2]);
  if (op->width > 64) {
    dstName = (opIdx == 0 && nodeEnd) ? node->name : NEW_TMP;
    dstOpNum = 1;
    std::string cond_true, cond_false;
    if (!valCons(valName.back())) {
      cond_true = "mpz_set(" + dstName + ", " + valValue(valName.back()) + ")";
    } else {
      if (valValue(valName.back()).length() <= 18)
        cond_true = "mpz_set_ui(" + dstName + ", " + valValue(valName.back()) + ")";
      else
        cond_true = "(void)mpz_set_str(" + dstName + ", \"" + valValue(valName.back()).substr(2) + "\", 16)";
    }
    valName.pop_back();

    if (!valCons(valName.back())) {
      cond_false = "mpz_set(" + dstName + ", " + valValue(valName.back()) + ")";
    } else {
      if (valValue(valName.back()).length() <= 1)
        cond_false = "mpz_set_ui(" + dstName + ", " + valValue(valName.back()) + ")";
      else
        cond_false = "(void)mpz_set_str(" + dstName + ", \"" + valValue(valName.back()).substr(2) + "\", 16)";
    }
    if(muxType == 0) node->insts.push_back(cond + "? " + cond_true + " : " + cond_false);
    else if(muxType == 1) node->insts.push_back(cond_false);
    else if(muxType == 2) node->insts.push_back(cond_true);
  } else {
    std::string cond_true = valValue(valName.back());
    valName.pop_back();
    std::string cond_false = valValue(valName.back());
    std::string value;
    if(muxType == 0) value = "(" + cond + "? " + cond_true + " : \n" + cond_false + ")";
    else if(muxType == 1) value = cond_false;
    else if(muxType == 2) value = cond_true;
    if (opIdx == 0 && nodeEnd) node->insts.push_back(VAR_NAME(node) + " = " + value);
    dstName = value;
    dstOpNum += muxType == 0;
  }
  valName.pop_back();
  valName.push_back(valType(op->width, op->sign, dstCons, dstOpNum, dstName));
}

void insts_intInit(Node* node, int opIdx) {
  int base;
  std::string cons;
  std::tie(base, cons) = strBaseAll(node->ops[opIdx]->getExtra(0));
  mpz_set_str(val, cons.c_str(), base);
  char* str = mpz_get_str(NULL, 16, val);
  valName.push_back(valType(node->ops[opIdx]->width, node->ops[opIdx]->sign, true, 1, std::string("0x") + str));
  topValid = true;
  free(str);
}

void insts_printf(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string cond = valCons(valName.back()) ? valValue(valName.back())
                                    : valEqualsZero(valWidth(valName.back()), valValue(valName.back()));
  std::string inst = "if(" + cond + ") printf(" + node->ops[opIdx]->getExtra(0);
  valName.pop_back();
  for (int i = 0; i < node->ops[opIdx]->getChild(2)->getChildNum(); i++) {
    inst += ", " + (valCons(valName.back()) ? valValue(valName.back()) :
          valEqualsZero(valWidth(valName.back()), valValue(valName.back())));
    valName.pop_back();
  }
  node->insts.push_back(inst + ")");
  valName.push_back(valType(0, 0, 0, 0, ""));
}

void insts_assert(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string pred = valCons(valName.back())? valValue(valName.back())
                  : valEqualsZero(valWidth(valName.back()), valValue(valName.back()));
  valName.pop_back();

  std::string en = valCons(valName.back())? valValue(valName.back())
                  : valEqualsZero(valWidth(valName.back()), valValue(valName.back()));
  valName.pop_back();

  std::string inst = "Assert(!" + en + " || " + pred + ", " + node->ops[opIdx]->getExtra(0) + ")";
  node->insts.push_back(inst);
  valName.push_back(valType(0, 0, 0, 0, ""));
}

void computeNode(Node* node, bool nodeEnd) {
  if (node->ops.size() == 0 && node->operands.size() == 0) return;
  if (node->ops.size() == 0) {
    Assert(node->operands.size() == 1, "Invalid operands size(%ld) for %s\n", node->operands.size(),
           node->name.c_str());
    int prevIdx = 0;
    setPrev(node, prevIdx);
    if (!valCons(valName.back())) {
      if (nodeEnd && node->width > 64) {
        node->insts.push_back("mpz_set(" + node->name + ", " + valValue(valName.back()) + ")");
      } else if (nodeEnd) {
        node->insts.push_back(VAR_NAME(node) + " = " + valValue(valName.back()));
      }
    } else {
      Assert(0, "invalid constant\n");
    }
    if (nodeEnd) {
      valName.pop_back();
      topValid = false;
    }
    return;
  }
  int prevIdx = node->operands.size() - 1;
  for (int i = node->ops.size() - 1; i >= 0; i--) {
    if (!node->ops[i]) {
      if (!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch (node->ops[i]->type) {
      case P_1EXPR1INT: insts_1expr1int(node, i, prevIdx, nodeEnd); break;
      case P_1EXPR2INT: insts_1expr2int(node, i, prevIdx, nodeEnd); break;
      case P_2EXPR: insts_2expr(node, i, prevIdx, nodeEnd); break;
      case P_1EXPR: insts_1expr(node, i, prevIdx, nodeEnd); break;
      case P_EXPR_MUX: insts_mux(node, i, prevIdx, nodeEnd); break;
      case P_EXPR_INT_INIT: insts_intInit(node, i); break;
      case P_PRINTF: insts_printf(node, i, prevIdx); break;
      case P_ASSERT: insts_assert(node, i, prevIdx); break;
      default: Assert(0, "Invalid op(%s) with type %d\n", node->ops[i]->name.c_str(), node->ops[i]->type);
    }
  }
  if (nodeEnd) {
    valName.pop_back();
    topValid = false;
  }
}

void instsGenerator(graph* g) {
  mpz_init(val);

  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (g->sorted[i]->status == CONSTANT_NODE) continue;

    switch (g->sorted[i]->type) {
      case NODE_READER:
      case NODE_WRITER: {
        for (Node* node : g->sorted[i]->member) computeNode(node, true);
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
