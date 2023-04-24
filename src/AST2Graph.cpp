#include "common.h"
#include "PNode.h"
#include "Node.h"
#include "graph.h"
#include <map>

#define SET_TYPE(x, y) do{ x->width = y->width; x->sign = y->sign;} while(0)
#define COMMU 1
#define NO_COMMU 0
#define SIGN_UINT 0
#define SIGN_SINT 1
#define SIGN_CHILD 2
#define SIGN_NEG_CHILD 3
#define FUNC_NAME(s) (std::string("s_") + s)

static int tmpIdx = 0;
#define CUR_TMP (tmp = (std::string("tmp") + std::to_string(tmpIdx)))
#define NEW_TMP (tmp = (std::string("tmp") + std::to_string(tmpIdx ++)))
#define clear_tmp tmpIdx = 0
std::string tmp;

#define EXPR_CONSTANT 0
#define EXPR_VAR 1
#define expr_type std::pair<int, std::string>
#define CONS(expr) (expr.first ? (std::string("mpz_get_ui(") + expr.second + ")") : expr.second)

static inline void insts_set_neq(Node* node, std::string& srcStr) {
  if(node->name != srcStr)
      node->insts.push_back(std::string("mpz_set(") + node->name + ", " + srcStr + ")");
}
static inline void insts_set_neq_ui(Node* node, std::string& srcStr) {
  node->insts.push_back(std::string("mpz_set_ui(") + node->name + ", " + srcStr + ")");
}
static inline void insts_set_expr_neq(Node* n, expr_type& src) {
  if(src.first) {
    insts_set_neq(n, src.second);
  } else {
    insts_set_neq_ui(n, src.second);
  }
}
#define insts_1expr(n, func, dst, expr1) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ")")
#define insts_2expr(n, func, dst, expr1, expr2) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ")")
#define insts_3expr(n, func, dst, expr1, expr2, expr3) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ")")
#define insts_4expr(n, func, dst, expr1, expr2, expr3, expr4) \
  n->insts.push_back(func + "(" + dst + ", " + expr1 + ", " + expr2 + ", " + expr3 + ", " + expr4 + ")")

