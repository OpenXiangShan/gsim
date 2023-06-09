#include "common.h"
#include "PNode.h"
#include "Node.h"
#include "graph.h"
#include <map>
#include <gmp.h>

#define EXPR_CONSTANT 0
#define EXPR_VAR 1

#define add_insts_1expr(n, func, dst, expr1) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ")")
#define add_insts_2expr(n, func, dst, expr1, expr2) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ")")
#define add_insts_3expr(n, func, dst, expr1, expr2, expr3) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ")")
#define add_insts_4expr(n, func, dst, expr1, expr2, expr3, expr4) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ", " + expr4 + ")")
#define NEW_TMP ("__tmp__" + std::to_string(++tmpIdx))
#define FUNC_NAME(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s)

int p_stoi(const char* str);
std::string cons2str(std::string s);
std::pair<int, std::string> strBaseAll(std::string s);

static std::vector<std::pair<int, std::string>>valName;
static bool topValid = false;
static int tmpIdx = 0;
static mpz_t val;

static inline void allocVal() {
  valName.push_back(std::pair(EXPR_VAR, std::to_string(++tmpIdx)));
}

static void setPrev(Node* node, int& prevIdx) {
  if(node->operands[prevIdx]->status == CONSTANT_NODE) {
    valName.push_back(std::pair(EXPR_CONSTANT, node->operands[prevIdx]->consVal));
  } else {
    valName.push_back(std::pair(EXPR_VAR, node->operands[prevIdx]->name));
  }
  topValid = true;
  prevIdx --;
}

void insts_1expr1int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if(!topValid) setPrev(node, prevIdx);
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  if(op->name == "head" || op->name == "tail") n = op->getChild(0)->width - n;
  std::string dstName = opIdx == 0 ? node->name : NEW_TMP;
  if(valName.back().first)
    add_insts_3expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second, std::to_string(op->getChild(0)->width), std::to_string(n));
  else
    add_insts_2expr(node, FUNC_NAME(op->sign, op->name) + "_ui", dstName, "0x" + valName.back().second, std::to_string(n));
  valName.pop_back();
  valName.push_back(std::pair(EXPR_VAR, dstName));
}

void insts_1expr2int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if(!topValid) setPrev(node, prevIdx);
  unsigned long n = p_stoi(op->getExtra(0).c_str());

  std::string dstName = opIdx == 0 ? node->name : NEW_TMP;
  add_insts_4expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second, std::to_string(op->getChild(0)->width), cons2str(op->getExtra(0)), cons2str(op->getExtra(1)));
  valName.pop_back();
  valName.push_back(std::pair(EXPR_VAR, dstName));
}

void insts_2expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if(!topValid) setPrev(node, prevIdx);
  int funcSign = op->sign;
  if(op->name == "add" || op->name == "mul" || op->name == "sub" || op->name == "div" || op->name == "rem") funcSign = 0;
  std::string dstName = opIdx == 0 ? node->name : NEW_TMP;
  if(valName.back().first && valName[valName.size() - 2].first) {
    add_insts_4expr(node, FUNC_NAME(funcSign, op->name), dstName, valName.back().second, std::to_string(op->getChild(0)->width), valName[valName.size() - 2].second, std::to_string(op->getChild(1)->width));
  } else if(valName.back().first && !valName[valName.size() - 2].first) {
    add_insts_3expr(node, FUNC_NAME(funcSign, op->name) + "_ui_r", dstName, valName.back().second, "0x" + valName[valName.size() - 2].second, std::to_string(op->getChild(1)->width));
  } else if(!valName.back().first && valName[valName.size() - 2].first) {
    add_insts_4expr(node, FUNC_NAME(funcSign, op->name) + "_ui_l", dstName, "0x" + valName.back().second, std::to_string(op->getChild(0)->width), valName[valName.size() - 2].second, std::to_string(op->getChild(1)->width));
  } else {
    add_insts_4expr(node, FUNC_NAME(funcSign, op->name) + "_ui2", dstName, "0x" + valName.back().second, std::to_string(op->getChild(0)->width), "0x" + valName[valName.size() - 2].second, std::to_string(op->getChild(1)->width));
  }
  valName.pop_back(); valName.pop_back();
  valName.push_back(std::pair(EXPR_VAR, dstName));
}

void insts_1expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if(!topValid) setPrev(node, prevIdx);

  std::string dstName = opIdx == 0 ? node->name : NEW_TMP;
  if(valName.back().first)
    add_insts_2expr(node, FUNC_NAME(op->sign, op->name), dstName, valName.back().second, std::to_string(op->getChild(0)->width));
  else
    add_insts_2expr(node, FUNC_NAME(op->sign, op->name) + "_ui", dstName, "0x" + valName.back().second, std::to_string(op->getChild(0)->width));
  valName.pop_back();
  valName.push_back(std::pair(EXPR_VAR, dstName));
}

