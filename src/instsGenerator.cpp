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

#define EXPR_CONSTANT 0
#define EXPR_VAR 1

#define add_insts_1expr(n, func, dst, expr1) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ")")

#define add_insts_2expr(n, func, dst, expr1, expr2) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ")")

#define add_insts_3expr(n, func, dst, expr1, expr2, expr3) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ")")

#define add_insts_4expr(n, func, dst, expr1, expr2, expr3, expr4)                           \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ", " + \
                     expr4 + ")")

#define NEW_TMP ("__tmp__" + std::to_string(++tmpIdx))
#define FUNC_NAME(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s)
#define FUNC_BASIC(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s + "_basic")
#define FUNC_I(sign, s) ((sign ? std::string("si_") : std::string("ui_")) + s)
#define FUNC_I_UI(sign, s) ((sign ? std::string("si_") : std::string("ui_")) + s + "_ui")
#define FUNC_SUI_R(fsign, vsign, s) \
  ((fsign ? std::string("s_") : std::string("u_")) + s + (vsign ? "_si_r" : "_ui_r"))
#define FUNC_SUI_L(fsign, vsign, s) \
  ((fsign ? std::string("s_") : std::string("u_")) + s + (vsign ? "_si_l" : "_ui_l"))
#define MAX_U64 0xffffffffffffffff
#define valEqualsZero(width, str) (width > 64 ? "mpz_sgn(" + str + ")" : str)
#define valName2Str(entry) (entry.first > 0 ? entry.second : "0x" + entry.second)
#define MPZ_SET(sign) std::string(sign ? "mpz_set_si" : "mpz_set_ui")

int p_stoi(const char* str);
std::string cons2str(std::string s);
std::pair<int, std::string> strBaseAll(std::string s);
std::string to_hex_string(unsigned long x);

static std::vector<std::pair<int, std::string>>
    valName;  // width(pos for val & neg for cons) & name
static bool topValid = false;
static int tmpIdx = 0;
static mpz_t val;

static std::map<std::string, std::string> opMap = {
    {"add", "+"},       {"sub", "-"},   {"mul", "*"},   {"div", "/"},   {"rem", "%"},
    {"lt", "<"},        {"leq", "<="},  {"gt", ">"},    {"geq", ">="},  {"eq", "=="},
    {"neq", "!="},      {"dshl", "<<"}, {"dshr", ">>"}, {"and", "&"},   {"or", "|"},
    {"xor", "^"},       {"shl", "<<"},  {"shr", ">>"},  {"asUInt", ""}, {"asSInt", ""},
    {"asClock", "0!="}, {"neg", "-"},   {"not", "~"},   {"cvt", ""},    {"orr", "0 != "}};

static void setPrev(Node* node, int& prevIdx) {
  if (node->operands[prevIdx]->status == CONSTANT_NODE) {
    valName.push_back(std::pair(-node->operands[prevIdx]->width, node->operands[prevIdx]->consVal));
  } else {
    valName.push_back(std::pair(node->operands[prevIdx]->width, node->operands[prevIdx]->name));
  }
  topValid = true;
  prevIdx--;
}

void insts_1expr1int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  if (op->name == "head" || op->name == "tail") n = op->getChild(0)->width - n;
  std::string dstName;
  bool result_mpz = op->width > 64;
  bool operand_mpz = ABS(valName.back().first) > 64;
  if (result_mpz) {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    if (valName.back().first > 64)
      add_insts_3expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second,
                      std::to_string(op->getChild(0)->width), std::to_string(n));
    else
      add_insts_2expr(node, FUNC_NAME(op->sign, op->name) + "_ui", dstName,
                      (valName.back().first ? "" : "0x") + valName.back().second,
                      std::to_string(n));
  } else if (!result_mpz && !operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstName = "(" + valName2Str(valName.back()) + opMap[op->name] + std::to_string(n) + ")";
    } else if (op->name == "pad") {
      dstName = valName2Str(valName.back());
    } else if (op->name == "tail") {
      unsigned long mask = n == 64 ? MAX_U64 : (((unsigned long)1 << n) - 1);
      dstName = "(" + valName2Str(valName.back()) + " & 0x" + to_hex_string(mask) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valName2Str(valName.back()) + ", " +
                std::to_string(n) + ")";
    }
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  } else if (!result_mpz && operand_mpz) {
    dstName = FUNC_I(op->sign, op->name) + "(" + valName2Str(valName.back()) + ", " +
              std::to_string(ABS(valName.back().first)) + ", " + std::to_string(n) + ")";
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  }
  valName.pop_back();
  valName.push_back(std::pair(op->width, dstName));
}