#define memory_member(str, parent) \
  do {  \
    Node* rn_##str = new Node(NODE_MEMBER); \
    parent->member.push_back(rn_##str); \
    rn_##str->member.push_back(parent); \
    rn_##str->name = parent->name + "_" + #str; \
    addSignal(rn_##str->name, rn_##str); \
  } while(0)

static int maxWidth(int a, int b, bool sign = 0) {
  return MAX(a, b);
}

static int minWidth(int a, int b, bool sign = 0) {
  return MAX(a, b);
}

static int maxWidthPlus1(int a, int b, bool sign = 0) {
  return MAX(a, b) + 1;
}

static int sumWidth(int a, int b, bool sign = 0) {
  return a + b;
}

static int minusWidthPos(int a, int b, bool sign = 0) {
  return MAX(1, a - b);
}

static int minusWidth(int a, int b, bool sign = 0) {
  return a - b;
}

static int divWidth(int a, int b, bool sign = 0) {
  return sign? a + 1 : a;
}

static int cvtWidth(int a, int b, bool sign = 0) {
  return sign ? a : a + 1;
}

static int boolWidth(int a, int b, bool sign = 0) {
  return 1;
}

static int dshlWidth(int a, int b, bool sign = 0) {
  return a + 1 << b -1;
}

static int firstWidth(int a, int b, bool sign = 0){
  return a;
}

static int secondWidth(int a, int b, bool sign = 0){
  return b;
}

// 0: uint; 1: child sign
                              // sign   widthFunc
std::map<std::string, std::tuple<bool, int (*)(int, int, bool), const char*, const char*, const char*, const char*>> expr2Map = {
  {"add",   {1, maxWidthPlus1,  "s_mpz_add",    "s_mpz_add_ui_r",     "s_mpz_add_ui_l",     "s_mpz_add_ui2",   }},
  {"sub",   {1, maxWidthPlus1,  "s_mpz_sub",    "s_mpz_sub_ui_r",     "s_mpz_sub_ui_l",     "s_mpz_sub_ui2",   }},
  {"mul",   {1, sumWidth,       "s_mpz_mul",    "s_mpz_mul_ui_r",     "s_mpz_mul_ui_l",     "s_mpz_mul_ui2",   }},
  {"div",   {1, divWidth,       "s_mpz_div",    "s_mpz_div_ui_r",     "s_mpz_div_ui_l",     "s_mpz_div_ui2",}},
  {"rem",   {1, minWidth,       "s_mpz_rem",    "s_mpz_rem_ui_r",     "s_mpz_rem_ui_l",     "s_mpz_rem_ui2",}},
  {"lt",    {0, boolWidth,      "s_mpz_lt",     "s_mpz_lt_ui_r",      "s_mpz_lt_ui_l",      "s_mpz_lt_ui2",    }},
  {"leq",   {0, boolWidth,      "s_mpz_leq",    "s_mpz_leq_ui_r",     "s_mpz_leq_ui_l",     "s_mpz_leq_ui2",   }},
  {"gt",    {0, boolWidth,      "s_mpz_gt",     "s_mpz_gt_ui_r",      "s_mpz_gt_ui_l",      "s_mpz_gt_ui2",    }},
  {"geq",   {0, boolWidth,      "s_mpz_geq",    "s_mpz_geq_ui_r",     "s_mpz_geq_ui_l",     "s_mpz_geq_ui2",   }},
  {"eq",    {0, boolWidth,      "s_mpz_eq",     "s_mpz_eq_ui_r",      "s_mpz_eq_ui_l",      "s_mpz_eq_ui2",    }},
  {"neq",   {0, boolWidth,      "s_mpz_neq",    "s_mpz_neq_ui_r",     "s_mpz_neq_ui_l",     "s_mpz_neq_ui2",   }},
  {"dshl",  {1, dshlWidth,      "s_mpz_dshl",   "s_mpz_dshl_ui_r",    "s_mpz_dshl_ui_l",    "s_mpz_dshl_ui2",  }},
  {"dshr",  {1, firstWidth,     "s_mpz_dshr",   "s_mpz_dshr_ui_r",    "s_mpz_dshr_ui_l",    "s_mpz_dshr_ui2",  }},
  {"and",   {0, maxWidth,       "s_mpz_and",    "s_mpz_and_ui_r",     "s_mpz_and_ui_l",     "s_mpz_and_ui2",   }},
  {"or",    {0, maxWidth,       "s_mpz_ior",    "s_mpz_ior_ui_r",     "s_mpz_ior_ui_l",     "s_mpz_ior_ui2",   }},
  {"xor",   {0, maxWidth,       "s_mpz_xor",    "s_mpz_xor_ui_r",     "s_mpz_xor_ui_l",     "s_mpz_xor_ui2",   }},
  {"cat",   {0, sumWidth,       "s_cat",        "s_cat_ui_r",         "s_cat_ui_l",         "s_cat_ui2",       }},
};

                                            // width num
std::map<std::string, std::tuple<bool, bool, int (*)(int, int, bool)>> expr1int1Map = {
  {"pad", {1, 0, maxWidth}},
  {"shl", {1, 0, sumWidth}},
  {"shr", {1, 0, minusWidthPos}},
  {"head", {0, 1, secondWidth}},
  {"tail", {0, 1, minusWidth}},
};

// 0: uint 1: reverse child sign
std::map<std::string, std::tuple<uint8_t, int (*)(int, int, bool)>> expr1Map = {
  {"asUInt",  {SIGN_UINT, firstWidth}},
  {"asSInt",  {SIGN_SINT, firstWidth}},
  {"asClock", {SIGN_UINT, boolWidth}},
  {"cvt",     {SIGN_SINT, cvtWidth}},
  {"neg",     {SIGN_SINT, maxWidthPlus1}},  // a + 1 (second is zero)
  {"not",     {SIGN_UINT, firstWidth}},
  {"andr",    {SIGN_UINT, boolWidth}},
  {"orr",     {SIGN_UINT, boolWidth}},
  {"xorr",    {SIGN_UINT, boolWidth}},
};

static std::map<std::string, PNode*> moduleMap;
static std::map<std::string, Node*> allSignals;
void visitStmts(std::string prefix, graph* g, PNode* stmts);


expr_type visitExpr(std::string& name, std::string prefix, Node* n, PNode* expr);
void visitType(Node* n, PNode* ptype);

int p_stoi(const char* str);
std::string cons2str(std::string s);

void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  allSignals[s] = n;
}

void addEdge(Node* src, Node* dst) {
  if(dst->type == NODE_MEMBER) {
    dst = dst->member[0];
  }
  if(src->type == NODE_MEMBER) src = src->member[0];
  dst->inEdge ++;
  src->next.push_back(dst);
  // std::cout << src->name << " -> " << dst->name << std::endl;
}

void addEdge(std::string str, Node* dst) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  addEdge(allSignals[str], dst);
}