void insts_mux(Node* node, int opIdx, int& prevIdx) {
  if(!topValid) setPrev(node, prevIdx);
  std::string cond = valName.back().first ? (std::string("mpz_sgn(") + valName.back().second + ")") : "0x" + valName.back().second;
  valName.pop_back();
  std::string dstName = opIdx == 0 ? node->name : NEW_TMP;
  std::string cond_true, cond_false;
  // int base; std::string cons;
  if(valName.back().first) {
    cond_true = "mpz_set(" + dstName + ", " + valName.back().second + ")";
  } else {
    if(valName.back().second.length() <= 16) cond_true = "mpz_set_ui(" + dstName + ", 0x" + valName.back().second + ")";
    else cond_true = "(void)mpz_set_str(" + dstName + ", \"" + valName.back().second + "\", 16)";
  }
  valName.pop_back();
  if(valName.back().first) {
    cond_false = "mpz_set(" + dstName + ", " + valName.back().second + ")";
  } else {
    if(valName.back().second.length() <= 16) cond_false = "mpz_set_ui(" + dstName + ", 0x" + valName.back().second + ")";
    else cond_false = "(void)mpz_set_str(" + dstName + ", \"" + valName.back().second + "\", 16)";
  }
  node->insts.push_back(cond + "? " + cond_true + " : " + cond_false);
  valName.pop_back();
  valName.push_back(std::pair(EXPR_VAR, dstName));
}

void insts_intInit(Node* node, int opIdx) {
  int base;
  std::string cons;
  std::tie(base, cons) = strBaseAll(node->ops[opIdx]->getExtra(0));
  mpz_set_str(val, cons.c_str(), base);
  char* str = mpz_get_str(NULL, 16, val);
  valName.push_back(std::pair(EXPR_CONSTANT, str));
  topValid = true;
  free(str);
}

void insts_printf(Node* node, int opIdx, int& prevIdx) {
  if(!topValid) setPrev(node, prevIdx);
  std::string cond = valName.back().first ? (std::string("mpz_sgn(") + valName.back().second + ")") : "0x" + valName.back().second;
  std::string inst = "if(" + cond + ") printf(" + node->ops[opIdx]->getExtra(0);
  valName.pop_back();
  for(int i = 0; i < node->ops[opIdx]->getChild(2)->getChildNum(); i++) {
    inst += ", " + (valName.back().first ? std::string("mpz_get_ui(") + valName.back().second + ")" : "0x" + valName.back().second);
    valName.pop_back();
  }
  node->insts.push_back(inst + ")");
  valName.push_back(std::pair(-1, ""));
}

void insts_assert(Node* node, int opIdx, int& prevIdx) {
  if(!topValid) setPrev(node, prevIdx);
  std::string pred = valName.back().first ? (std::string("mpz_sgn(") + valName.back().second + ")") : "0x" + valName.back().second;
  valName.pop_back();
  std::string en = valName.back().first ? (std::string("mpz_sgn(") + valName.back().second + ")") : "0x" + valName.back().second;
  valName.pop_back();
  std::string inst = "Assert(!" + en + " || " + pred + ", " + node->ops[opIdx]->getExtra(0) + ")";
  node->insts.push_back(inst);
  valName.push_back(std::pair(-1, ""));
}

void computeNode(Node* node) {
  if(node->ops.size() == 0 && node->operands.size() == 0) return;
  if(node->ops.size() == 0) {
    Assert(node->operands.size() == 1, "Invalid operands size(%d) for %s\n", node->operands.size(), node->name.c_str());
    node->insts.push_back("mpz_set(" + node->name + ", " + node->operands[0]->name + ")");
    return;
  }
  int prevIdx = node->operands.size() - 1;
  for(int i = node->ops.size() - 1; i >= 0; i--) {
    if(!node->ops[i]) {
      if(!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch(node->ops[i]->type) {
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
  Assert(valName.size() == 1, "Invalid valname size %d for node %s\n", valName.size(), node->name.c_str());
  valName.pop_back();
  topValid = false;
  
}

void instsGenerator(graph* g) {
  mpz_init(val);
  for(int i = 0; i < g->sorted.size(); i++) {
    if(g->sorted[i]->status == CONSTANT_NODE) continue;
    switch(g->sorted[i]->type) {
      case NODE_READER: case NODE_WRITER:
        for(Node* node : g->sorted[i]->member) {
          computeNode(node);
        }
        break;
      default:
        computeNode(g->sorted[i]);
    }
    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx = 0;
  }
  for(Node* node : g->active) {
    computeNode(node);
    g->maxTmp = MAX(tmpIdx, g->maxTmp);
    tmpIdx = 0;
  }
}

