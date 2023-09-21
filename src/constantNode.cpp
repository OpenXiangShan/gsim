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

#define POTENTIAL_TYPE(n) (potentialUncheckedType(n) || (n->type == NODE_REG_SRC))

#define EXPR1INT_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt, mp_bitcnt_t n)
#define EXPR1_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src, mp_bitcnt_t bitcnt)
#define EXPR2_FUNC_TYPE void (*)(mpz_t & dst, mpz_t & src1, mp_bitcnt_t bitcnt1, mpz_t & src2, mp_bitcnt_t bitcnt2)

int p_stoi(const char* str);
std::pair<int, std::string> strBaseAll(std::string s);
void checkAndComputeConstant(Node* node);
static std::set<Node*> checked;
static int constantNode = 0;
static int totalNode    = 0;

class mpz_wrap {
 public:
  mpz_wrap() { mpz_init(a); n = NULL; }
  mpz_t a;
  Node* n;
  std::vector<int>index;
};

static std::vector<mpz_wrap*> val;
static bool topValid = false;

static inline bool potentialUncheckedType(Node* n) {
  if (n->type == NODE_MEMBER) {
    switch(n->parent->type) {
      case NODE_READER:
        return n == n->parent->member[0] || n == n->parent->member[1];
      case NODE_WRITER:
        return true;
      case NODE_READWRITER:
        return n != n->parent->member[2] && n != n->parent->member[3];
      default:
        Assert(0, "Should not reach here\n");
    }
  }
  return (n->type == NODE_REG_DST) || (n->type == NODE_ACTIVE) || (n->type == NODE_OUT) || (n->type == NODE_OTHERS);
}

static inline void allocVal() {
  mpz_wrap* wrap = new mpz_wrap();
  val.push_back(wrap);
}

static void setPrev(Node* node, int& prevIdx) {
  allocVal();
  if (node->workingVal->operands[prevIdx]->dimension.size() == 0) {
    mpz_set_str(val.back()->a, node->workingVal->operands[prevIdx]->workingVal->consVal.c_str(), 16);
  }
  val.back()->n = node->workingVal->operands[prevIdx];
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
  PNode* op = node->workingVal->ops[opIdx];
  Assert(expr1int1Map.find(op->name) != expr1int1Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR1INT_FUNC_TYPE, EXPR1INT_FUNC_TYPE> funcs = expr1int1Map[op->name];
  unsigned long n = p_stoi(op->getExtra(0).c_str());
  if (op->name == "head" || op->name == "tail") n = op->getChild(0)->width - n;
  if (!topValid) setPrev(node, prevIdx);
  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val.back()->a, val.back()->a, op->getChild(0)->width, n);
}

static void cons_1expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  Assert(expr1Map.find(op->name) != expr1Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR1_FUNC_TYPE, EXPR1_FUNC_TYPE> funcs = expr1Map[op->name];
  if (!topValid) setPrev(node, prevIdx);
  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val.back()->a, val.back()->a, op->getChild(0)->width);
}

static void cons_2expr(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  Assert(expr2Map.find(op->name) != expr2Map.end(), "Operation %s not found\n", op->name.c_str());
  std::tuple<EXPR2_FUNC_TYPE, EXPR2_FUNC_TYPE> funcs = expr2Map[op->name];

  if (!topValid) setPrev(node, prevIdx);

  (op->sign ? std::get<1>(funcs) : std::get<0>(funcs))(val[val.size() - 2]->a, val.back()->a, op->getChild(0)->width,
                                                       val[val.size() - 2]->a, op->getChild(1)->width);
  deleteAndPop();
}

static void cons_1expr2int(Node* node, int opIdx, int& prevIdx) {
  PNode* op = node->workingVal->ops[opIdx];
  if (!topValid) { setPrev(node, prevIdx); }
  if (op->sign) {
    std::cout << op->name << " " << op->lineno << " " << node->name << std::endl;
    TODO();
  }
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
  std::tie(base, cons) = strBaseAll(node->workingVal->ops[opIdx]->getExtra(0));
  allocVal();
  mpz_set_str(val.back()->a, cons.c_str(), base);
  topValid = true;
}