Node* str2Node(std::string str) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  return allSignals[str];
}

void visitPorts(std::string prefix, graph* g, PNode* ports) { // treat as node
  for(int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = new Node();
    visitType(io, port->getChild(0));
    io->name = prefix + port->name;
    addSignal(io->name, io);
  }
}

void visitModule(std::string prefix, graph* g, PNode* module) {
  MUX_DEBUG(std::cout << "visit " << module->name << std::endl);
  visitPorts(prefix, g, module->getChild(0));
  visitStmts(prefix, g, module->getChild(1));
  MUX_DEBUG(std::cout << "leave " << module->name << std::endl);
}

void visitExtModule(std::string prefix, graph* g, PNode* module) {
  MUX_DEBUG(std::cout << "visit " << module->name << std::endl);
  visitPorts(prefix, g, module->getChild(0));
  MUX_DEBUG(std::cout << "leave " << module->name << std::endl);
}

void visitType(Node* n, PNode* ptype) {
  switch(ptype->type) {
    case P_Clock: n->width = 1; break;
    case P_INT_TYPE: SET_TYPE(n, ptype); break;
    default: TODO();
  }
}

expr_type visit1Expr1Int(std::string& name, std::string prefix, Node* n, PNode* expr) { // pad|shl|shr|head|tail
  Assert(expr1int1Map.find(expr->name) != expr1int1Map.end(), "Operation %s not found\n", expr->name.c_str());
  std::tuple<bool, bool, int (*)(int, int, bool)>info = expr1int1Map[expr->name];
  expr_type src = visitExpr(NEW_TMP, prefix, n, expr->getChild(0));;
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  int arg = p_stoi(expr->getExtra(0).c_str());
  expr->width = std::get<2>(info)(expr->getChild(0)->width, arg, false);
  Assert(src.first, "Expr in 1Expr1Int must be var %s\n", src.second.c_str());
  std::string cons = (std::get<1>(info) ? std::to_string(expr->getChild(0)->width - arg) : cons2str(expr->getExtra(0)));
  insts_2expr(n, FUNC_NAME(expr->name), name, src.second, cons);
  return std::make_pair(EXPR_VAR, name);
}

expr_type visit1Expr2Int(std::string& name, std::string prefix, Node* n, PNode* expr){ // bits
  expr_type src = visitExpr(NEW_TMP, prefix, n, expr->getChild(0));
  expr->sign = 0;
  expr->width = p_stoi(expr->getExtra(0).c_str()) - p_stoi(expr->getExtra(1).c_str()) + 1;
  Assert(src.first, "Expr in 1Expr2Int must be var %s\n", src.second.c_str());
  insts_3expr(n, FUNC_NAME(expr->name), name, src.second, cons2str(expr->getExtra(0)), cons2str(expr->getExtra(1)));
  return std::make_pair(EXPR_VAR, name);
}