void insts_1expr2int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  // unsigned long n = p_stoi(op->getExtra(0).c_str());
  std::string dstName;
  if (op->width <= 64) {
    if (ABS(valName.back().first) <= 64) {
      int l = p_stoi(op->getExtra(0).c_str()), r = p_stoi(op->getExtra(1).c_str()), w = l - r + 1;
      unsigned long mask = w == 64 ? MAX_U64 : ((unsigned long)1 << w) - 1;
      dstName = "((" + valName2Str(valName.back()) + " >> " + std::to_string(r) + ") & 0x" +
                to_hex_string(mask) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valName2Str(valName.back()) + ", " +
                std::to_string(ABS(valName.back().first)) + ", " + cons2str(op->getExtra(0)) +
                ", " + cons2str(op->getExtra(1)) + ")";
    }
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  } else {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    add_insts_4expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second,
                    std::to_string(op->getChild(0)->width), cons2str(op->getExtra(0)),
                    cons2str(op->getExtra(1)));
  }
  valName.pop_back();
  valName.push_back(std::pair(op->width, dstName));
}

void insts_2expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);
  int funcSign = op->sign;
  if (op->name == "add" || op->name == "mul" || op->name == "sub" || op->name == "div" ||
      op->name == "rem")
    funcSign = 0;

  std::string dstName;
  bool result_mpz = op->width > 64;
  bool left_mpz = ABS(valName.back().first) > 64;
  bool right_mpz = (ABS(valName[valName.size() - 2].first) > 64);
  bool operand_mpz = left_mpz && right_mpz;
  if (result_mpz && operand_mpz) {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    // TODO: replace getChild with back.first
    if (valName.back().first > 0 && valName[valName.size() - 2].first > 0) {
      add_insts_4expr(node, FUNC_NAME(funcSign, op->name), dstName, valName.back().second,
                      std::to_string(op->getChild(0)->width), valName[valName.size() - 2].second,
                      std::to_string(op->getChild(1)->width));
    } else if (valName.back().first > 0 && valName[valName.size() - 2].first < 0) {
      add_insts_3expr(node, FUNC_NAME(funcSign, op->name) + "_ui_r", dstName, valName.back().second,
                      "0x" + valName[valName.size() - 2].second,
                      std::to_string(op->getChild(1)->width));
    } else if (valName.back().first < 0 && valName[valName.size() - 2].first > 0) {
      add_insts_4expr(node,
                      FUNC_NAME(funcSign, op->name) + (op->getChild(0)->sign ? "_si_l" : "_ui_l"),
                      dstName, "0x" + valName.back().second, std::to_string(op->getChild(0)->width),
                      valName[valName.size() - 2].second, std::to_string(op->getChild(1)->width));
    } else {
      add_insts_4expr(node, FUNC_NAME(funcSign, op->name) + "_ui2", dstName,
                      "0x" + valName.back().second, std::to_string(op->getChild(0)->width),
                      "0x" + valName[valName.size() - 2].second,
                      std::to_string(op->getChild(1)->width));
    }
  } else if (!result_mpz && !operand_mpz) {
    if (opMap.find(op->name) != opMap.end()) {
      dstName = "(" + valName2Str(valName.back()) + opMap[op->name] +
                valName2Str(valName[valName.size() - 2]) + ")";
    } else if (op->name == "cat") {
      dstName = "((uint64_t)" + valName2Str(valName.back()) + " << " +
                std::to_string(ABS(valName[valName.size() - 2].first)) + " | " +
                valName2Str(valName[valName.size() - 2]) + ")";
    } else {
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valName2Str(valName.back()) + ", " +
                valName2Str(valName[valName.size() - 2]) + ")";
    }
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  } else if (result_mpz && !operand_mpz) {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    if (left_mpz) {
      add_insts_3expr(node, FUNC_SUI_R(funcSign, op->getChild(1)->sign, op->name), dstName,
                      valName2Str(valName.back()), valName2Str(valName[valName.size() - 2]),
                      std::to_string(ABS(valName[valName.size() - 2].first)));
    } else if (right_mpz) {
      add_insts_4expr(node, FUNC_SUI_L(funcSign, op->getChild(0)->sign, op->name), dstName,
                      valName2Str(valName.back()), std::to_string(ABS(valName.back().first)),
                      valName2Str(valName[valName.size() - 2]),
                      std::to_string(ABS(valName[valName.size() - 2].first)));
    } else {
      node->insts.push_back(MPZ_SET(op->getChild(0)->sign) + "(" + dstName + ", " +
                            valName2Str(valName.back()) + ")");
      add_insts_3expr(node, FUNC_SUI_R(funcSign, op->getChild(1)->sign, op->name), dstName, dstName,
                      valName2Str(valName[valName.size() - 2]),
                      std::to_string(ABS(valName[valName.size() - 2].first)));
    }
  } else if (!result_mpz && operand_mpz) {
    dstName = FUNC_I(op->sign, op->name) + "(" + valName.back().second + ", " +
              std::to_string(op->getChild(0)->width) + ", " + valName[valName.size() - 2].second +
              ", " + std::to_string(op->getChild(1)->width) + ")";
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  }
  valName.pop_back();
  valName.pop_back();
  valName.push_back(std::pair(op->width, dstName));
}