void computeConstant(Node* node) {
  int prevIdx = node->workingVal->operands.size() - 1;
  for (int i = node->workingVal->ops.size() - 1; i >= 0; i--) {
    if (!node->workingVal->ops[i]) {
      if (!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch (node->workingVal->ops[i]->type) {
      case P_1EXPR1INT: cons_1expr1int(node, i, prevIdx); break;
      case P_1EXPR2INT: cons_1expr2int(node, i, prevIdx); break;
      case P_2EXPR: cons_2expr(node, i, prevIdx); break;
      case P_1EXPR: cons_1expr(node, i, prevIdx); break;
      case P_EXPR_MUX: cons_mux(node, i, prevIdx); break;
      case P_EXPR_INT_INIT: cons_intInit(node, i); break;
      case P_WHEN: cons_mux(node, i, prevIdx); break;
      case P_PRINTF:
      case P_ASSERT: break;  // TODO
      default: Assert(0, "Invalid constantNode(%s) with type %d\n", node->name.c_str(), node->workingVal->ops[i]->type);
    }
    char* str             = mpz_get_str(NULL, 16, val[0]->a);
    node->workingVal->ops[i]->consVal = str;
    free(str);
  }
}

void constantReCheck(Node* node) { // re-check node->next
  for (Node* n : node->next) {
    if (n->status == CONSTANT_NODE) continue;
    checkAndComputeConstant(n);
    if (n->status == CONSTANT_NODE) constantReCheck(n);
  }
}

bool cons_l_index(Node* node, int opIdx, int& prevIdx, std::vector<int>& index) {
  PNode* op = node->workingVal->ops[opIdx];
  index.push_back(std::stoi(op->name));
  return op->lineno == 0;
}

void computeArrayMember(Node* node, Node* parent) {
  int prevIdx = node->workingVal->operands.size() - 1;
  std::vector<int> index;
  bool indexEnd = false;
  for (int i = node->workingVal->ops.size() - 1; i >= 0; i --) {
    if (!node->workingVal->ops[i]) {
      if (!topValid) setPrev(node, prevIdx);
      topValid = false;
      continue;
    }
    switch (node->workingVal->ops[i]->type) {
      case P_1EXPR1INT: cons_1expr1int(node, i, prevIdx); break;
      case P_1EXPR2INT: cons_1expr2int(node, i, prevIdx); break;
      case P_2EXPR: cons_2expr(node, i, prevIdx); break;
      case P_1EXPR: cons_1expr(node, i, prevIdx); break;
      case P_EXPR_MUX: cons_mux(node, i, prevIdx); break;
      case P_EXPR_INT_INIT: cons_intInit(node, i); break;
      case P_L_CONS_INDEX: indexEnd = cons_l_index(node, i, prevIdx, index); break;
      default: Assert(0, "Invalid constantNode(%s) with type %d\n", node->name.c_str(), node->workingVal->ops[i]->type);
    }
    if (indexEnd) {
      if (!topValid) setPrev(node, prevIdx);
      int dstIdx = 0;
      for (size_t t1 = 0; t1 < index.size(); t1 ++) dstIdx = dstIdx * parent->dimension[t1] + index[t1];
      for (size_t t1 = index.size(); t1 < parent->dimension.size(); t1 ++) dstIdx *= parent->dimension[t1];
      if (val.back()->n && val.back()->n->dimension.size() != val.back()->index.size()) { // array set
        Assert(val.back()->n->dimension.size() > val.back()->index.size(), "index out of bound (%s)\n", val.back()->n->name.c_str());
        int base = 0, num = 1;
        for (size_t t1 = 0; t1 < val.back()->index.size(); t1 ++) {
          base = base * val.back()->n->dimension[t1] + val.back()->index[t1];
        }
        for (size_t t1 = val.back()->index.size(); t1 < val.back()->n->dimension.size(); t1 ++) {
          num *= val.back()->n->dimension[t1];
        }

        for (int t1 = base * num; t1 < num; t1 ++) {
          parent->consArray[dstIdx ++] = val.back()->n->consArray[t1];
        }
      } else {
        char* str = mpz_get_str(NULL, 16, val[val.size()-1]->a);
        parent->consArray[dstIdx] = str;
        free(str);
      }
      index.erase(index.begin(), index.end());
      indexEnd = false;
      topValid = false;
      deleteAndPop();
    }
  }
}

void computeArrayConstant(Node* node) {
  topValid = false;
  int entryNum = 1;
  for (size_t i = 0; i < node->dimension.size(); i ++) entryNum *= node->dimension[i];
  Assert(node->consArray.size() == 0, "%s is already computed!\n", node->name.c_str());
  node->consArray.resize(entryNum);

  computeArrayMember(node, node);
  for (Node* member : node->member) computeArrayMember(member, node);

  for (size_t i = 0; i < node->consArray.size(); i ++) {
    if (node->consArray[i].length() == 0) node->consArray[i] = "0";
    node->workingVal->consVal += node->consArray[i];
    if (i != (node->consArray.size() - 1)) node->workingVal->consVal += ", ";
  }
  // std::cout << "consArray " << node->name << " " << node->workingVal->consVal << std::endl;
}


void checkAndComputeConstant(Node* node) {
  // std::cout << "checking " << node->name << std::endl;
  checked.insert(node);
  bool isConstant = true;
  for (Node* operand : node->workingVal->operands) {
    if (potentialUncheckedType(operand) && checked.find(operand) == checked.end()) {
      checkAndComputeConstant(operand);
    }
    if (!POTENTIAL_TYPE(operand) || operand->status != CONSTANT_NODE) {
      isConstant = false;
    }
  }
  if (node->dimension.size() != 0) {
    for (Node* member : node->member) {
      for (Node* operand : member->workingVal->operands) {
        if (potentialUncheckedType(operand) && checked.find(operand) == checked.end()) {
          checkAndComputeConstant(operand);
        }
        if (!POTENTIAL_TYPE(operand) || operand->status != CONSTANT_NODE) {
          isConstant = false;
        }
      }
    }
  }
  if (isConstant) {  // compute constant
  // std::cout << node->name << " is constant " << std::endl;
    constantNode++;
    if (node->dimension.size() != 0) {
      computeArrayConstant(node);
      node->status  = CONSTANT_NODE;
      return;
    }
    if (node->workingVal->ops.size() == 0) {
      if (node->workingVal->operands.size() != 0) {
        Assert(node->workingVal->operands.size() == 1, "Invalid operand size %ld for %s\n", node->workingVal->operands.size(), node->name.c_str());
        node->workingVal->consVal = node->workingVal->operands[0]->workingVal->consVal;
      } else {
        node->workingVal->consVal = "0";
      }

    } else {
      topValid = false;
      computeConstant(node);
      Assert(val.size() == 1, "Invalid val size %ld for %s\n", val.size(), node->name.c_str());
      char* str     = mpz_get_str(NULL, 16, val[0]->a);
      node->workingVal->consVal = str;
      free(str);
      deleteAndPop();
    }
    node->status  = CONSTANT_NODE;
    // std::cout << "set " << node->name << " = " << node->workingVal->consVal << std::endl;

  } else {
    // std::cout << node->name << " is not constant\n";
  }
}

void removeInValid(std::set<Node*>& nodeSet) {
  for (auto iter = nodeSet.begin(); iter != nodeSet.end(); ) {
    if ((*iter)->status != VALID_NODE) iter = nodeSet.erase(iter);
    else iter ++;
  }
}

// compute constant val
void constantPropagation(graph* g) {
  for (size_t i = 0; i < g->sorted.size(); i++) {
    // std::cout << N(i)->name << " " << N(i)->type << " " << N(i)->status << "======="<< std::endl;
    if (N(i)->status == CONSTANT_NODE) {
      checkAndComputeConstant(N(i));
      continue;
    }
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
      removeInValid(N(i)->prev);
      removeInValid(N(i)->next);
    }
  }


  g->sorted.erase(
      std::remove_if(g->sorted.begin(), g->sorted.end(), [](const Node* n) { return n->status != VALID_NODE; }),
      g->sorted.end());

  std::cout << "find " << constantNode << " constant nodes (" << totalNode << ")\n";
#if 0
  for (Node* superNode : g->superNodes) {
    std::cout << superNode->name << " " << superNode->id << ": ";
    for (Node* member : superNode->setOrder) std::cout << member->name <<  "(" << member->id << ", " << (member->type == NODE_INVALID ? "invalid " : "valid ") << member->prev.size()<< " " << member->next.size() << ") ";
    std::cout << "\nprev: " << std::endl;
    for (Node* prevSuper : superNode->prev) std::cout << "  " <<prevSuper->name << " " << prevSuper->id << std::endl;
    std::cout << "next: " << std::endl;
    for (Node* nextSuper : superNode->next) std::cout << "  " <<nextSuper->name << " " << nextSuper->id << std::endl;
  }
#endif
}
