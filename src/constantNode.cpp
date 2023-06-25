/**
 * @file ConstantNode
 * @brief ConstantNode 常量传播
 */

#include <gmp.h>
#include <map>
#include <stack>
#include <tuple>

#include "common.h"
#include "Node.h"
#include "graph.h"
#include "PNode.h"
#include "opFuncs.h"

#define N(i) g->sorted[i]
#define POTENTIAL_TYPE(n)                                                                                        \
  ((n->type == NODE_REG_DST) || (n->type == NODE_ACTIVE) || (n->type == NODE_OUT) || (n->type == NODE_OTHERS) || \
   (n->type) == NODE_MEMBER)

#define EXPR1INT_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt, mp_bitcnt_t n)
#define EXPR1_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt)
#define EXPR2_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src1, mp_bitcnt_t bitcnt1, mpz_t & src2, mp_bitcnt_t bitcnt2)

int p_stoi(const char* str);
std::pair<int, std::string> strBaseAll(std::string s);

static int constantNode = 0;
static int totalNode    = 0;

class mpz_wrap {
 public:
  mpz_wrap() { mpz_init(a); }
  mpz_t a;
};

static std::vector<mpz_wrap*> val;
static bool topValid = false;

static inline void allocVal() {
  mpz_wrap* wrap = new mpz_wrap();
  val.push_back(wrap);
}

static void setPrev(Node* node, int& prevIdx) {
  allocVal();
  mpz_set_str(val.back()->a, node->operands[prevIdx]->consVal.c_str(), 16);
  topValid = true;
  prevIdx--;
}

static void inline deleteAndPop() {
  delete val.back();
  val.pop_back();
}

// width num
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

static void cons_1expr1int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  Assert(expr1int1Map.find(op->name) != expr1int1Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR1INT_FUNC_TYPE, EXPR1INT_FUNC_TYPE> funcs = expr1int1Map[op->name];
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  if (op->name == "head" || op->name == "tail") n = op->getChild(0)->width - n;
  if (!topValid) setPrev(node, prevIdx);
  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val.back()->a, val.back()->a, op->getChild(0)->width, n);
}

static void cons_1expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  Assert(expr1Map.find(op->name) != expr1Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR1_FUNC_TYPE, EXPR1_FUNC_TYPE> funcs = expr1Map[op->name];
  if (!topValid) setPrev(node, prevIdx);
  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val.back()->a, val.back()->a, op->getChild(0)->width);
}

static void cons_2expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  Assert(expr2Map.find(op->name) != expr2Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR2_FUNC_TYPE, EXPR2_FUNC_TYPE> funcs = expr2Map[op->name];

  if (!topValid) setPrev(node, prevIdx);

  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val[val.size() - 2]->a, val.back()->a, op->getChild(0)->width,
                                                       val[val.size() - 2]->a, op->getChild(1)->width);
  deleteAndPop();
}

static void cons_1expr2int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->ops[opIdx];
  if (!topValid) { setPrev(node, prevIdx); }
  if (op->sign) TODO();
  else u_bits(val.back()->a, val.back()->a, op->getChild(0)->width, p_stoi(op->getExtra(0).c_str()),
           p_stoi(op->getExtra(1).c_str()));
}

static void cons_mux(Node* node, int opIdx, int& prevIdx) {
  if (!topValid) setPrev(node, prevIdx);
  us_mux(val[val.size() - 3]->a, val.back()->a, val[val.size() - 2]->a, val[val.size() - 3]->a);
  deleteAndPop();
  deleteAndPop();
}

static void cons_intInit(Node* node, int opIdx) {
  int base;
  std::string cons;
  std::tie(base, cons) = strBaseAll(node->ops[opIdx]->getExtra(0));
  allocVal();
  mpz_set_str(val.back()->a, cons.c_str(), base);
  topValid = true;
}