expr_type visit2Expr(std::string& name, std::string prefix, Node* n, PNode* expr) { // add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
  Assert(expr->getChildNum() == 2, "Invalid childNum for expr %s\n", expr->name.c_str());
  expr_type left = visitExpr(NEW_TMP, prefix, n, expr->getChild(0));
  expr_type right = visitExpr(NEW_TMP, prefix, n, expr->getChild(1));
  // std::string left = visitExpr(NEW_TMP, prefix, n, expr->getChild(0));
  // std::string right = visitExpr(NEW_TMP, prefix, n, expr->getChild(1));
  Assert(expr2Map.find(expr->name) != expr2Map.end(), "Operation %s not found\n", expr->name.c_str());
  std::tuple<bool, int (*)(int, int, bool), const char*, const char*, const char*, const char*>info = expr2Map[expr->name];
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  expr->width = std::get<1>(info)(expr->getChild(0)->width, expr->getChild(1)->width, expr->getChild(0)->sign);
  if(left.first&& right.first)
    insts_4expr(n, std::string(std::get<2>(info)), name, left.second, std::to_string(expr->getChild(0)->width), right.second, std::to_string(expr->getChild(1)->width));
  else if(left.first && !right.first)
    insts_3expr(n, std::string(std::get<3>(info)), name, left.second, right.second, std::to_string(expr->getChild(1)->width));
  else if(!left.first && right.first)
    insts_4expr(n, std::string(std::get<4>(info)), name, left.second, std::to_string(expr->getChild(0)->width), right.second, std::to_string(expr->getChild(1)->width));
  else
    insts_4expr(n, std::string(std::get<5>(info)), name, left.second, std::to_string(expr->getChild(0)->width), right.second, std::to_string(expr->getChild(1)->width));
  return std::make_pair(EXPR_VAR, name);
}

expr_type visit1Expr(std::string& name, std::string prefix, Node* n, PNode* expr) { // asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr
  std::tuple<uint8_t, int (*)(int, int, bool)>info = expr1Map[expr->name];
  expr->sign = std::get<0>(info);
  expr->width = std::get<1>(info)(expr->getChild(0)->width, 0, expr->getChild(0)->sign);
  expr_type src = visitExpr(tmp, prefix, n, expr->getChild(0));
  // Assert(src.first, "Expr in 1Expr(%s) must be var %s\n", expr->name.c_str(), src.second.c_str());
  if(src.first){
    insts_2expr(n, FUNC_NAME(expr->name), name, src.second, std::to_string(expr->getChild(0)->width));
    return std::make_pair(EXPR_VAR, name);
  } else {
    return std::make_pair(EXPR_CONSTANT, src.second.c_str());
  }
}

std::string visitReference(std::string prefix, PNode* expr) { // return ref name
  std::string ret;
  if(expr->getChildNum() == 0) {
    ret = prefix + expr->name;
    // addEdge(ret, n);
    return ret;
  } else {
    ret = prefix + expr->name;
    for(int i = 0; i < expr->getChildNum(); i++) {
      PNode* child = expr->getChild(i);
      // SET_TYPE(expr, expr->getChild(i));
      switch(child->type) {
        case P_REF_DOT:
          ret += "_" + visitReference("", child);
          break;
        default: Assert(0, "TODO: invalid ref child type(%d) for %s\n", child->type, expr->name.c_str());
      }
    }
    return ret;
  }
}

expr_type visitMux(std::string& name, std::string prefix, Node* n, PNode* mux) {
  Assert(mux->getChildNum() == 3, "Invalid childNum(%d) for Mux\n", mux->getChildNum());

  expr_type cond = visitExpr(NEW_TMP, prefix, n, mux->getChild(0));
  expr_type expr1 = visitExpr(name, prefix, n, mux->getChild(1));
  expr_type expr2 = visitExpr(name, prefix, n, mux->getChild(2));
  std::string cond_true  = (expr1.first ? std::string("mpz_set(") : std::string("mpz_set_ui(")) + name + ", " + expr1.second + ")";
  std::string cond_false = (expr2.first ? std::string("mpz_set(") : std::string("mpz_set_ui(")) + name + ", " + expr2.second + ")";
  std::string cond_str = cond.first ? (std::string("mpz_cmp_ui(") + cond.second + ", 0) ") : cond.second;
  n->insts.push_back(cond_str + "?" + cond_true + " : " + cond_false);
  SET_TYPE(mux, mux->getChild(1));
  return std::make_pair(EXPR_VAR, name);
}

std::string cons2str(std::string s) {
  if (s.length() <= 1) return s;
  std::string ret;
  int idx = 1;
  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }
  if (s[0] == 'b') ret += "0b";
  else if(s[0] == 'o') ret += "0";
  else if(s[0] == 'h') ret += "0x";
  else idx = 0;
  ret += s.substr(idx);
  return ret;
}