void insts_1expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if (!topValid) setPrev(node, prevIdx);

  std::string dstName;
  bool result_mpz = op->width > 64;
  bool operand_mpz = ABS(valName.back().first) > 64;
  if (result_mpz) {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    if (valName.back().first > 64)
      add_insts_2expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second,
                      std::to_string(op->getChild(0)->width));
    else
      add_insts_2expr(node, FUNC_NAME(op->sign, op->name) + "_ui", dstName,
                      valName2Str(valName.back()), std::to_string(op->getChild(0)->width));
  } else if (!result_mpz && !operand_mpz) {
    if (opMap.find(op->name) != opMap.end())
      dstName = "(" + opMap[op->name] + valName2Str(valName.back()) + ")";
    else
      dstName = FUNC_BASIC(op->sign, op->name) + "(" + valName2Str(valName.back()) + ")";
    if (opIdx == 0) node->insts.push_back(node->name + " = " + dstName);
  } else {
    std::cout << op->name << " " << op->getChild(0)->name << std::endl;
    TODO();
  }
  valName.pop_back();
  valName.push_back(std::pair(op->width, dstName));
}

void insts_mux(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string cond = valName.back().first > 0
                         ? valEqualsZero(valName.back().first, valName.back().second)
                         : ("0x" + valName.back().second);
  valName.pop_back();
  std::string dstName;
  if (node[opIdx].width > 64) {
    dstName = opIdx == 0 ? node->name : NEW_TMP;
    std::string cond_true, cond_false;
    if (valName.back().first > 0) {
      cond_true = "mpz_set(" + dstName + ", " + valName2Str(valName.back()) + ")";
    } else {
      if (valName.back().second.length() <= 16)
        cond_true = "mpz_set_ui(" + dstName + ", " + valName2Str(valName.back()) + ")";
      else
        cond_true = "(void)mpz_set_str(" + dstName + ", \"" + valName.back().second + "\", 16)";
    }
    valName.pop_back();
    if (valName.back().first > 0) {
      cond_false = "mpz_set(" + dstName + ", " + valName.back().second + ")";
    } else {
      if (valName.back().second.length() <= 16)
        cond_false = "mpz_set_ui(" + dstName + ", " + valName2Str(valName.back()) + ")";
      else
        cond_false = "(void)mpz_set_str(" + dstName + ", \"" + valName.back().second + "\", 16)";
    }
    node->insts.push_back(cond + "? " + cond_true + " : " + cond_false);
  } else {
    std::string value = "(" + cond + "? " + valName2Str(valName.back()) + " : ";
    valName.pop_back();
    value += valName2Str(valName.back()) + ")";
    if (opIdx == 0) node->insts.push_back(node->name + " = " + value);
    dstName = value;
  }
  valName.pop_back();
  valName.push_back(std::pair(node->ops[opIdx]->width, dstName));
}