void computeConstant(Node* node) {
  int prevIdx = node->operands.size() - 1;
  for (int i = node->ops.size() - 1; i >= 0; i--) {
    if (!node->ops[i]) {
      if (!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch (node->ops[i]->type) {
      case P_1EXPR1INT: cons_1expr1int(node, i, prevIdx); break;
      case P_1EXPR2INT: cons_1expr2int(node, i, prevIdx); break;
      case P_2EXPR: cons_2expr(node, i, prevIdx); break;
      case P_1EXPR: cons_1expr(node, i, prevIdx); break;
      case P_EXPR_MUX: cons_mux(node, i, prevIdx); break;
      case P_EXPR_INT_INIT: cons_intInit(node, i); break;
      case P_PRINTF:
      case P_ASSERT: break;  // TODO
      default: Assert(0, "Invalid constantNode(%s) with type %d\n", node->name.c_str(), node->ops[i]->type);
    }
    char* str             = mpz_get_str(NULL, 16, val[0]->a);
    node->ops[i]->consVal = str;
    free(str);
  }
}

void checkAndComputeConstant(Node* node) {
  bool isConstant = true;
  for (Node* operand : node->operands) {
    if (!POTENTIAL_TYPE(operand) || operand->status != CONSTANT_NODE) {
      isConstant = false;
      return;
    }
  }
  if (isConstant) {  // compute constant
    constantNode++;
    if (node->ops.size() == 0) {
      if (node->operands.size() != 0) {
        Assert(node->operands.size() == 1, "Invalid operand size %ld\n", node->operands.size());
        node->consVal = node->operands[0]->consVal;
      } else {
        node->consVal = "0";
      }
      node->status = CONSTANT_NODE;
      // std::cout << "set " << node->name << " = " << node->consVal << std::endl;
      return;
    }
    topValid = false;
    computeConstant(node);
    Assert(val.size() == 1, "Invalid val size %ld for %s\n", val.size(), node->name.c_str());
    char* str     = mpz_get_str(NULL, 16, val[0]->a);
    node->status  = CONSTANT_NODE;
    node->consVal = str;
    // std::cout << "set " << node->name << " = " << node->consVal << std::endl;
    free(str);
    deleteAndPop();
  }
}

// compute constant val
void constantPropagation(graph* g) {
  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (N(i)->status != VALID_NODE) continue;
    totalNode++;
    switch (N(i)->type) {
      case NODE_READER: {
        checkAndComputeConstant(N(i)->member[0]);  // addr
        checkAndComputeConstant(N(i)->member[1]);  // en
        break;
      }
      case NODE_WRITER: {
        checkAndComputeConstant(N(i)->member[0]);  // addr
        checkAndComputeConstant(N(i)->member[1]);  // en
        checkAndComputeConstant(N(i)->member[3]);  // data
        checkAndComputeConstant(N(i)->member[4]);  // mask
        break;
      }
      case NODE_READWRITER: {
        checkAndComputeConstant(N(i)->member[0]);  // addr
        checkAndComputeConstant(N(i)->member[1]);  // en
        checkAndComputeConstant(N(i)->member[4]);  // wdata
        checkAndComputeConstant(N(i)->member[5]);  // wmask
        checkAndComputeConstant(N(i)->member[6]);  // wmode
        break;
      }
      case NODE_OTHERS:
      case NODE_OUT:
      case NODE_ACTIVE:
      case NODE_REG_DST: {
        checkAndComputeConstant(N(i));
        break;
      }
      default: break;
    }
  }

  for (size_t i = 0; i < g->sorted.size(); i++) {
    if (N(i)->status == VALID_NODE) {
      N(i)->prev.erase(
          std::remove_if(N(i)->prev.begin(), N(i)->prev.end(), [](const Node* n) { return n->status != VALID_NODE; }),
          N(i)->prev.end());
      N(i)->next.erase(
          std::remove_if(N(i)->next.begin(), N(i)->next.end(), [](const Node* n) { return n->status != VALID_NODE; }),
          N(i)->next.end());
    }
  }

  g->sorted.erase(
      std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->status != VALID_NODE; }),
      g->sorted.end());

  std::cout << "find " << constantNode << " constant nodes (" << totalNode << ")\n";
}
