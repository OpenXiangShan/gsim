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
#define FUNC_NAME(sign, s) ((sign ? std::string("s_") : std::string("u_")) + s)

#define EXPR_CONSTANT 0
#define EXPR_VAR 1
#define expr_type std::pair<int, std::string>
#define CONS(expr) (expr.first ? (std::string("mpz_get_ui(") + expr.second + ")") : expr.second)

void visitExpr(std::string prefix, Node* n, PNode* expr);
void visitType(Node* n, PNode* ptype);

int p_stoi(const char* str);
std::string cons2str(std::string s);
std::pair<int, std::string> strBase(std::string s);

#define memory_member(str, parent, w) \
  do {  \
    Node* rn_##str = new Node(NODE_MEMBER); \
    parent->member.push_back(rn_##str); \
    rn_##str->regNext = parent; \
    rn_##str->name = parent->name + "$" + #str; \
    rn_##str->width = w; \
    addSignal(rn_##str->name, rn_##str); \
  } while(0)

int log2 (int x){return 31 - __builtin_clz(x);}

int p2(int x) { return 1 << (32 - __builtin_clz (x - 1)); }

static int maxWidth(int a, int b, bool sign = 0) {
  return MAX(a, b);
}

static int minWidth(int a, int b, bool sign = 0) {
  return MIN(a, b);
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
  return a + (1 << b) -1;
}

static int firstWidth(int a, int b, bool sign = 0){
  return a;
}

static int secondWidth(int a, int b, bool sign = 0){
  return b;
}

// 0: uint; 1: child sign
                              // sign   widthFunc
std::map<std::string, std::tuple<bool, bool, int (*)(int, int, bool)>> expr2Map = {
  {"add",   {1, 0, maxWidthPlus1,}},
  {"sub",   {1, 0, maxWidthPlus1,}},
  {"mul",   {1, 0, sumWidth,}},
  {"div",   {1, 0, divWidth,}},
  {"rem",   {1, 0, minWidth,}},
  {"lt",    {0, 0, boolWidth,}},
  {"leq",   {0, 0, boolWidth,}},
  {"gt",    {0, 0, boolWidth,}},
  {"geq",   {0, 0, boolWidth,}},
  {"eq",    {0, 0, boolWidth,}},
  {"neq",   {0, 0, boolWidth,}},
  {"dshl",  {1, 1, dshlWidth,}},
  {"dshr",  {1, 1, firstWidth,}},
  {"and",   {0, 1, maxWidth,}},
  {"or",    {0, 1, maxWidth,}},
  {"xor",   {0, 1, maxWidth,}},
  {"cat",   {0, 1, sumWidth,}},
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
  {"asAsyncReset", {SIGN_UINT, boolWidth}},
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

void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  allSignals[s] = n;
}

void addEdge(Node* src, Node* dst) {
  if(dst->type == NODE_REG_SRC) {
    dst = dst->regNext;
  }
  dst->operands.push_back(src);
  if(dst->type == NODE_MEMBER) {
    dst = dst->regNext;
  }
  if(src->type == NODE_MEMBER) {
    src = src->regNext;
  }
  dst->inEdge ++;
  src->next.push_back(dst);
  dst->prev.push_back(src); // get arguments in instGenerator
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
    SET_TYPE(port, io);
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

void visit1Expr1Int(std::string prefix, Node* n, PNode* expr) { // pad|shl|shr|head|tail
  Assert(expr1int1Map.find(expr->name) != expr1int1Map.end(), "Operation %s not found\n", expr->name.c_str());
  n->ops.push_back(expr);
  std::tuple<bool, bool, int (*)(int, int, bool)>info = expr1int1Map[expr->name];
  visitExpr(prefix, n, expr->getChild(0));
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  int arg = p_stoi(expr->getExtra(0).c_str());
  expr->width = std::get<2>(info)(expr->getChild(0)->width, arg, false);
  if(expr->getChild(0)->status == CONSTANT_PNODE) {
    expr->status = CONSTANT_PNODE;
  }
}

void visit1Expr2Int(std::string prefix, Node* n, PNode* expr){ // bits
  n->ops.push_back(expr);
  visitExpr(prefix, n, expr->getChild(0));
  expr->sign = 0;
  expr->width = p_stoi(expr->getExtra(0).c_str()) - p_stoi(expr->getExtra(1).c_str()) + 1;
  if(expr->getChild(0)->status == CONSTANT_PNODE) {
    expr->status = CONSTANT_PNODE;
  }
}

void visit2Expr(std::string prefix, Node* n, PNode* expr) { // add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
  Assert(expr->getChildNum() == 2, "Invalid childNum for expr %s\n", expr->name.c_str());
  n->ops.push_back(expr);
  visitExpr(prefix, n, expr->getChild(0));
  n->ops.push_back(NULL);
  visitExpr(prefix, n, expr->getChild(1));
  Assert(expr2Map.find(expr->name) != expr2Map.end(), "Operation %s not found\n", expr->name.c_str());
  std::tuple<bool, bool, int (*)(int, int, bool)>info = expr2Map[expr->name];
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  bool funcSign = std::get<1>(info) ? expr->sign : 0;
  expr->width = std::get<2>(info)(expr->getChild(0)->width, expr->getChild(1)->width, expr->getChild(0)->sign);
  if(expr->getChild(0)->status == CONSTANT_PNODE && expr->getChild(1)->status == CONSTANT_PNODE) {
    expr->status = CONSTANT_PNODE;
  }
}

void visit1Expr(std::string prefix, Node* n, PNode* expr) { // asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr
  n->ops.push_back(expr);
  std::tuple<uint8_t, int (*)(int, int, bool)>info = expr1Map[expr->name];
  visitExpr(prefix, n, expr->getChild(0));
  expr->sign = std::get<0>(info);
  expr->width = std::get<1>(info)(expr->getChild(0)->width, 0, expr->getChild(0)->sign);
  if(expr->getChild(0)->status == CONSTANT_PNODE) {
    expr->status = CONSTANT_PNODE;
  }
}

std::string visitReference(std::string prefix, PNode* expr) { // return ref name
  std::string ret;
  if(expr->getChildNum() == 0) {
    ret = prefix + expr->name;
    return ret;
  } else {
    ret = prefix + expr->name;
    for(int i = 0; i < expr->getChildNum(); i++) {
      PNode* child = expr->getChild(i);
      switch(child->type) {
        case P_REF_DOT:
          ret += "$" + visitReference("", child);
          break;
        default: Assert(0, "TODO: invalid ref child type(%d) for %s\n", child->type, expr->name.c_str());
      }
    }
    return ret;
  }
}

void visitMux(std::string prefix, Node* n, PNode* mux) {
  Assert(mux->getChildNum() == 3, "Invalid childNum(%d) for Mux\n", mux->getChildNum());
  n->ops.push_back(mux);
  visitExpr(prefix, n, mux->getChild(0));
  n->ops.push_back(NULL);
  visitExpr(prefix, n, mux->getChild(1));
  n->ops.push_back(NULL);
  visitExpr(prefix, n, mux->getChild(2));
  if (mux->getChild(0)->status == CONSTANT_NODE && mux->getChild(1)->status == CONSTANT_NODE &&
      mux->getChild(2)->status == CONSTANT_NODE) {
    mux->status = CONSTANT_NODE;
  }
  mux->getChild(1)->width = mux->getChild(2)->width = MAX(mux->getChild(1)->width, mux->getChild(2)->width);
  SET_TYPE(mux, mux->getChild(1));
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

void visitExpr(std::string prefix, Node* n, PNode* expr) { // return varName & update connect
  std::string ret;
  switch(expr->type) {
    case P_1EXPR1INT: visit1Expr1Int(prefix, n, expr); break;
    case P_2EXPR: visit2Expr(prefix, n, expr); break;
    case P_REF:
      ret = visitReference(prefix, expr);
      addEdge(ret, n);
      SET_TYPE(expr, allSignals[ret]);
      if(allSignals[ret]->status == CONSTANT_NODE) {
        expr->status = CONSTANT_NODE;
      }
      break;
    case P_EXPR_MUX: visitMux(prefix, n, expr); break;
    case P_EXPR_INT_INIT:
      n->ops.push_back(expr);
      expr->status = CONSTANT_NODE;
      break;
    case P_1EXPR: visit1Expr(prefix, n, expr); break;
    case P_1EXPR2INT: visit1Expr2Int(prefix, n, expr); break;
    default: std::cout << expr->type << std::endl; TODO();
  }
}

void visitNode(std::string prefix, PNode* node) { // generate new node and connect
  Node* newSig = new Node();
  newSig->name = prefix + node->name;
  Assert(node->getChildNum() >= 1, "Invalid childNum for node %s\n", node->name.c_str());
  visitExpr(prefix, newSig, node->getChild(0));
  SET_TYPE(newSig, node->getChild(0));
  addSignal(newSig->name, newSig);
  SET_TYPE(node, newSig);
}

void visitConnect(std::string prefix, PNode* connect) {
  Assert(connect->getChildNum() == 2, "Invalid childNum for connect %s\n", connect->name.c_str());
  std::string strDst = visitReference(prefix, connect->getChild(0));
  Node* dst = str2Node(strDst);
  if(dst->type == NODE_REG_SRC) dst = dst->regNext;
  visitExpr(prefix, dst, connect->getChild(1));
  SET_TYPE(connect->getChild(0), connect->getChild(1));
  if(connect->getChild(1)->status == CONSTANT_NODE) {
    connect->status = CONSTANT_NODE;
  }
}

void visitRegDef(std::string prefix, graph* g, PNode* reg) {
  Node* newReg = new Node(NODE_REG_SRC);
  newReg->name = prefix + reg->name;
  visitType(newReg, reg->getChild(0));
  Node* nextReg = new Node(NODE_REG_DST);
  nextReg->name = newReg->name + "$next";
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
  n->ops.push_back(print);
  visitExpr(prefix, n, print->getChild(1));
  PNode* exprs = print->getChild(2);
  for(int i = 0; i < exprs->getChildNum(); i++ ) {
    n->ops.push_back(NULL);
    visitExpr(prefix, n, exprs->getChild(i));
  }
  g->active.push_back(n);
}

void visitAssert(std::string prefix, graph* g, PNode* ass) {
  Node* n = new Node(NODE_ACTIVE);
  n->name = prefix + ass->name;
  n->ops.push_back(ass);
  visitExpr(prefix, n, ass->getChild(1));
  n->ops.push_back(NULL);
  visitExpr(prefix, n, ass->getChild(2));
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
  int width = n->width;
  if(n->width < 8) n->width = 8;
  else {
    n->width = p2(n->width);
  }
  Assert(n->width % 8 == 0, "invalid memory width %d for %s\n", n->width, n->name.c_str());
  int depth = p_stoi(memory->getChild(1)->name.c_str());
  int readLatency = p_stoi(memory->getChild(2)->name.c_str());
  int writeLatency = p_stoi(memory->getChild(3)->name.c_str());
  Assert(readLatency <= 1 && writeLatency <= 1, "Invalid readLatency(%d) or writeLatency(%d)\n", readLatency, writeLatency);
  n->latency[0] = readLatency;
  n->latency[1] = writeLatency;
  n->val = depth;
  Assert(memory->getChild(4)->name == "undefined", "Invalid ruw %s\n", memory->getChild(4)->name.c_str());
// readers
  for(int i = 5; i < memory->getChildNum(); i++) {
    PNode* rw = memory->getChild(i);
    Node* rn = new Node();
    rn->name = n->name + "$" + rw->name;
    n->member.push_back(rn);
    rn->regNext = n;
    memory_member(addr, rn, log2(depth));
    memory_member(en, rn, 1);
    memory_member(clk, rn, 1);
    if(rw->type == P_READER) {
      rn->type = NODE_READER;
      memory_member(data, rn, width);
      if(readLatency == 1) {
        rn->member[3]->type = NODE_L1_RDATA;
        g->memRdata1.push_back(rn->member[3]);
      }
    } else if(rw->type == P_WRITER) {
      rn->type = NODE_WRITER;
      memory_member(data, rn, width);
      memory_member(mask, rn, 1);
    } else if(rw->type == P_READWRITER) {
      rn->type = NODE_READWRITER;
      memory_member(rdata, rn, width);
      memory_member(wdata, rn, width);
      memory_member(wmask, rn, 1);
      memory_member(wmode, rn, 1);
      if(readLatency == 1) {
        rn->member[3]->type = NODE_L1_RDATA;
        g->memRdata1.push_back(rn->member[3]);
      }
    } else {
      Assert(0, "Invalid rw type %d\n", rw->type);
    }
  }
}

void visitWireDef(std::string prefix, graph* g, PNode* wire) {
  Node* newWire = new Node();
  newWire->name = prefix + wire->name;
  visitType(newWire, wire->getChild(0));
  addSignal(newWire->name, newWire);
}

void visitStmts(std::string prefix, graph* g, PNode* stmts) {
  PNode* module;
  for (int i = 0; i < stmts->getChildNum(); i++) {
    PNode* stmt = stmts->getChild(i);
    switch(stmt->type) {
      case P_INST : 
        Assert(stmt->getExtraNum() >= 1 && moduleMap.find(stmt->getExtra(0)) != moduleMap.end(), "Module %s is not defined!\n", stmt->name.c_str());
        module = moduleMap[stmt->getExtra(0)];
        if(module->type == P_MOD) visitModule(prefix + stmt->name + "$", g, module);
        else if(module->type == P_EXTMOD) visitExtModule(prefix + stmt->name + "$", g, module);
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
      case P_WIRE_DEF:
        visitWireDef(prefix, g, stmt);
        break;
      default: Assert(0, "Invalid stmt %s(%d)\n", stmt->name.c_str(), stmt->type);
    }
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
