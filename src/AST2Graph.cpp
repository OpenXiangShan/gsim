#include "common.h"
#include "PNode.h"
#include "Node.h"
#include "graph.h"
#include <map>
static std::map<std::string, PNode*> moduleMap;
static std::map<std::string, Node*> allSignals;
void visitStmts(std::string prefix, graph* g, PNode* stmts);

std::string visitExpr(std::string prefix, Node* n, PNode* expr);
void visitType(Node* n, PNode* ptype);

int p_stoi(const char* str);

void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  allSignals[s] = n;
}

void addEdge(Node* src, Node* dst) {
  dst->inEdge ++;
  src->next.push_back(dst);
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

void visitType(Node* n, PNode* ptype) {
  switch(ptype->type) {
    case P_Clock: n->width = 1; break;
    case P_INT_TYPE: n->width = 32; break;  //TODO: UInt & SInt
    default: TODO();
  }
}

std::string visit1Expr1Int(std::string prefix, Node* n, PNode* expr) { // pad|shl|shr|head|tail
  if(expr->name == "pad") TODO();
  else if(expr->name == "shl") TODO();
  else if(expr->name == "shr") TODO();
  else if(expr->name == "head") TODO();
  else if(expr->name == "tail") {
    // TODO: truncate
    return visitExpr(prefix, n, expr->getChild(0));
  } else {
    Assert(0, "Invalid E1I1OP %s\n", expr->name.c_str());
  }
}

std::string visit2Expr(std::string prefix, Node* n, PNode* expr) { // add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
  Assert(expr->getChildNum() == 2, "Invalid childNum for expr %s\n", expr->name.c_str());
  return "__" + expr->name + "(" + visitExpr(prefix, n, expr->getChild(0)) + ", " +  visitExpr(prefix, n, expr->getChild(1)) + ")";
}

std::string visitReference(std::string prefix, PNode* expr) { // return ref name
  std::string ret;
  if(expr->getChildNum() == 0) {
    ret = prefix + expr->name;
    // addEdge(ret, n);
    return ret;
  } else {
    PNode* child = expr->getChild(0);
    switch(child->type) {
      case P_REF_DOT: 
        ret = prefix + expr->name + "_" + visitReference("", child);
        // addEdge(ret, n);
        return ret;
      default: Assert(0, "TODO: invalid ref child type for %s\n", expr->name.c_str());
    }
  }
  Assert(expr->getChildNum() == 0, "TODO: expr %s with childNum %d\n", expr->name, expr->getChildNum());
}

std::string visitMux(std::string prefix, Node* n, PNode* mux) {
  Assert(mux->getChildNum() == 3, "Invalid childNum for Mux\n");
  return "(" + visitExpr(prefix, n, mux->getChild(0)) + "? " + visitExpr(prefix, n, mux->getChild(1)) + " : " + visitExpr(prefix, n, mux->getChild(2)) + ")";
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

std::string visitExpr(std::string prefix, Node* n, PNode* expr) { // return op & update connect
  std::string ret;
  switch(expr->type) {
    case P_1EXPR1INT: return visit1Expr1Int(prefix, n, expr);
    case P_2EXPR: return visit2Expr(prefix, n, expr);
    case P_REF: ret = visitReference(prefix, expr); addEdge(ret, n); return ret;
    case P_EXPR_MUX: return visitMux(prefix, n, expr);
    case P_EXPR_INT_INIT: return cons2str(expr->getExtra(0).substr(1, expr->getExtra(0).length()-2));
    default: TODO();
  }

}

Node* visitNode(std::string prefix, PNode* node) { // generate new node and connect
  Node* newSig = new Node();
  newSig->name = prefix + node->name;
  addSignal(newSig->name, newSig);
  Assert(node->getChildNum() >= 1, "Invalid childNum for node %s\n", node->name.c_str());
  newSig->op = visitExpr(prefix, newSig, node->getChild(0));
  return newSig;
}

void visitConnect(std::string prefix, PNode* connect) {
  Assert(connect->getChildNum() == 2, "Invalid childNum for connect %s\n", connect->name.c_str());
  std::string strDst = visitReference(prefix, connect->getChild(0));
  Node* dst = str2Node(strDst);
  if(dst->type == NODE_REG_SRC) dst = dst->regNext;
  dst->op = visitExpr(prefix, dst, connect->getChild(1));
}

void visitRegDef(std::string prefix, graph* g, PNode* reg) {
  Node* newReg = new Node();
  newReg->name = prefix + reg->name;
  newReg->type = NODE_REG_SRC;
  Node* nextReg = new Node();
  nextReg->name = newReg->name + "_next";
  nextReg->type = NODE_REG_DST;
  newReg->regNext = nextReg;
  nextReg->regNext = newReg;
  addSignal(newReg->name, newReg);
  addSignal(nextReg->name, nextReg);
  g->sources.push_back(newReg);
}

void visitStmts(std::string prefix, graph* g, PNode* stmts) {
  for (int i = 0; i < stmts->getChildNum(); i++) {
    PNode* stmt = stmts->getChild(i);
    switch(stmt->type) {
      case P_INST : 
        Assert(stmt->getExtraNum() >= 1 && moduleMap.find(stmt->getExtra(0)) != moduleMap.end(), "Module %s is not defined!\n", stmt->name.c_str());
        visitModule(prefix + stmt->name + "_", g, moduleMap[stmt->getExtra(0)]);
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
      default: Assert(0, "Invalid stmt %s\n", stmt->name.c_str());
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
    } else if(port->type == P_OUTPUT) {
      g->output.push_back(io);
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
  return g;
}