void insts_intInit(Node* node, int opIdx) {
  int base;
  std::string cons;
  std::tie(base, cons) = strBaseAll(node->ops[opIdx]->getExtra(0));
  mpz_set_str(val, cons.c_str(), base);
  char* str = mpz_get_str(NULL, 16, val);
  valName.push_back(std::pair(-node->ops[opIdx]->width, str));
  topValid = true;
  free(str);
}

void insts_printf(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string cond = valName.back().first > 0
                         ? valEqualsZero(valName.back().first, valName.back().second)
                         : "0x" + valName.back().second;
  std::string inst = "if(" + cond + ") printf(" + node->ops[opIdx]->getExtra(0);
  valName.pop_back();
  for (int i = 0; i < node->ops[opIdx]->getChild(2)->getChildNum(); i++) {
    inst += ", " + (valName.back().first > 0
                        ? valEqualsZero(valName.back().first, valName.back().second)
                        : "0x" + valName.back().second);
    valName.pop_back();
  }
  node->insts.push_back(inst + ")");
  valName.push_back(std::pair(0, ""));
}

void insts_assert(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  std::string pred = valName.back().first > 0
                         ? valEqualsZero(valName.back().first, valName.back().second)
                         : "0x" + valName.back().second;
  // std::string pred = valName.back().first ? (std::string("mpz_sgn(") +
  // valName.back().second + ")") : "0x" + valName.back().second;
  valName.pop_back();
  std::string en = valName.back().first > 0
                       ? valEqualsZero(valName.back().first, valName.back().second)
                       : "0x" + valName.back().second;
  // (std::string("mpz_sgn(") + valName.back().second + ")") : "0x" +
  // valName.back().second;
  valName.pop_back();
  std::string inst = "Assert(!" + en + " || " + pred + ", " + node->ops[opIdx]->getExtra(0) + ")";
  node->insts.push_back(inst);
  valName.push_back(std::pair(0, ""));
}

void computeNode(Node* node) {
  if (node->ops.size() == 0 && node->operands.size() == 0) return;
  if (node->ops.size() == 0) {
    Assert(node->operands.size() == 1, "Invalid operands size(%ld) for %s\n", node->operands.size(),
           node->name.c_str());
    if (node->operands[0]->status != CONSTANT_NODE) {
      if (node->width > 64) {
        node->insts.push_back("mpz_set(" + node->name + ", " + node->operands[0]->name + ")");
      } else {
        node->insts.push_back(node->name + " = " + node->operands[0]->name);
      }
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
      case P_1EXPR1INT:
        insts_1expr1int(node, i, prevIdx);
        break;
      case P_1EXPR2INT:
        insts_1expr2int(node, i, prevIdx);
        break;
      case P_2EXPR:
        insts_2expr(node, i, prevIdx);
        break;
      case P_1EXPR:
        insts_1expr(node, i, prevIdx);
        break;
      case P_EXPR_MUX:
        insts_mux(node, i, prevIdx);
        break;
      case P_EXPR_INT_INIT:
        insts_intInit(node, i);
        break;
      case P_PRINTF:
        insts_printf(node, i, prevIdx);
        break;
      case P_ASSERT:
        insts_assert(node, i, prevIdx);
        break;
      default:
        Assert(0, "Invalid op(%s) with type %d\n", node->ops[i]->name.c_str(), node->ops[i]->type);
    }
  }
  Assert(valName.size() == 1, "Invalid valname size %ld for node %s\n", valName.size(),
         node->name.c_str());
  valName.pop_back();
  topValid = false;
}

void instsGenerator(graph* g) {
  mpz_init(val);

  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (g->sorted[i]->status == CONSTANT_NODE) {
      continue;
    }

    switch (g->sorted[i]->type) {
      case NODE_READER:
      case NODE_WRITER: {
        for (Node* node : g->sorted[i]->member) {
          computeNode(node);
        }
        break;
      }

      default:
        computeNode(g->sorted[i]);
    }

    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx = 0;
  }

  for (Node* node : g->active) {
    computeNode(node);
    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx = 0;
  }
}