expr_type visitExpr(std::string& name, std::string prefix, Node* n, PNode* expr) { // return varName & update connect
  std::string ret;
  switch(expr->type) {
    case P_1EXPR1INT: return visit1Expr1Int(name, prefix, n, expr);
    case P_2EXPR: return visit2Expr(name, prefix, n, expr);
    case P_REF: ret = visitReference(prefix, expr); if(n->type != NODE_ACTIVE) addEdge(ret, n); SET_TYPE(expr, allSignals[ret]); return std::make_pair(EXPR_VAR, ret);
    case P_EXPR_MUX: return visitMux(name, prefix, n, expr);
    case P_EXPR_INT_INIT: return std::make_pair(EXPR_CONSTANT, cons2str(expr->getExtra(0).substr(1, expr->getExtra(0).length()-2)));
    case P_1EXPR: return visit1Expr(name, prefix, n, expr);
    case P_1EXPR2INT: return visit1Expr2Int(name, prefix, n, expr);
    default: std::cout << expr->type << std::endl; TODO();
  }

}

Node* visitNode(std::string prefix, PNode* node) { // generate new node and connect
  Node* newSig = new Node();
  newSig->name = prefix + node->name;
  addSignal(newSig->name, newSig);
  Assert(node->getChildNum() >= 1, "Invalid childNum for node %s\n", node->name.c_str());
  expr_type right = visitExpr(newSig->name, prefix, newSig, node->getChild(0));
  insts_set_expr_neq(newSig, right);
  SET_TYPE(newSig, node->getChild(0));
  return newSig;
}

void visitConnect(std::string prefix, PNode* connect) {
  Assert(connect->getChildNum() == 2, "Invalid childNum for connect %s\n", connect->name.c_str());
  std::string strDst = visitReference(prefix, connect->getChild(0));
  Node* dst = str2Node(strDst);
  if(dst->type == NODE_REG_SRC) dst = dst->regNext;
  expr_type right = visitExpr(dst->name, prefix, dst, connect->getChild(1));
  insts_set_expr_neq(dst, right);
}

void visitRegDef(std::string prefix, graph* g, PNode* reg) {
  Node* newReg = new Node(NODE_REG_SRC);
  newReg->name = prefix + reg->name;
  visitType(newReg, reg->getChild(0));
  Node* nextReg = new Node(NODE_REG_DST);
  nextReg->name = newReg->name + "_next";
  SET_TYPE(nextReg, newReg);
  newReg->regNext = nextReg;
  nextReg->regNext = newReg;
  addSignal(newReg->name, newReg);
  addSignal(nextReg->name, nextReg);
  g->sources.push_back(newReg);
}

void visitPrintf(std::string prefix, graph* g, PNode* print) {
  Node* n = new Node(NODE_ACTIVE);
  n->name = prefix + print->name;
  expr_type cond = visitExpr(NEW_TMP, prefix, n, print->getChild(1));
  std::string cond_str = cond.first ? (std::string("mpz_cmp_ui(") + cond.second + ", 0)") : cond.second;
  std::string inst = std::string("if(") + cond_str + ") printf(" + print->getExtra(0);
  PNode* exprs = print->getChild(2);
  for(int i = 0; i < exprs->getChildNum(); i++ ) inst += "," + CONS(visitExpr(NEW_TMP, prefix, n, exprs->getChild(i)));
  n->insts.push_back(inst + ")");
  g->active.push_back(n);
}

void visitAssert(std::string prefix, graph* g, PNode* ass) {
  Node* n = new Node(NODE_ACTIVE);
  n->name = prefix + ass->name;
  expr_type pred = visitExpr(NEW_TMP, prefix, n, ass->getChild(1));
  expr_type en = visitExpr(NEW_TMP, prefix, n, ass->getChild(2));
  std::string en_str = en.first ? (std::string("mpz_cmp_ui(") + en.second + ", 0)") : en.second;
  std::string pred_str = pred.first ? (std::string("mpz_cmp_ui(") + pred.second + ", 0)") : pred.second;
  n->insts.push_back(std::string("Assert(!") + en_str + " || " + pred_str + ", " + ass->getExtra(0) + ")");
  g->active.push_back(n);
}

void visitMemory(std::string prefix, graph* g, PNode* memory) {
  Assert(memory->getChildNum() >= 5 , "Invalid childNum(%d) ", memory->getChildNum());
  Assert(memory->getChild(0)->type == P_DATATYPE, "Invalid child0 type(%d)\n", memory->getChild(0)->type);
  Node* n = new Node(NODE_MEMORY);
  n->name = prefix + memory->name;
  g->memory.push_back(n);
  visitType(n, memory->getChild(0)->getChild(0));
  Assert(memory->getChild(1)->type == P_DEPTH, "Invalid child0 type(%d)\n", memory->getChild(0)->type);
  Assert(n->width % 8 == 0, "invalid memory width %d\n", n);
  int depth = p_stoi(memory->getChild(1)->name.c_str());
  int readLatency = p_stoi(memory->getChild(2)->name.c_str());
  int writeLatency = p_stoi(memory->getChild(3)->name.c_str());
  n->latency[0] = readLatency;
  n->latency[1] = writeLatency;
  Assert(memory->getChild(4)->name == "undefined", "Invalid ruw %s\n", memory->getChild(4)->name.c_str());
// readers
  for(int i = 5; i < memory->getChildNum(); i++) {
    PNode* rw = memory->getChild(i);
    Node* rn = new Node();
    rn->name = n->name + "_" + rw->name;
    n->member.push_back(rn);
    rn->regNext = n;
    memory_member(addr, rn);
    memory_member(en, rn);
    memory_member(clk, rn);
    memory_member(data, rn);
    if(rw->type == P_READER) {
      rn->type = NODE_READER;
    } else if(rw->type == P_WRITER) {
      memory_member(mask, rn);
    } else {
      Assert(0, "Invalid rw type %d\n", rw->type);
    }
  }
}

void visitStmts(std::string prefix, graph* g, PNode* stmts) {
  PNode* module;
  for (int i = 0; i < stmts->getChildNum(); i++) {
    PNode* stmt = stmts->getChild(i);
    clear_tmp;
    switch(stmt->type) {
      case P_INST : 
        Assert(stmt->getExtraNum() >= 1 && moduleMap.find(stmt->getExtra(0)) != moduleMap.end(), "Module %s is not defined!\n", stmt->name.c_str());
        module = moduleMap[stmt->getExtra(0)];
        if(module->type == P_MOD) visitModule(prefix + stmt->name + "_", g, module);
        else if(module->type == P_EXTMOD) visitExtModule(prefix + stmt->name + "_", g, module);
        break;
      case P_NODE :
        visitNode(prefix, stmt);
        break;
      case P_CONNECT :
        visitConnect(prefix, stmt);
        break;
      case P_REG_DEF:
        visitRegDef(prefix, g, stmt);
        break;
      case P_PRINTF:
        visitPrintf(prefix, g, stmt);
        break;
      case P_ASSERT:
        visitAssert(prefix, g, stmt);
        break;
      case P_MEMORY:
        visitMemory(prefix, g, stmt);
        break;
      default: Assert(0, "Invalid stmt %s\n", stmt->name.c_str());
    }
    g->maxTmp = MAX(g->maxTmp, tmpIdx);
  }
}

void visitTopPorts(graph* g, PNode* ports) {
  for(int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = new Node();
    visitType(io, port->getChild(0));
    io->name = port->name;
    if(port->type == P_INPUT) {
      g->input.push_back(io);
      io->type = NODE_INP;
    } else if(port->type == P_OUTPUT) {
      g->output.push_back(io);
      io->type = NODE_OUT;
    } else {
      Assert(0, "Invalid port %s with type %d\n", port->name.c_str(), port->type);
    }
    addSignal(io->name, io);
  }
}

void visitTopModule(graph* g, PNode* topModule) {
  Assert(topModule->getChildNum() >= 2 && topModule->getChild(0)->type == P_PORTS, "Invalid top module %s\n", topModule->name.c_str());
  visitTopPorts(g, topModule->getChild(0));
  visitStmts("", g, topModule->getChild(1));
}

graph* AST2Garph(PNode* root) {
  graph* g = new graph();
  g->name = root->name;
  PNode* topModule = NULL;
  for (int i = 0; i < root->getChildNum(); i++) {
    PNode* module = root->getChild(i);
    if(module->name == root->name) topModule = module;
    moduleMap[module->name] = module;
  }
  Assert(topModule, "Top module can not be NULL\n");
  visitTopModule(g, topModule);
  for(auto n: allSignals) {
    if(n.second->type == NODE_OTHERS && n.second->inEdge == 0) {
      g->constant.push_back(n.second);
    }
  }
  return g;
}
