/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/


#include "common.h"
#include <stack>
#include <map>
#include <utility>

/* check the node type and children num */
#define TYPE_CHECK(node, min, max,...) typeCheck(node, (const int[]){__VA_ARGS__}, sizeof((const int[]){__VA_ARGS__}) / sizeof(int), min, max)
#define SEP_MODULE "$" // seperator for module
#define SEP_AGGR "$$"

#define MOD_IN(module) ((module == P_EXTMOD) ? NODE_EXT_IN : (module == P_MOD ? NODE_OTHERS : NODE_INP))
#define MOD_OUT(module) ((module == P_EXTMOD) ? NODE_EXT_OUT : (module == P_MOD ? NODE_OTHERS : NODE_OUT))
#define MOD_EXT_FLIP(type) (type == NODE_INP ? NODE_OUT : \
                           (type == NODE_OUT ? NODE_INP : \
                           (type == NODE_EXT_IN ? NODE_EXT_OUT : \
                           (type == NODE_EXT_OUT ? NODE_EXT_IN : type))))

/* module1, module1$module2 module1$a_b_c...*/
class ModulePath {
private:
  std::stack<std::string> path;

public:
  std::string cwd() {
    Assert(!path.empty(), "path is empty");
    return path.top();
  }
  std::string subDir(const char *sep, std::string name) {
    return (path.empty() ? "" : cwd() + sep) + name;
  }
  void cd(const char *sep, std::string str) {
    path.push(subDir(sep, str));
  }
  void cdLast() { path.pop(); }
};

typedef struct Context {
  graph *g;
  ModulePath *path;

  void visitModule(PNode* module);
  void visitTopModule(PNode* topModule);
  void visitExtModule(PNode* module);
  void visitTopPorts(PNode* ports);
  TypeInfo* visitPort(PNode* port, PNodeType modType);
  TypeInfo* visitType(PNode* ptype, NodeType parentType);
  TypeInfo* visitMemType(PNode* dataType);
  TypeInfo* visitFields(PNode* fields, NodeType parentType);
  TypeInfo* visitField(PNode* field, NodeType parentType);
  void visitStmts(PNode* stmts);
  void visitStmt(PNode* stmt);
  void visitWireDef(PNode* wire);
  void visitRegDef(PNode* reg, PNodeType t);
  void visitInst(PNode* inst);
  void visitWhenStmts(PNode* stmts);
  void visitWhenStmt(PNode* stmt);
  void visitWhen(PNode* when);
  void visitMemory(PNode* mem);
  void visitNode(PNode* node);
  void visitConnect(PNode* connect);
  void visitPartialConnect(PNode* connect);
  void visitWhenConnect(PNode* connect);
  void visitWhenPrintf(PNode* print);
  void visitWhenAssert(PNode* ass);
  void visitWhenStop(PNode* stop);
  void visitPrintf(PNode* print);
  void visitStop(PNode* stop);
  void visitAssert(PNode* ass);
  AggrParentNode* allocNodeFromAggr(AggrParentNode* parent);
  Node* allocCondNode(ASTExpTree* condExp, PNode* when);
  ASTExpTree* visitIntNoInit(PNode* expr);
  ASTExpTree* visitInvalid(PNode* expr);
  ASTExpTree* visitIntInit(PNode* expr);
  ASTExpTree* allocIndex(PNode* expr);
  ASTExpTree* visitReference(PNode* expr);
  ASTExpTree* visit2Expr(PNode* expr);
  ASTExpTree* visit1Expr(PNode* expr);
  ASTExpTree* visit1Expr1Int(PNode* expr);
  ASTExpTree* visit1Expr2Int(PNode* expr);
  ASTExpTree* visitExpr(PNode* expr);
  ASTExpTree* visitMux(PNode* expr);
  Node* visitMemPort(PNode* port, int depth, int width, bool sign, std::string suffix, Node* node);
  Node* visitChirrtlPort(PNode* port, int width, int depth, bool sign, std::string suffix, Node* node, ENode* addr_enode, Node* clock_node);
  void visitChirrtlMemPort(PNode* port);
  void visitChirrtlMemory(PNode* mem);
  Node* add_member(Node* parent, std::string name, int idx, int width, int sign);
  Node* visitReader(PNode* reader, int width, int depth, bool sign, std::string suffix, Node* node);
  Node* visitWriter(PNode* writer, int width, int depth, bool sign, std::string suffix, Node* node);
  Node* visitReadWriter(PNode* readWriter, int width, int depth, bool sign, std::string suffix, Node* node);
  void whenConnect(Node* node, ENode* lvalue, ENode* rvalue, PNode* connect);
} Context;

int p_stoi(const char* str);
void removeDummyDim(graph* g);
ENode* getWhenEnode(ExpTree* valTree, int depth);
void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

/* map between module name and module pnode*/
static std::map<std::string, PNode*> *moduleMap;
static std::map<std::string, Node*> allSignals;
static std::map<std::string, AggrParentNode*> allDummy; // CHECK: any other dummy nodes ?
static std::vector<std::pair<bool, Node*>> whenTrace;
static std::set<std::string> moduleInstances;

static std::set<Node*> stmtsNodes;

static std::map<std::string, std::pair<std::vector<Node*>, std::vector<AggrParentNode*>>> memoryMap;

static inline void typeCheck(PNode* node, const int expect[], int size, int minChildNum, int maxChildNum) {
  const int* expectEnd = expect + size;
  Assert((size == 0) || (std::find(expect, expectEnd, node->type) != expectEnd),
    "The type of node %s should in {%d...}, got %d\n", node->name.c_str(), expect[0], node->type);
  Assert(node->getChildNum() >= minChildNum && node->getChildNum() <= maxChildNum,
    "the childNum %d of node %s is out of bound [%d, %d]", node->getChildNum(), node->name.c_str(), minChildNum, maxChildNum);
}

static inline Node* allocNode(NodeType type = NODE_OTHERS, std::string name = "", int lineno = -1) {
  Node* node = new Node(type);
  node->name = name;
  node->lineno = lineno;
  return node;
}

static inline void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  Assert(allDummy.find(s) == allDummy.end(), "Signal %s is already in allDummy\n", s.c_str());
  allSignals[s] = n;
  n->whenDepth = whenTrace.size();
  // printf("add signal %s\n", s.c_str());
}

static inline Node* getSignal(std::string s) {
  Assert(allSignals.find(s) != allSignals.end(), "Signal %s is not in allSignals\n", s.c_str());
  return allSignals[s];  
}

static inline void addDummy(std::string s, AggrParentNode* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  Assert(allDummy.find(s) == allDummy.end(), "Node %s is already in allDummy\n", s.c_str());
  allDummy[s] = n;
  // printf("add dummy %s\n", s.c_str());
}

static inline AggrParentNode* getDummy(std::string s) {
  Assert(allDummy.find(s) != allDummy.end(), "Node %s is not in allDummy\n", s.c_str());
  return allDummy[s];
}

static inline bool isAggr(std::string s) {
  if (allDummy.find(s) != allDummy.end()) return true;
  if (allSignals.find(s) != allSignals.end()) return false;
  Assert(0, "%s is not added\n", s.c_str());
}

static inline std::string replacePrefix(std::string oldPrefix, std::string newPrefix, std::string str) {
  Assert(str.compare(0, oldPrefix.length(), oldPrefix) == 0, "member name %s does not start with %s", str.c_str(), oldPrefix.c_str());
  return newPrefix + str.substr(oldPrefix.length());
}
/* return the trailing string of the last $ */
static inline std::string lastField(std::string s) {
  size_t pos = s.find_last_of('$');
  return pos == std::string::npos ? "" : s.substr(pos + 1);
}

int countEmptyENodeWhen(ENode* enode) {
  int ret = 0;
  std::stack<ENode*> s;
  s.push(enode);
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->opType == OP_STMT) { /* push the first child */
      for (ENode* childENode : top->child) {
        if (childENode) {
          s.push(childENode);
          break;
        }
      }
    } else {
      for (ENode* childENode : top->child) {
        if (childENode) s.push(childENode);
      }
    }
    if (top->opType == OP_WHEN) {
      if (!top->getChild(1)) ret ++;
      if (!top->getChild(2)) ret ++;
    }
  }
  return ret;
}

int countEmptyWhen(ExpTree* tree) {
  return countEmptyENodeWhen(tree->getRoot());
}

/*
field: ALLID ':' type { $$ = newNode(P_FIELD, synlineno(), $1, 1, $3); }
    | Flip ALLID ':' type  { $$ = newNode(P_FLIP_FIELD, synlineno(), $2, 1, $4); }
@return: node list in this field
*/
TypeInfo* Context::visitField(PNode* field, NodeType parentType) {
  NodeType fieldType = parentType;
  if (field->type == P_FLIP_FIELD) fieldType = MOD_EXT_FLIP(fieldType);
  path->cd(SEP_AGGR, field->name);
  TypeInfo* info = visitType(field->getChild(0), fieldType);
  if (field->type == P_FLIP_FIELD) info->flip();
  path->cdLast();
  return info;
  
}

/*
fields:                 { $$ = new PList(); }
    | fields ',' field  { $$ = $1; $$->append($3); }
    | field             { $$ = new PList($1); }
*/
TypeInfo* Context::visitFields(PNode* fields, NodeType parentType) {
  TypeInfo* info = new TypeInfo();
  for (int i = 0; i < fields->getChildNum(); i ++) {
    PNode* field = fields->getChild(i);
    TypeInfo* fieldInfo = visitField(field, parentType);
    NodeType curType = parentType;
    if (field->type == P_FLIP_FIELD) {
      curType = MOD_EXT_FLIP(parentType);
    }
    if (!fieldInfo->isAggr()) { // The type of field is ground
      Node* fieldNode = allocNode(curType, path->subDir(SEP_AGGR, field->name), fields->lineno);
      fieldNode->updateInfo(fieldInfo);
      info->add(fieldNode, field->type == P_FLIP_FIELD);
    } else { // The type of field is aggregate
      info->mergeInto(fieldInfo);
    }
    delete fieldInfo;
  }
  return info;
}

/*
type_aggregate: '{' fields '}'  { $$ = new PNode(P_AG_FIELDS, synlineno()); $$->appendChildList($2); }
    | type '[' INT ']'          { $$ = newNode(P_AG_ARRAY, synlineno(), emptyStr, 1, $1); $$->appendExtraInfo($3); }
type_ground: Clock    { $$ = new PNode(P_Clock, synlineno()); }
    | IntType width   { $$ = newNode(P_INT_TYPE, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); }
    | anaType width   { TODO(); }
    | FixedType width binary_point  { TODO(); }
update width/sign/dimension/aggrtype
*/
TypeInfo* Context::visitType(PNode* ptype, NodeType parentType) {
  TYPE_CHECK(ptype, 0, INT32_MAX, P_AG_FIELDS, P_AG_ARRAY, P_Clock, P_INT_TYPE, P_ASYRESET);
  TypeInfo* info = NULL;
  switch (ptype->type) {
    case P_Clock:
      info = new TypeInfo();
      info->set_sign(false); info->set_width(1);
      info->set_clock(true); info->set_reset(UINTRESET);
      break;
    case P_INT_TYPE:
      info = new TypeInfo();
      info->set_sign(ptype->sign); info->set_width(ptype->width);
      info->set_reset(UINTRESET);
      break;
    case P_ASYRESET:
      info = new TypeInfo();
      info->set_sign(false); info->set_width(1);
      info->set_reset(ASYRESET);
      break;
    case P_AG_FIELDS:
      info = visitFields(ptype, parentType);
      info->newParent(path->cwd());
      break;
    case P_AG_ARRAY:
      TYPE_CHECK(ptype, 1, 1, P_AG_ARRAY);
      info = visitType(ptype->getChild(0), parentType);
      info->addDim(p_stoi(ptype->getExtra(0).c_str()));
      break;
    default:
      Panic();
  }
  Assert(info, "info should not be empty");
  return info;
}

/*
port: Input ALLID ':' type info    { $$ = newNode(P_INPUT, synlineno(), $5, $2, 1, $4); }
    | Output ALLID ':' type info   { $$ = newNode(P_OUTPUT, synlineno(), $5, $2, 1, $4); }
all nodes are stored in aggrMember
*/
TypeInfo* Context::visitPort(PNode* port, PNodeType modType) {
  TYPE_CHECK(port, 1, 1, P_INPUT, P_OUTPUT);
  NodeType type = port->type == P_INPUT ? MOD_IN(modType) : MOD_OUT(modType);
  path->cd(SEP_MODULE, port->name);
  TypeInfo* info = visitType(port->getChild(0), type);

  if (!info->isAggr()) {
    Node* node = allocNode(type, path->cwd(), port->lineno);
    node->updateInfo(info);
    info->add(node, false);
  }
  path->cdLast();
  return info;
}

/*
  ports:  { $$ = new PNode(P_PORTS); }
      | ports port    { $$ = $1; $$->appendChild($2); }
*/
void Context::visitTopPorts(PNode* ports) {
  TYPE_CHECK(ports, 0, INT32_MAX, P_PORTS);
  // visit all ports
  for (int i = 0; i < ports->getChildNum(); i ++) {
    PNode* port = ports->getChild(i);
    TypeInfo* info = visitPort(port, P_CIRCUIT);
    for (auto entry : info->aggrMember) {
      Node* node = entry.first;
      addSignal(node->name, node);
      if (node->type == NODE_INP) g->input.push_back(node);
      else if (node->type == NODE_OUT) g->output.push_back(node);
    }
    for (AggrParentNode* dummy : info->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }
}
/*
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S');}
*/
ASTExpTree* Context::visitIntNoInit(PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_EXPR_INT_NOINIT);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->getExpRoot()->strVal = "0";
  ret->setType(expr->width, expr->sign);
  ret->setOp(OP_INT);
  return ret;
}
/*
  reference Is Invalid info
*/
ASTExpTree* Context::visitInvalid(PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_INVALID);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(OP_INVALID);
  return ret;
}
/*
| IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
*/
ASTExpTree* Context::visitIntInit(PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_EXPR_INT_INIT);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->getExpRoot()->strVal = expr->getExtra(0);
  ret->setType(expr->width, expr->sign);
  ret->setOp(OP_INT);
  return ret;
}

ENode* allocIntIndex(std::string intStr) {
  ENode* index = new ENode(OP_INDEX_INT);
  index->addVal(p_stoi(intStr.c_str()));
  return index;
}
/*
  OP_INDEX
      \
      exprTree
*/
ASTExpTree* Context::allocIndex(PNode* expr) {
  ASTExpTree* exprTree = visitExpr(expr);
  if (exprTree->getExpRoot()->opType == OP_INT) {
    ENode* index = new ENode(OP_INDEX_INT);
    index->addVal(p_stoi(exprTree->getExpRoot()->strVal.c_str()));
    exprTree->setRoot(index);
  } else {
    ENode* index = new ENode(OP_INDEX);
    exprTree->updateRoot(index);
  }
  return exprTree;
}

/*
    reference: ALLID  { $$ = newNode(P_REF, synlineno(), $1, 0); }
    | reference '.' ALLID  { $$ = $1; $1->appendChild(newNode(P_REF_DOT, synlineno(), $3, 0)); }
    | reference '[' INT ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_INT, synlineno(), $3, 0)); }
    | reference '[' expr ']' { $$ = $1; $1->appendChild(newNode(P_REF_IDX_EXPR, synlineno(), NULL, 1, $3)); }
*/
ASTExpTree* Context::visitReference(PNode* expr) {
  TYPE_CHECK(expr, 0, INT32_MAX, P_REF);
  std::string name = path->subDir(SEP_MODULE, expr->name);

  for (int i = 0; i < expr->getChildNum(); i ++) {
    if (expr->getChild(i)->type == P_REF_DOT) {
      name += (moduleInstances.find(name) != moduleInstances.end() ? SEP_MODULE : SEP_AGGR) + expr->getChild(i)->name;
    }
  }
  ASTExpTree* ret = nullptr;

  if (isAggr(name)) { // add all signals and their names into aggr
    AggrParentNode* parent = getDummy(name);
    ret = new ASTExpTree(true, parent->size());
    ret->setAnyParent(parent);
    // point the root of aggrTree to parent member
    // the width of node may not be determined yet
    for (int i = 0; i < parent->size(); i++) {
      ret->getAggr(i)->setNode(parent->member[i].first);
      ret->setFlip(i, parent->member[i].second);
    }
  } else {
    ret = new ASTExpTree(false);
    ret->getExpRoot()->setNode(getSignal(name));
  }
  // update dimensions
  for (int i = 0; i < expr->getChildNum(); i ++) {
    PNode* child = expr->getChild(i);
    switch(child->type) {
      case P_REF_IDX_EXPR: ret->addChildSameTree(allocIndex(child->getChild(0))); break;
      case P_REF_IDX_INT: ret->addChild(allocIntIndex(child->name.c_str())); break;
      case P_REF_DOT: break;
      default: Panic();
    }
  }
  return ret;
}
/*
| Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, synlineno(), NULL, 3, $3, $5, $7); }
*/
ASTExpTree* Context::visitMux(PNode* expr) {
  TYPE_CHECK(expr, 3, 3, P_EXPR_MUX);

  ASTExpTree* cond  = visitExpr(expr->getChild(0));
  ASTExpTree* left  = visitExpr(expr->getChild(1));
  ASTExpTree* right = visitExpr(expr->getChild(2));

  ASTExpTree* ret = left->dupEmpty();
  ret->setOp(OP_MUX);
  ret->addChildSameTree(cond);
  ret->addChildTree(2, left, right);

  delete cond;
  delete left;
  delete right;

  return ret;
}
/*
primop_2expr: E2OP expr ',' expr ')' { $$ = newNode(P_2EXPR, synlineno(), $1, 2, $2, $4); }
*/
ASTExpTree* Context::visit2Expr(PNode* expr) {
  TYPE_CHECK(expr, 2, 2, P_2EXPR);

  ASTExpTree* left  = visitExpr(expr->getChild(0));
  ASTExpTree* right = visitExpr(expr->getChild(1));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr2(expr->name));
  ret->addChildTree(2, left, right);

  delete left;
  delete right;

  return ret;
}
/*
primop_1expr: E1OP expr ')' { $$ = newNode(P_1EXPR, synlineno(), $1, 1, $2); }
*/
ASTExpTree* Context::visit1Expr(PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR);

  ASTExpTree* child = visitExpr(expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr1(expr->name));
  ret->addChildTree(1, child);

  delete child;
  return ret;
}
/*
primop_1expr1int: E1I1OP expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); }
*/
ASTExpTree* Context::visit1Expr1Int(PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR1INT);

  ASTExpTree* child = visitExpr(expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr1int1(expr->name));
  ret->addChildTree(1, child);
  ret->addVal(p_stoi(expr->getExtra(0).c_str()));

  delete child;
  return ret;
}
/*
primop_1expr2int: E1I2OP expr ',' INT ',' INT ')' { $$ = newNode(P_1EXPR2INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); $$->appendExtraInfo($6); }
*/
ASTExpTree* Context::visit1Expr2Int(PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR2INT);

  ASTExpTree* child = visitExpr(expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(OP_BITS);
  ret->addChildTree(1, child);
  ret->addVal(p_stoi(expr->getExtra(0).c_str()));
  ret->addVal(p_stoi(expr->getExtra(1).c_str()));

  delete child;
  return ret;
}


/*
expr: IntType width '(' ')'     { $$ = newNode(P_EXPR_INT_NOINIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S');}
    | IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
    | reference { $$ = $1; }
    | Mux '(' expr ',' expr ',' expr ')' { $$ = newNode(P_EXPR_MUX, synlineno(), NULL, 3, $3, $5, $7); }
    | Validif '(' expr ',' expr ')' { $$ = $5; }
    | primop_2expr  { $$ = $1; }
    | primop_1expr  { $$ = $1; }
    | primop_1expr1int  { $$ = $1; }
    | primop_1expr2int  { $$ = $1; }
    ;
*/
ASTExpTree* Context::visitExpr(PNode* expr) {
  ASTExpTree* ret;
  switch (expr->type) {
    case P_EXPR_INT_NOINIT: ret = visitIntNoInit(expr); break;
    case P_EXPR_INT_INIT: ret = visitIntInit(expr); break;
    case P_REF: ret = visitReference(expr); break;
    case P_EXPR_MUX: ret = visitMux(expr); break;
    case P_2EXPR: ret = visit2Expr(expr); break;
    case P_1EXPR: ret = visit1Expr(expr); break;
    case P_1EXPR1INT: ret = visit1Expr1Int(expr); break;
    case P_1EXPR2INT: ret = visit1Expr2Int(expr); break;
    case P_INVALID: ret = visitInvalid(expr); break;
    default: printf("Invalid type %d\n", expr->type); Panic();
  }
  return ret;
}
/*
module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
*/
void Context::visitModule(PNode* module) {
  TYPE_CHECK(module, 2, 2, P_MOD);
  // printf("visit module %s\n", module->name.c_str());

  PNode* ports = module->getChild(0);
  for (int i = 0; i < ports->getChildNum(); i ++) {
    TypeInfo* portInfo = visitPort(ports->getChild(i), P_MOD);

    for (auto entry : portInfo->aggrMember) {
      Node* node = entry.first;
      addSignal(node->name, node);
    }
    for (AggrParentNode* dummy : portInfo->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }

  visitStmts(module->getChild(1));
  // printf("leave module %s\n", module->name.c_str());
}
/*
extmodule: Extmodule ALLID ':' info INDENT ports ext_defname params DEDENT  { $$ = newNode(P_EXTMOD, synlineno(), $4, $2, 1, $6); $$->appendChildList($8);}
*/
void Context::visitExtModule(PNode* module) {
  TYPE_CHECK(module, 1, 1, P_EXTMOD);

  Node* extNode = allocNode(NODE_EXT, module->name, module->lineno);
  extNode->extraInfo = module->getExtra(0);
  addSignal(extNode->name, extNode);
  PNode* ports = module->getChild(0);
  for (int i = 0; i < ports->getChildNum(); i ++) {
    TypeInfo* portInfo = visitPort(ports->getChild(i), P_EXTMOD);
    for (auto entry : portInfo->aggrMember) {
      if (entry.first->isClock) {
        if (!extNode->clock) extNode->clock = entry.first;
        else entry.first->type = NODE_OTHERS;
      } else extNode->add_member(entry.first);
      addSignal(entry.first->name, entry.first);
    }
    for (AggrParentNode* dummy : portInfo->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }
  /* construct valTree for every output and NODE_EXT */
  /* in -> ext */
  ENode* funcENode = new ENode(OP_EXT_FUNC);
  for (Node* node : extNode->member) {
    if (node->type == NODE_EXT_OUT) continue;
    ENode* inENode = new ENode(node);
    funcENode->addChild(inENode);
  }
  extNode->valTree = new ExpTree(funcENode, extNode);
  /* ext -> out */
  for (Node* node : extNode->member) {
    if (node->type == NODE_EXT_IN) continue;
    ENode* outFuncENode = new ENode(OP_EXT_FUNC);
    outFuncENode->addChild(new ENode(extNode));
    node->valTree = new ExpTree(outFuncENode, node);
  }
}

/*
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
*/
void Context::visitWireDef(PNode* wire) {
  TYPE_CHECK(wire, 1, 1, P_WIRE_DEF);

  path->cd(SEP_MODULE, wire->name);

  TypeInfo* info = visitType(wire->getChild(0), NODE_OTHERS);

  for (auto entry : info->aggrMember) {
    addSignal(entry.first->name, entry.first);
  }
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  if (!info->isAggr()) {
    Node* node = allocNode(NODE_OTHERS, path->cwd(), wire->lineno);
    node->updateInfo(info);
    addSignal(node->name, node);
  }

  path->cdLast();
}

/*
statement: Reg ALLID ':' type ',' expr(1) RegWith INDENT RegReset '(' expr ',' expr ')' info DEDENT { $$ = newNode(P_REG_DEF, $4->lineno, $15, $2, 4, $4, $6, $11, $13); }
expr(1) must be clock
*/
void Context::visitRegDef(PNode* reg, PNodeType t) {
  ASTExpTree* clkExp = visitExpr(reg->getChild(1));
  Assert(!clkExp->isAggr() && clkExp->getExpRoot()->getNode(), "unsupported clock in lineno %d", reg->lineno);
  Node* clockNode = clkExp->getExpRoot()->getNode();

  path->cd(SEP_MODULE, reg->name);
  TypeInfo* info = visitType(reg->getChild(0), NODE_REG_SRC);
  /* alloc node for basic nodes */
  if (!info->isAggr()) {
    Node* src = allocNode(NODE_REG_SRC, path->cwd(), reg->lineno);
    src->updateInfo(info);
    info->add(src, false);
  }
  // add reg_src and red_dst to all signals
  for (auto entry : info->aggrMember) {
    Node* src = entry.first;
    g->addReg(src);
    Node* dst = src->dup();
    dst->type = NODE_REG_DST;
    dst->name += "$NEXT";
    addSignal(src->name, src);
    addSignal(dst->name, dst);
    src->bindReg(dst);
    src->clock = dst->clock = clockNode;

    if (t == P_REG_RESET_DEF) continue;

    // Fake reset
    auto* cond = new ENode(OP_INT);
    cond->width = 1;
    cond->strVal = "0";

    src->resetCond = new ExpTree(cond, src);
    src->resetVal = new ExpTree(new ENode(src), src);
  }
  // only src dummy nodes are in allDummy
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  
  path->cdLast();

  if (t == P_REG_DEF)
    return ;
  
  ASTExpTree* resetCond = visitExpr(reg->getChild(2));
  ASTExpTree* resetVal  = visitExpr(reg->getChild(3));
  Assert(!resetCond->isAggr(), "reg %s: reset cond can never be aggregate\n", reg->name.c_str());
  // all aggregate nodes share the same resetCond ExpRoot
  for (size_t i = 0; i < info->aggrMember.size(); i ++) {
    Node* src = info->aggrMember[i].first;
    src->resetCond = new ExpTree(resetCond->getExpRoot(), src);
    if (info->isAggr())
      src->resetVal = new ExpTree(resetVal->getAggr(i), src);
    else
      src->resetVal = new ExpTree(resetVal->getExpRoot(), src);
  }
}

/*
mem_datatype: DataType "=>" type { $$ = newNode(P_DATATYPE, synlineno(), NULL, 1, $3); }
*/
TypeInfo* Context::visitMemType(PNode* dataType) {
  TYPE_CHECK(dataType, 1, 1, P_DATATYPE);
  return visitType(dataType->getChild(0), NODE_INVALID);
}
/*
mem_depth: Depth "=>" INT   { $$ = newNode(P_DEPTH, synlineno(), $3, 0); }
*/
static inline int visitMemDepth(PNode* depth) {
  TYPE_CHECK(depth, 0, 0, P_DEPTH);
  return p_stoi(depth->name.c_str());
}
/*
mem_rlatency: ReadLatency "=>" INT  { $$ = newNode(P_RLATENCT, synlineno(), $3, 0); }
*/
static inline int visitReadLatency(PNode* latency) {
  TYPE_CHECK(latency, 0, 0, P_RLATENCT);
  return p_stoi(latency->name.c_str());
}
/*
mem_wlatency: WriteLatency "=>" INT { $$ = newNode(P_WLATENCT, synlineno(), $3, 0); }
*/
static inline int visitWriteLatency(PNode* latency) {
  TYPE_CHECK(latency, 0, 0, P_WLATENCT);
  return p_stoi(latency->name.c_str());
}
/*
mem_ruw: ReadUnderwrite "=>" Ruw { $$ = newNode(P_RUW, synlineno(), $3, 0); }
*/
static inline void visitRUW(PNode* ruw) {
  TYPE_CHECK(ruw, 0, 0, P_RUW);
  Assert(ruw->name == "undefined", "IMPLEMENT ME");
}

Node* Context::add_member(Node* parent, std::string name, int idx, int width, int sign) {
  Node* member = allocNode(NODE_MEM_MEMBER, path->subDir(SEP_AGGR, name));
  member->setType(width, sign);
  parent->set_member(idx, member);
  return member;
}
/*
mem_reader Reader "=>" ALLID  { $$ = $1; $$->append(newNode(P_READER, synlineno(), $4, 0));}
*/
Node* Context::visitReader(PNode* reader, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(reader, 0, 0, P_READER);

  path->cd(SEP_MODULE, reader->name);

  Node* ret = allocNode(NODE_READER, path->cwd(), reader->lineno);

  for (int i = 0; i < READER_MEMBER_NUM; i ++) { // resize member vector
    ret->add_member(nullptr);
  }

  add_member(ret, "addr" + suffix, READER_ADDR, upperLog2(depth), false);
  add_member(ret, "en" + suffix, READER_EN, 1, false);
  Node* clk = add_member(ret, "clk" + suffix, READER_CLK, 1, false);
  clk->isClock = true;
  Node* data = add_member(ret, "data" + suffix, READER_DATA, width, sign);
  if (node) {
    data->dimension.insert(data->dimension.end(), node->dimension.begin(), node->dimension.end());
  }

  path->cdLast();

  return ret;
}
/*
mem_writer Writer "=>" ALLID    { $$ = $1; $$->append(newNode(P_WRITER, synlineno(), $4, 0));}
*/
Node* Context::visitWriter(PNode* writer, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(writer, 0, 0, P_WRITER);

  path->cd(SEP_MODULE, writer->name);

  Node* ret = allocNode(NODE_WRITER, path->cwd(), writer->lineno);

  for (int i = 0; i < WRITER_MEMBER_NUM; i ++) {
    ret->add_member(nullptr);
  }

  add_member(ret, "addr" + suffix, WRITER_ADDR, upperLog2(depth), false);
  add_member(ret, "en" + suffix, WRITER_EN, 1, false);
  Node* clk = add_member(ret, "clk" + suffix, WRITER_CLK, 1, false);
  clk->isClock = true;
  Node* data = add_member(ret, "data" + suffix, WRITER_DATA, width, sign);
  Node* mask = add_member(ret, "mask" + suffix, WRITER_MASK, 1, false);
  if (node) {
    data->dimension.insert(data->dimension.end(), node->dimension.begin(), node->dimension.end());
    mask->dimension.insert(mask->dimension.end(), node->dimension.begin(), node->dimension.end());
  }

  path->cdLast();

  return ret;
}
/*
mem_readwriter Readwriter "=>" ALLID  { $$ = $1; $$->append(newNode(P_READWRITER, synlineno(), $4, 0));}
*/
Node* Context::visitReadWriter(PNode* readWriter, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(readWriter, 0, 0, P_READWRITER);

  path->cd(SEP_MODULE, readWriter->name);

  Node* ret = allocNode(NODE_READWRITER, path->cwd(), readWriter->lineno);

  for (int i = 0; i < READWRITER_MEMBER_NUM; i ++) {
    ret->add_member(nullptr);
  }

  add_member(ret, "addr" + suffix, READWRITER_ADDR, upperLog2(depth), false);
  add_member(ret, "en" + suffix, READWRITER_EN, 1, false);
  Node* clk = add_member(ret, "clk" + suffix, READWRITER_CLK, 1, false);
  clk->isClock = true;
  Node* rdata = add_member(ret, "rdata" + suffix, READWRITER_RDATA, width, sign);
  Node* wdata = add_member(ret, "wdata" + suffix, READWRITER_WDATA, width, sign);
  Node* wmask = add_member(ret, "wmask" + suffix, READWRITER_WMASK, 1, false);
  add_member(ret, "wmode" + suffix, READWRITER_WMODE, 1, false);
  if (node) {
    rdata->dimension.insert(rdata->dimension.end(), node->dimension.begin(), node->dimension.end());
    wdata->dimension.insert(wdata->dimension.end(), node->dimension.begin(), node->dimension.end());
    wmask->dimension.insert(wmask->dimension.end(), node->dimension.begin(), node->dimension.end());
  }

  path->cdLast();

  return ret;
}

Node* Context::visitMemPort(PNode* port, int depth, int width, bool sign, std::string suffix, Node* node) {
  Node* portNode = nullptr;
  switch(port->type) {
    case P_READER: portNode = visitReader(port, width, depth, sign, suffix, node); break;
    case P_WRITER: portNode = visitWriter(port, width, depth, sign, suffix, node); break;
    case P_READWRITER: portNode = visitReadWriter(port, width, depth, sign, suffix, node); break;
    default: Panic();
  }
  return portNode;
}

void memoryAlias(Node* origin, Node* alias) {
  Assert(origin->type == alias->type, "type not match %s(%d) %s(%d)", origin->name.c_str(), origin->type, alias->name.c_str(), alias->type);
  Assert(origin->member.size() == alias->member.size(), "member not match");

  for(size_t i = 0; i < origin->member.size(); i ++) {
    if (alias->type == NODE_READER && i == READER_DATA) continue;
    if (alias->type == NODE_WRITER && i == WRITER_DATA) continue;
    if (alias->type == NODE_WRITER && i == WRITER_MASK) continue;
    if (alias->type == NODE_READWRITER && i == READWRITER_RDATA) continue;
    if (alias->type == NODE_READWRITER && i == READWRITER_WDATA) continue;
    if (alias->type == NODE_READWRITER && i == READWRITER_WMASK) continue;
    alias->member[i]->valTree = new ExpTree(new ENode(origin->member[i]), alias->member[i]);
  }
}

void addOriginMember(Node* originPort, PNode* port) {
  for (Node* member : originPort->member) member->type = NODE_OTHERS;
  Node* originMember;
  switch(port->type) {
    case P_READER:
      originMember = originPort->get_member(READER_ADDR);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(READER_EN);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(READER_CLK);
      addSignal(originMember->name, originMember);
      break;
    case P_WRITER:
      originMember = originPort->get_member(WRITER_ADDR);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(WRITER_EN);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(WRITER_CLK);
      addSignal(originMember->name, originMember);
      break;
    case P_READWRITER:
      originMember = originPort->get_member(READWRITER_ADDR);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(READWRITER_EN);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(READWRITER_CLK);
      addSignal(originMember->name, originMember);
      originMember = originPort->get_member(READWRITER_WMODE);
      addSignal(originMember->name, originMember);
      break;
    default:
      Panic();
  }
}
/*
memory: Mem ALLID ':' info INDENT mem_compulsory mem_optional mem_ruw DEDENT { $$ = newNode(P_MEMORY, synlineno(), $4, $2, 0); $$->appendChildList($6); $$->appendChild($8); $$->appendChildList($7); }
mem_compulsory: mem_datatype mem_depth mem_rlatency mem_wlatency { $$ = new PList(); $$->append(4, $1, $2, $3, $4); }
mem_optional: mem_reader mem_writer mem_readwriter { $$ = $1; $$->concat($2); $$->concat($3); }
*/
void Context::visitMemory(PNode* mem) {
  TYPE_CHECK(mem, 5, INT32_MAX, P_MEMORY);
  path->cd(SEP_MODULE, mem->name);
  TypeInfo* info = visitMemType(mem->getChild(0));
  int depth = visitMemDepth(mem->getChild(1));
  int rlatency = visitReadLatency(mem->getChild(2));
  int wlatency = visitWriteLatency(mem->getChild(3));
  visitRUW(mem->getChild(4));

  moduleInstances.insert(path->cwd());

  if (info->isAggr()) {
    std::vector<Node*> allSubMem;
    for (auto entry : info->aggrMember) {
      Node* node = entry.first;
      node->type = NODE_MEMORY;
      allSubMem.push_back(node);
      g->memory.push_back(node);
      node->set_memory(depth, rlatency, wlatency);
    }
    for (int i = 5; i < mem->getChildNum(); i ++) {
      // AggrParentNode* parent = new AggrParentNode(path->subDir(SEP_AGGR, mem->getChild(i)->name));
      /* data & mask is unused */
      Node* originPort = visitMemPort(mem->getChild(i), depth, 0, 0, "", NULL);
      addOriginMember(originPort, mem->getChild(i));
      std::map<Node*, Node*>allSubPort;
      /* create all ports */
      for (auto node : allSubMem) {
        std::string suffix = replacePrefix(path->cwd(), "", node->name).c_str();
        Node* portNode = visitMemPort(mem->getChild(i), depth, node->width, node->sign, suffix, node);
        node->add_member(portNode);
        allSubPort[node] = portNode;
        for (Node* member : portNode->member) {
          addSignal(member->name, member);
        }
        /* create alias */
        memoryAlias(originPort, portNode);
      }
      /* create aggr parent */
      for (AggrParentNode* parent : info->aggrParent) {
        if (parent->member.size() == 0) continue;
        std::string suffix = replacePrefix(path->cwd(), "", parent->name);
        PNode* memPort = mem->getChild(i);
        if (memPort->type == P_READER) { // data only
          AggrParentNode* portAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "data" + suffix));
          for (auto entry : parent->member) {
            portAggr->addMember(allSubPort[entry.first]->get_member(READER_DATA), entry.second);
          }
          addDummy(portAggr->name, portAggr);

        } else if (memPort->type == P_WRITER) {
          AggrParentNode* maskAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "mask" + suffix));
          AggrParentNode* dataAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "data" + suffix));
          for (auto entry : parent->member) {
            maskAggr->addMember(allSubPort[entry.first]->get_member(WRITER_MASK), entry.second);
            dataAggr->addMember(allSubPort[entry.first]->get_member(WRITER_DATA), entry.second);
          }
          addDummy(maskAggr->name, maskAggr);
          addDummy(dataAggr->name, dataAggr);

        } else if (memPort->type == P_READWRITER) {
          AggrParentNode* wmaskAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "wmask" + suffix));
          AggrParentNode* rdataAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "rdata" + suffix));
          AggrParentNode* wdataAggr = new AggrParentNode(path->subDir(SEP_MODULE, memPort->name + SEP_AGGR + "wdata" + suffix));
          for (auto entry : parent->member) {
            wmaskAggr->addMember(allSubPort[entry.first]->get_member(READWRITER_WMASK), entry.second);
            rdataAggr->addMember(allSubPort[entry.first]->get_member(READWRITER_RDATA), entry.second);
            wdataAggr->addMember(allSubPort[entry.first]->get_member(READWRITER_RDATA), entry.second);
          }
          addDummy(wmaskAggr->name, wmaskAggr);
          addDummy(rdataAggr->name, rdataAggr);
          addDummy(wdataAggr->name, wdataAggr);
        }
      }
    }

  } else {
    Node* memNode = allocNode(NODE_MEMORY, path->cwd(), mem->lineno);
    g->memory.push_back(memNode);
    memNode->updateInfo(info);

    memNode->set_memory(depth, rlatency, wlatency);

    for (int i = 5; i < mem->getChildNum(); i ++) {
      PNode* port = mem->getChild(i);
      Node* portNode = visitMemPort(port, depth, info->width, info->sign, "", memNode);
      memNode->add_member(portNode);
      for (Node* member : portNode->member) {
        addSignal(member->name, member);
      }
    }


  }
  path->cdLast();
}

#if 0
struct ChirrtlVistor {
  static Node* findMemory(graph *g, std::string &Name) {
    Node* mem = nullptr;
    for (auto& m : g->memory) {
      if (m->name == Name) {
        mem = m;
        break;
      }
    }
    return mem;
  }

  static NodeType createType(PNode* n) {
    switch (n->type) {
      case P_WRITE: return NODE_WRITER;
      case P_READ: return NODE_READER;
      default: break;
    }

    Panic();
  }

  static ASTExpTree* createRef(Node* n) {
    auto* ret = new ASTExpTree(false);
    ret->getExpRoot()->setNode(n);
    return ret;
  }

  static ASTExpTree * createInvalid() {
    auto *ret = new ASTExpTree(false);
    ret->setOp(OP_INVALID);
    return ret;
  }

  enum class ConnectType { Invalid = 0, Empty, Zero };

  static void createConnectCommon(Node* ref, ASTExpTree* exp, bool addIndexChild, const std::string& indexValue = "0", ConnectType Type = ConnectType::Invalid) {
    stmtsNodes.insert(ref);

    std::pair<ExpTree*, ENode*> growWhenTrace(ExpTree * valTree, int depth);
    auto [valTree, whenNode] = growWhenTrace(ref->valTree, ref->whenDepth);

    // Create ASTExpr for ref node
    auto* from = createRef(ref);
    valTree->setlval(from->getExpRoot());

    // Add index child if needed
    if (addIndexChild) from->addChild(allocIntIndex(indexValue));

    if (whenNode) {
      auto idx = whenTrace.back().first ? 1 : 2;
      whenNode->setChild(idx, exp->getExpRoot());

      switch (Type) {
        case ConnectType::Invalid: {
          auto invalid = createInvalid();
          whenNode->setChild(++idx, invalid->getExpRoot());
          break;
        }

        case ConnectType::Empty: break;

        case ConnectType::Zero: {
          auto zero = Builder::createZero();

          whenNode->setChild(++idx, zero->getExpRoot());
          break;
        }
      }
    } else {
      valTree->setRoot(exp->getExpRoot());
    }

    ref->valTree = valTree;
  }

  static void createConnect(Node* ref, ASTExpTree* exp, ConnectType Type = ConnectType::Invalid) {
    createConnectCommon(ref, exp, false, "0", Type);
  }

  static void createIdxConnect(Node* ref, ASTExpTree* exp, const std::string& indexValue,
                               ConnectType Type = ConnectType::Invalid) {
    createConnectCommon(ref, exp, true, indexValue, Type);
  }

  struct Builder {
    static ASTExpTree* createEnable() {
      ASTExpTree* ret = new ASTExpTree(false);
      ret->getExpRoot()->strVal = "1";
      ret->setType(1, false);
      ret->setOp(OP_INT);
      return ret;
    }

    static ASTExpTree* createZero() {
      ASTExpTree* ret = new ASTExpTree(false);
      ret->getExpRoot()->strVal = "0";
      ret->setType(1, false);
      ret->setOp(OP_INT);
      return ret;
    }

    static ASTExpTree* createMask() {
      ASTExpTree* ret = new ASTExpTree(false);
      ret->getExpRoot()->strVal = "1";
      ret->setType(1, false);
      ret->setOp(OP_INT);
      return ret;
    }
  };

  static void resetMember(Node* N) {
    auto reset = [&](int n) {
      for (int i = 0; i < n; ++i) N->add_member(nullptr);
    };

    if (N->type == NODE_READER)
      reset(READER_MEMBER_NUM);
    else
      reset(WRITER_MEMBER_NUM);
  }

  static void createMembers(ModulePath *path, Node* N, int width, int depth, bool sign, std::string suffix, Node* Mem) {
    add_member(path, N, "addr" + suffix, WRITER_ADDR, upperLog2(depth), false);
    add_member(path, N, "en" + suffix, WRITER_EN, 1, false);

    Node* clk = add_member(path, N, "clk" + suffix, WRITER_CLK, 1, false);
    clk->isClock = true;

    Node* data = add_member(path, N, "data" + suffix, READER_DATA, width, sign);
    if (Mem) { data->dimension.insert(data->dimension.end(), Mem->dimension.begin(), Mem->dimension.end()); }

    if (N->type != NODE_READER) {
      auto* mask = add_member(path, N, "mask" + suffix, WRITER_MASK, 1, false);
      mask->dimension.insert(mask->dimension.end(), Mem->dimension.begin(), Mem->dimension.end());
    }
  }

  static Node* findRef(std::string& Name, std::string Member) {
    Node* ret = nullptr;

    for (const auto& [key, value] : allSignals) {
      auto isMem = value->type == NODE_MEM_MEMBER;
      if (key.starts_with(Name) && isMem)
        if (key.find(Member) != std::string::npos) {
          ret = value;
          break;
        }
    }

    return ret;
  }

  static void createAccess(std::string& Name, ASTExpTree* Addr, bool write, int depth) {
    auto* addr = findRef(Name, "$$addr");
    auto* en = findRef(Name, "$$en");
    auto* clk = findRef(Name, "$$clk");
    auto* mask = findRef(Name, "$$mask");

    // Create Addr connect
    // e.g.
    //    connect array.MPORT.addr, _T_2
    createConnect(addr, Addr, ConnectType::Invalid);

    // Create Access Enable
    // e.g.
    //    Connect array.MPORT.en, UInt<1>(1)
    auto* enableRef = Builder::createEnable();
    createConnect(en, enableRef, ConnectType::Zero);

    // Create Access Clock
    // e.g.
    //    Connect array.MPORT.clk, clock
    auto* clkRef = createRef(getSignal("clock"));
    auto* from = createRef(clk);
    auto* valTree = new ExpTree(nullptr);

    valTree->setlval(from->getExpRoot());
    valTree->setRoot(clkRef->getExpRoot());

    clk->valTree = valTree;

    // Create Write Access Mask
    // e.g.
    //    Connect array.MPORT.mask[0], UInt<1>(1)
    if (write) {
      auto* maskRef = Builder::createMask();
      for (int i = 0; i < depth; ++i) createIdxConnect(mask, maskRef, std::to_string(i), ConnectType::Zero);

      createConnect(mask, maskRef, ConnectType::Zero);
    }
  }
};
#endif

Node* Context::visitChirrtlPort(PNode* port, int width, int depth, bool sign, std::string suffix, Node* node, ENode* addr_enode, Node* clock_node) {
  assert(port->type == P_READ || port->type == P_WRITE || port->type == P_INFER);
  // Add prefix port name
  path->cd(SEP_MODULE, port->name);
  NodeType type;
  OPType op;
  if (port->type == P_READ) {
    type = NODE_READER;
    op = OP_READ_MEM;
  } else if (port->type == P_WRITE) {
    type = NODE_WRITER;
    op = OP_WRITE_MEM;
  } else if (port->type == P_INFER) {
    type = NODE_INFER;
    op = OP_INFER_MEM;
  } else TODO();

  Node* ret = allocNode(type, path->cwd() + suffix, port->lineno);
  ret->dimension = node->dimension;
  ENode* enode = new ENode(op);
  enode->memoryNode = node;
  enode->addChild(addr_enode);
  ret->memTree = new ExpTree(enode, ret);
  ret->clock = clock_node;

  path->cdLast();
  return ret;
}

void Context::visitChirrtlMemPort(PNode* port) {
  // If we are in the top module, 
  //    the memory name does not need to have the prefix added.
  ASTExpTree* addr = visitExpr(port->getChild(0));
  ENode* addr_enode = addr->getExpRoot();
  Node* clock_node = getSignal(path->subDir(SEP_MODULE, port->getExtra(1)));
  std::string memName = path->subDir(SEP_MODULE, port->getExtra(0));

 if (port->type == P_READ && memoryMap[memName].first[0]->rlatency == 1) {
    Node* addr_src = allocNode(NODE_REG_SRC, format("%s%s%s%s%s", path->cwd().c_str(), SEP_MODULE, port->name.c_str(), SEP_AGGR, "IN"), port->lineno);
    g->addReg(addr_src);
    Node* addr_dst = addr_src->dup();
    addr_dst->type = NODE_REG_DST;
    addr_dst->name += format("%s%s", SEP_AGGR, "NEXT");
    addSignal(addr_src->name, addr_src);
    addSignal(addr_dst->name, addr_dst);
    addr_src->bindReg(addr_dst);
    addr_src->clock = addr_dst->clock = clock_node;
    addr_src->valTree = new ExpTree(addr_enode, addr_src);
    ENode* resetCond = new ENode(OP_INT);
    resetCond->width = 1;
    resetCond->strVal = "h0";
    addr_src->resetCond = new ExpTree(resetCond, addr_src);
    addr_src->resetVal = new ExpTree(new ENode(addr_src), addr_src);
    addr_enode = new ENode(addr_src);
  }

  Assert(memoryMap.find(memName) != memoryMap.end(), "Could not find memory: %s", memName.c_str());
  std::vector<Node*> memoryMembers;
  for (Node* mem : memoryMap[memName].first) {
    int depth = mem->depth;
    bool sign = mem->sign;
    int width = mem->width;
    std::string suffix = replacePrefix(memName, "", mem->name).c_str();
    Node* portNode = visitChirrtlPort(port, width, depth, sign, suffix, mem, addr_enode, clock_node);
    mem->add_member(portNode);
    addSignal(portNode->name, portNode);
    memoryMembers.push_back(portNode);
  }

  for (AggrParentNode* memParent : memoryMap[memName].second) {
    std::string suffix = replacePrefix(memName, "", memParent->name).c_str();
    AggrParentNode* parent = new AggrParentNode(path->subDir(SEP_MODULE, port->name) + suffix);
    for (auto entry : memParent->member) {
      std::string memberSuffix = replacePrefix(memName, "", entry.first->name).c_str();
      Node* memberNode = getSignal(path->subDir(SEP_MODULE, port->name) + memberSuffix);
      parent->addMember(memberNode, entry.second);
    }
    addDummy(parent->name, parent);
  }
}

// TODO: Comb memory support
void Context::visitChirrtlMemory(PNode* mem) {
  assert(mem->type == P_SEQ_MEMORY || mem->type == P_COMB_MEMORY);
  path->cd(SEP_MODULE, mem->name);
  bool isSeq = mem->type == P_SEQ_MEMORY;

  // Transform the vector type into memory type
  // UInt<32>[1][2]
  //    =>
  //        type : UInt<32>[1]
  //        depth: 2
  TypeInfo* type = visitMemType(mem->getChild(0));
  int depth = type->dimension.front();
  type->dimension.erase(type->dimension.begin());
  // Convert to firrtl memory
  int rlatency = isSeq;
  int wlatency = 1;
  if (isSeq && mem->getChildNum() >= 2) visitRUW(mem->getChild(1));

  moduleInstances.insert(path->cwd());

  if (type->isAggr()) {
    memoryMap[path->cwd()] = std::pair(std::vector<Node*>(), std::vector<AggrParentNode*>());
    for (auto& entry : type->aggrMember) {
      entry.first->dimension.erase(entry.first->dimension.begin());
      auto* node = entry.first;
      node->type = NODE_MEMORY;
      memoryMap[path->cwd()].first.push_back(node);
      g->memory.push_back(node);
      node->set_memory(depth, rlatency, wlatency);
    }
    memoryMap[path->cwd()].second = type->aggrParent;
  } else {
    auto* memNode = allocNode(NODE_MEMORY, path->cwd(), mem->lineno);
    memoryMap[path->cwd()] = std::pair(std::vector<Node*>{memNode}, std::vector<AggrParentNode*>());
    g->memory.push_back(memNode);
    memNode->updateInfo(type);
    memNode->set_memory(depth, rlatency, wlatency);
  }

  path->cdLast();
}

/*
| Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
*/
void Context::visitInst(PNode* inst) {
  TYPE_CHECK(inst, 0, 0, P_INST);
  auto iterator = moduleMap->find(inst->getExtra(0));
  Assert(inst->getExtraNum() >= 1 && iterator != moduleMap->end(),
               "Module %s is not defined!\n", inst->getExtra(0).c_str());
  PNode* module = iterator->second;
  path->cd(SEP_MODULE, inst->name);
  moduleInstances.insert(path->cwd());
  switch(module->type) {
    case P_MOD: visitModule(module); break;
    case P_EXTMOD: visitExtModule(module); break;
    case P_INTMOD: TODO();
    default:
      Panic();
  }
  path->cdLast();
}

AggrParentNode* Context::allocNodeFromAggr(AggrParentNode* parent) {
  AggrParentNode* ret = new AggrParentNode(path->cwd());
  std::string oldPrefix = parent->name;
  /* alloc all real nodes */
  for (auto entry : parent->member) {
    Node* member = entry.first;
    std::string name = replacePrefix(oldPrefix, path->cwd(), member->name);
    /* the type of parent can be registers, thus the node->type cannot set to member->type */
    Node* node = member->dup(NODE_OTHERS, name); // SEP_AGGR is already in name
  
    addSignal(node->name, node);
    ret->addMember(node, entry.second);
  }
  /* alloc all dummy nodes, and connect them to real nodes stored in allSignals */
  for (AggrParentNode* aggrMember : parent->parent) {
    // create new aggr node
    AggrParentNode* aggrNode = new AggrParentNode(replacePrefix(oldPrefix, path->cwd(), aggrMember->name));
    // update member and parent in new aggrNode
    for (auto entry : aggrMember->member) {
      aggrNode->addMember(getSignal(replacePrefix(oldPrefix, path->cwd(), entry.first->name)), entry.second);
    }
    // the children of aggrMember are earlier than it
    for (AggrParentNode* parent : aggrMember->parent) {
      aggrNode->addParent(getDummy(replacePrefix(oldPrefix, path->cwd(), parent->name)));
    }

    addDummy(aggrNode->name, aggrNode);
    ret->addParent(aggrNode);
  }
  return ret;
}

std::vector<int> ENode::getDim() {
  std::vector<int> dims;
  if (nodePtr) {
    for (size_t i = getChildNum(); i < nodePtr->dimension.size(); i ++) dims.push_back(nodePtr->dimension[i]);
    return dims;
  }
  switch (opType) {
    case OP_MUX: return getChild(1)->getDim();
    case OP_WHEN:
      if (getChild(1)) return getChild(1)->getDim();
      if (getChild(2)) return getChild(2)->getDim();
      break;
    default:
      break;
  }

  return dims;
}

/*
| Node ALLID '=' expr info { $$ = newNode(P_NODE, synlineno(), $5, $2, 1, $4); }
*/
void Context::visitNode(PNode* node) {
  TYPE_CHECK(node, 1, 1, P_NODE);
  ASTExpTree* exp = visitExpr(node->getChild(0));
  path->cd(SEP_MODULE, node->name);
  if (exp->isAggr()) {// create all nodes in aggregate
    AggrParentNode* aggrNode = allocNodeFromAggr(exp->getParent());
    Assert(aggrNode->size() == exp->getAggrNum(), "aggrMember num %d tree num %d", aggrNode->size(), exp->getAggrNum());
    for (int i = 0; i < aggrNode->size(); i ++) {
      aggrNode->member[i].first->valTree = new ExpTree(exp->getAggr(i), aggrNode->member[i].first);
      aggrNode->member[i].first->dimension.clear();
      std::vector<int> dims = exp->getAggr(i)->getDim();
      aggrNode->member[i].first->dimension.insert(aggrNode->member[i].first->dimension.end(), dims.begin(), dims.end());
    }
    addDummy(aggrNode->name, aggrNode);
  } else {
    Node* n = allocNode(NODE_OTHERS, path->cwd(), node->lineno);
    std::vector<int> dims = exp->getExpRoot()->getDim();
    n->dimension.insert(n->dimension.end(), dims.begin(), dims.end());

    n->valTree = new ExpTree(exp->getExpRoot(), n);
    addSignal(n->name, n);
  }
  path->cdLast();
}
/*
| reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
*/
void Context::visitConnect(PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(connect->getChild(0));
  ASTExpTree* exp = visitExpr(connect->getChild(1));

  Assert(!(ref->isAggr() ^ exp->isAggr()) || exp->isInvalid(), "lineno %d type not match, ref aggr %d exp aggr %d", connect->getChild(0)->lineno, ref->isAggr(), exp->isAggr());
  if(ref->isAggr() && exp->isInvalid()) {
    for (int i = 0; i < ref->getAggrNum(); i ++) {
      if (ref->getFlip(i)) continue;
      Node* node = ref->getAggr(i)->getNode();
      ExpTree* valTree = new ExpTree(exp->getExpRoot()->dup(), ref->getAggr(i));
      if (node->isArray()) {
        node->addArrayVal(valTree);
        if (exp->isInvalid()) {
          int beg, end;
          std::tie(beg, end) = ref->getAggr(i)->getIdx(node);
          if (beg < 0) TODO();
          for (int i = beg; i <= end; i ++) node->invalidIdx.insert(i);
        }
      } else if (!node->valTree) node->valTree = valTree;
    }
  } else if (ref->isAggr()) {
    for (int i = 0; i < ref->getAggrNum(); i ++) {
      if (ref->getFlip(i)) {
        Node* node = exp->getAggr(i)->getNode();
        if (!node) TODO(); // like a <= mux(cond, b, c)

        ExpTree* valTree = new ExpTree(ref->getAggr(i), exp->getAggr(i));
        if (node->isArray()) node->addArrayVal(valTree);
        else node->valTree = valTree;
      } else {
        Node* node = ref->getAggr(i)->getNode();
        ExpTree* valTree = new ExpTree(exp->getAggr(i), ref->getAggr(i));
        if (node->isArray()) node->addArrayVal(valTree);
        else node->valTree = valTree;
      }
    }
  } else {
    Node* node = ref->getExpRoot()->getNode();
    if (exp->isInvalid()) {
      if (!node->isArray() && node->valTree) return;
    }
    ExpTree* valTree = new ExpTree(exp->getExpRoot(), ref->getExpRoot());
    if (node->isArray()) {
      node->addArrayVal(valTree);
      if (exp->isInvalid()) {
        int beg, end;
        std::tie(beg, end) = ref->getExpRoot()->getIdx(node);
        if (beg < 0) TODO();
        for (int i = beg; i <= end; i ++) node->invalidIdx.insert(i);
      }
    } else {
      // Override all prefix nodes in the prev assignTree.
      if (!exp->isInvalid()) node->assignTree.clear();
      node->valTree = valTree;
    }
  }
}

void Context::visitPartialConnect(PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_PAR_CONNECT);
  ASTExpTree* ref = visitReference(connect->getChild(0));
  ASTExpTree* exp = visitExpr(connect->getChild(1));
  Assert(ref->isAggr() && exp->isAggr(), "lineno %d type not match, ref aggr %d exp aggr %d", connect->getChild(0)->lineno, ref->isAggr(), exp->isAggr());

  std::map<std::string, std::pair<ENode*, bool>> refName;
  for (int i = 0; i < ref->getAggrNum(); i ++) {
    std::string lastName = lastField(ref->getAggr(i)->getNode()->name);
    Assert (refName.find(lastName) == refName.end(), "line %d : %s duplicated", connect->lineno, lastName.c_str());
    refName[lastName] = std::make_pair(ref->getAggr(i), ref->getFlip(i));
  }
  std::map<std::string, ENode*> expName;
  for (int i = 0; i < exp->getAggrNum(); i ++) {
    if (!exp->getAggr(i)->getNode()) TODO(); // like a <- mux(cond, b, c)
    std::string lastName = lastField(exp->getAggr(i)->getNode()->name);
    Assert (expName.find(lastName) == expName.end(), "line %d : %s duplicated", connect->lineno, lastName.c_str());
    expName[lastName] = exp->getAggr(i);
  }
  for (auto entry : refName) {
    if (expName.find(entry.first) == expName.end()) continue;
    ENode* refENode = entry.second.first;
    ENode* expENode = expName[entry.first];
    if (entry.second.second) { // isFlip
      ExpTree* valTree = new ExpTree(refENode, expENode);
      Node* expNode = expENode->getNode();
      if(!expNode) TODO();
      if (expNode->isArray()) expNode->addArrayVal(valTree);
      else expNode->valTree = valTree;
    } else {
      ExpTree* valTree = new ExpTree(expENode, refENode);
      Node* refNode = refENode->getNode();
      if(!refNode) TODO();
      if (refNode->isArray()) refNode->addArrayVal(valTree);
      else refNode->valTree = valTree;
    }
  }

}

bool matchWhen(ENode* enode, int depth) {
  if (enode->opType != OP_WHEN) return false;
  Assert(enode->getChildNum() == 3, "invalid child num %d", enode->getChildNum());
  Assert(enode->getChild(0) && enode->getChild(0)->nodePtr, "invalid cond");
  if (enode->getChild(0)->nodePtr == whenTrace[depth].second) return true;
  return false;
}

/* find the latest matched when ENode and the number of matched */
std::pair<ENode*, int> getDeepestWhen(ExpTree* valTree, int depth) {
  if (!valTree) return std::make_pair(nullptr, depth);
  ENode* checkNode = valTree->getRoot();
  ENode* whenNode = nullptr;
  int deep = depth;

  for (size_t i = depth; i < whenTrace.size(); i ++) {
    if (checkNode && matchWhen(checkNode, i)) {
      whenNode = checkNode;
      deep = i + 1;
      checkNode = whenTrace[i].first ? checkNode->getChild(1) : checkNode->getChild(2);
    } else {
      break;
    }
  }
  return std::make_pair(whenNode, deep);
}

/*
add when Enodes for node
e.g.
whenTrace: (when1, true), (when2, true), (when3, false)
node->valTree: when1
                |
            cond1 when2 any
                    |
              cond2 a any

oldparent: when2; depth: 2
oldRoot: a
newRoot:  when3
            |
      cond3 a b
replace oldRoot by newRoot
*/
std::pair<ExpTree*, ENode*> growWhenTrace(ExpTree* valTree, int depth) {
  ENode* oldParent = nullptr;
  int maxDepth = depth;
  if (valTree) std::tie(oldParent, maxDepth) = getDeepestWhen(valTree, depth);
  if (maxDepth == (int)whenTrace.size()) {
    ExpTree* retTree = valTree ? valTree : new ExpTree(nullptr);
    return std::make_pair(retTree, getWhenEnode(retTree, depth));
  }

  ENode* oldRoot = maxDepth == depth ?
                          (valTree ? valTree->getRoot() : nullptr)
                        : (whenTrace[maxDepth-1].first ? oldParent->getChild(1) : oldParent->getChild(2));
  ENode* childRoot = nullptr;
  if (oldRoot) {
    /* e.g. a <= 0
            when cond:
              a <= 1
    */
    if (oldRoot->opType != OP_WHEN && oldRoot->opType != OP_STMT) {
      childRoot = oldRoot;
      oldRoot = nullptr;
    }
  }
  ENode* newRoot = nullptr; // latest whenNode
  ENode* retENode = oldParent;
  for (int depth = whenTrace.size() - 1; depth >= maxDepth ; depth --) {
    ENode* whenNode = new ENode(OP_WHEN); // return the deepest whenNode
    if (depth == whenTrace.size() - 1) retENode = whenNode;
    ENode* condNode = new ENode(whenTrace[depth].second);

    if (whenTrace[depth].first) {
      whenNode->addChild(condNode);
      whenNode->addChild(newRoot);
      whenNode->addChild(childRoot);
    } else {
      whenNode->addChild(condNode);
      whenNode->addChild(childRoot);
      whenNode->addChild(newRoot);
    }
    newRoot = whenNode;
  }
  if (!oldRoot) {
    if (maxDepth == depth) {
      if (valTree) valTree->setRoot(newRoot);
      else valTree = new ExpTree(newRoot);
    } else {
      oldParent->setChild(whenTrace[maxDepth-1].first ? 1 : 2, newRoot);
    }
  } else {
    if (maxDepth == depth) {
      TODO();
    } else {
      ENode* stmt = nullptr;
      if (oldRoot->opType == OP_STMT) {
        stmt = oldRoot;
      } else {
        stmt = new ENode(OP_STMT);
        stmt->addChild(oldRoot);
      }
      stmt->addChild(newRoot);
      oldParent->setChild(whenTrace[maxDepth-1].first ? 1 : 2, stmt);
    }
  }
  return std::make_pair(valTree, retENode);
}

ENode* getWhenEnode(ExpTree* valTree, int depth) {
  ENode* whenNode;
  int maxDepth;
  std::tie(whenNode, maxDepth) = getDeepestWhen(valTree, depth);
  Assert(maxDepth == (int)whenTrace.size(), "when not match %d %ld", maxDepth, whenTrace.size());
  return whenNode;
}

void Context::whenConnect(Node* node, ENode* lvalue, ENode* rvalue, PNode* connect) {
  if (!node) {
    printf("connect lineno %d\n", connect->lineno);
    TODO();
  }
  ExpTree* valTree = nullptr;
  ENode* whenNode;
  int connectDepth = (node->type == NODE_WRITER || node->type == NODE_INFER) ? 0 :node->whenDepth;
  if (node->isArray()) {
    std::tie(valTree, whenNode) = growWhenTrace(nullptr, connectDepth);
  } else {
    std::tie(valTree, whenNode) = growWhenTrace(node->valTree, connectDepth);
  }
  valTree->setlval(lvalue);
  if (node->type == NODE_WRITER || node->type == NODE_INFER) {
    ENode* writeENode = node->memTree->getRoot()->dup();
    writeENode->opType = OP_WRITE_MEM;
    writeENode->addChild(rvalue);
    rvalue = writeENode;
    node->set_writer();
  }
  if (whenNode) whenNode->setChild(whenTrace.back().first ? 1 : 2, rvalue);
  else valTree->setRoot(rvalue);
  if (node->isArray()) node->addArrayVal(valTree);
  else node->valTree = valTree;
}

/*
| reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
*/
void Context::visitWhenConnect(PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(connect->getChild(0));
  ASTExpTree* exp = visitExpr(connect->getChild(1));
  Assert(!(ref->isAggr() ^ exp->isAggr()), "type not match, ref aggr %d exp aggr %d", ref->isAggr(), exp->isAggr());

  if (ref->isAggr()) {
    for (int i = 0; i < ref->getAggrNum(); i++) {
      if (exp->getFlip(i)) {
        Node* node = exp->getAggr(i)->getNode();
        stmtsNodes.insert(node);
        whenConnect(node, exp->getAggr(i), ref->getAggr(i), connect);
      } else {
        Node* node = ref->getAggr(i)->getNode();
        stmtsNodes.insert(node);
        whenConnect(node, ref->getAggr(i), exp->getAggr(i), connect);
      }
    }
  } else {
    Node* node = ref->getExpRoot()->getNode();
    stmtsNodes.insert(node);
    whenConnect(node, ref->getExpRoot(), exp->getExpRoot(), connect);
  }
}
/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void Context::visitWhenPrintf(PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, "PRINTF_" + std::to_string(print->lineno)), print->lineno);
  ASTExpTree* exp = visitExpr(print->getChild(1));

  ENode* expRoot = exp->getExpRoot();
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    ENode* andNode = new ENode(OP_AND);
    andNode->addChild(expRoot);
    ENode* condNode = new ENode(whenTrace[i].second);
    if (whenTrace[i].first) {
      andNode->addChild(condNode);
    } else {
      ENode* notNode = new ENode(OP_NOT);
      notNode->addChild(condNode);
      andNode->addChild(notNode);
    }
    expRoot = andNode;
  }

  ENode* enode = new ENode(OP_PRINTF);
  enode->strVal = print->getExtra(0);
  enode->addChild(expRoot);

  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(exprs->getChild(i));
    enode->addChild(val->getExpRoot());
  }

  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

void Context::visitWhenAssert(PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, ass->name), ass->lineno);

  ASTExpTree* pred = visitExpr(ass->getChild(1));
  ASTExpTree* en = visitExpr(ass->getChild(2));

  ENode* enRoot = en->getExpRoot();
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    ENode* andNode = new ENode(OP_AND);
    andNode->addChild(enRoot);
    ENode* condNode = new ENode(whenTrace[i].second);
    if (whenTrace[i].first) {
      andNode->addChild(condNode);
    } else {
      ENode* notNode = new ENode(OP_NOT);
      notNode->addChild(condNode);
      andNode->addChild(notNode);
    }
    enRoot = andNode;
  }

  ENode* enode = new ENode(OP_ASSERT);
  enode->strVal = ass->getExtra(0);

  enode->addChild(pred->getExpRoot());
  enode->addChild(enRoot);
  
  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

void Context::visitWhenStop(PNode* stop) {
  TYPE_CHECK(stop, 2, 2, P_STOP);
  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, stop->name), stop->lineno);

  ASTExpTree* exp = visitExpr(stop->getChild(1));

  ENode* cond = exp->getExpRoot();
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    ENode* andNode = new ENode(OP_AND);
    andNode->addChild(cond);
    ENode* condNode = new ENode(whenTrace[i].second);
    if (whenTrace[i].first) {
      andNode->addChild(condNode);
    } else {
      ENode* notNode = new ENode(OP_NOT);
      notNode->addChild(condNode);
      andNode->addChild(notNode);
    }
    cond = andNode;
  }

  ENode* enode = new ENode(OP_EXIT);
  enode->strVal = stop->getExtra(0);
  enode->addChild(cond);

  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

/* return the lvalue node */
void Context::visitWhenStmt(PNode* stmt) {
  switch (stmt->type) {
    case P_NODE: visitNode(stmt); break; // local nodes
    case P_CONNECT: visitWhenConnect(stmt); break;
    case P_WHEN: visitWhen(stmt); break;
    case P_WIRE_DEF: visitWireDef(stmt); break;
    case P_PRINTF: visitWhenPrintf(stmt); break;
    case P_ASSERT: visitWhenAssert(stmt); break;
    case P_STOP: visitWhenStop(stmt); break;
    case P_INST: visitInst(stmt); break;
    case P_REG_DEF: visitRegDef(stmt, P_REG_DEF); break;
    case P_REG_RESET_DEF: visitRegDef(stmt, P_REG_RESET_DEF); break;
    case P_READ :
    case P_WRITE :
    case P_INFER : visitChirrtlMemPort(stmt); break;
    default: printf("Invalid type %d %d\n", stmt->type, stmt->lineno); Panic();
  }
}
void Context::visitWhenStmts(PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitWhenStmt(stmts->getChild(i));
  }
}

Node* Context::allocCondNode(ASTExpTree* condExp, PNode* when) {
  Node* cond = allocNode(NODE_OTHERS, path->subDir(SEP_MODULE, "WHEN_COND_" + std::to_string(when->lineno)), when->lineno);
  cond->valTree = new ExpTree(condExp->getExpRoot(), cond);
  addSignal(cond->name, cond);
  return cond;
}

/*
| When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); }
*/
void Context::visitWhen(PNode* when) {
  TYPE_CHECK(when, 3, 3, P_WHEN);
  ASTExpTree* condExp = visitExpr(when->getChild(0));
  Node* condNode = allocCondNode(condExp, when);
  // allocWhenId(when); distinguish when through condNode rather than id
  whenTrace.push_back(std::make_pair(true, condNode));
  visitWhenStmts(when->getChild(1));
  
  whenTrace.back().first = false;
  visitWhenStmts(when->getChild(2));
  
  whenTrace.pop_back();
}

/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void Context::visitPrintf(PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, print->name), print->lineno);
  ASTExpTree* exp = visitExpr(print->getChild(1));

  ENode* enode = new ENode(OP_PRINTF);
  enode->strVal = print->getExtra(0);

  enode->addChild(exp->getExpRoot());
  
  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(exprs->getChild(i));
    enode->addChild(val->getExpRoot());
  }

  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}
/*
  | Stop '(' expr ',' expr ',' INT ')' info   { $$ = newNode(P_STOP, synlineno(), $9, NULL, 2, $3, $5); $$->appendExtraInfo($7); }
  | Stop '(' expr ',' expr ',' INT ')' ':' ALLID info   { $$ = newNode(P_STOP, synlineno(), $11, $10, 2, $3, $5); $$->appendExtraInfo($7); }
*/
void Context::visitStop(PNode* stop) {
  TYPE_CHECK(stop, 2, 2, P_STOP);

  ASTExpTree* exp = visitExpr(stop->getChild(1));
  ENode* enode = new ENode(OP_EXIT);
  enode->addChild(exp->getExpRoot());
  enode->strVal = stop->getExtra(0);

  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, stop->name), stop->lineno);
  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

/*
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
*/
void Context::visitAssert(PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, path->subDir(SEP_MODULE, ass->name), ass->lineno);

  ASTExpTree* pred = visitExpr(ass->getChild(1));
  ASTExpTree* en = visitExpr(ass->getChild(2));

  ENode* enode = new ENode(OP_ASSERT);
  enode->strVal = ass->getExtra(0);

  enode->addChild(pred->getExpRoot());
  enode->addChild(en->getExpRoot());

  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

void saveWhenTree() {
  for (Node* node : stmtsNodes) {
    if (node->valTree) {
      if (!node->isArray() && node->assignTree.size() > 0 && countEmptyWhen(node->valTree) == 1) { // TODO: check if countEmptyWhen = 0
        fillEmptyWhen(node->valTree, node->assignTree.back()->getRoot());
        node->assignTree.back() = node->valTree;
        node->valTree = nullptr;
      } else {
        node->assignTree.push_back(node->valTree);
        node->valTree = nullptr;
      }
      if (countEmptyWhen(node->assignTree.back()) == 0 && node->assignTree.back()->getRoot()->opType != OP_INVALID) {
        if (node->assignTree.size() > 1) {
          node->assignTree.erase(node->assignTree.begin(), node->assignTree.end() - 1);
        }
      }
    }
  }
}

/*
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
    | Reg ALLID ':' type ',' expr RegWith INDENT RegReset '(' expr ',' expr ')' info DEDENT { $$ = newNode(P_REG_DEF, $4->lineno, $15, $2, 4, $4, $6, $11, $13); }
    | memory    { $$ = $1;}
    | Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
    | Node ALLID '=' expr info { $$ = newNode(P_NODE, synlineno(), $5, $2, 1, $4); }
    | reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
    | reference "<-" expr info  { TODO(); }
    | reference Is Invalid info { $$ = NULL; }
    | Attach '(' references ')' info { TODO(); }
    | When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); }
    | Stop '(' expr ',' expr ',' INT ')' info   { TODO(); }
    | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Skip info { $$ = NULL; }
*/
void Context::visitStmt(PNode* stmt) {
  stmtsNodes.clear();
  switch (stmt->type) {
    case P_WIRE_DEF: visitWireDef(stmt); break;
    case P_REG_DEF: visitRegDef(stmt, P_REG_DEF); break;
    case P_REG_RESET_DEF: visitRegDef(stmt, P_REG_RESET_DEF); break;
    case P_INST: visitInst(stmt); break;
    case P_MEMORY: visitMemory(stmt); break;
    case P_SEQ_MEMORY :
    case P_COMB_MEMORY: visitChirrtlMemory(stmt); break;
    case P_READ:
    case P_WRITE:
    case P_INFER: visitChirrtlMemPort(stmt); break;
    case P_NODE: visitNode(stmt); break;
    case P_CONNECT: visitConnect(stmt); break;
    case P_PAR_CONNECT: visitPartialConnect(stmt); break;
    case P_WHEN:
      visitWhen(stmt);
      saveWhenTree();
      break;
    case P_PRINTF: visitPrintf(stmt); break;
    case P_ASSERT: visitAssert(stmt); break;
    case P_STOP: visitStop(stmt); break;
    default:
      printf("invalid stmt type %d in lineno %d\n", stmt->type, stmt->lineno);
      Panic();
  }
}

/*
statements: { $$ = new PNode(P_STATEMENTS, synlineno()); }
    | statements statement { $$ =  $1; $1->appendChild($2); }
*/
void Context::visitStmts(PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitStmt(stmts->getChild(i));
  }
}

/*
  module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
  children: ports, statments
*/
void Context::visitTopModule(PNode* topModule) {
  TYPE_CHECK(topModule, 2, 2, P_MOD);
  visitTopPorts(topModule->getChild(0));
  visitStmts(topModule->getChild(1));
}

void updatePrevNext(Node* n) {
  switch (n->type) {
    case NODE_INP:
      Assert(!n->valTree || n->valTree->isInvalid(), "valTree of %s should be empty", n->name.c_str());
      break;
    case NODE_REG_SRC:
    case NODE_REG_DST:
    case NODE_SPECIAL:
    case NODE_OUT:
    case NODE_MEM_MEMBER:
    case NODE_OTHERS:
    case NODE_READER:
    case NODE_WRITER:
    case NODE_READWRITER:
    case NODE_EXT_IN:
    case NODE_EXT_OUT:
    case NODE_EXT:
      n->updateConnect();
      break;
/* should not exists in allSignals */
    // case NODE_L1_RDATA:
    case NODE_MEMORY:
    case NODE_INVALID:
    default: Panic();

  }
}


/*
  traverse the AST and generate graph
*/
graph* AST2Graph(PNode* root) {
  graph *g = new graph();
  g->name = root->name;
  Context c;
  c.g = g;
  c.path = new ModulePath;

  PNode* topModule = NULL;

  moduleMap = new std::map<std::string, PNode*>;
  for (int i = 0; i < root->getChildNum(); i++) {
    PNode* module = root->getChild(i);
    if (module->name == root->name) { topModule = module; }
    moduleMap->emplace(module->name, module);
  }
  Assert(topModule, "Top module can not be NULL\n");
  c.visitTopModule(topModule);
  delete moduleMap;
  delete c.path;

/* infer memory port */

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    if (it->second->type == NODE_INFER || it->second->type == NODE_READER) {
      it->second->memTree->getRoot()->opType = OP_READ_MEM;
      it->second->valTree = it->second->memTree;
      it->second->set_reader();
    }
  }

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    Node* node = it->second;
    if (node->valTree) {
      node->assignTree.push_back(node->valTree);
      node->valTree = nullptr;
    }
  }
  removeDummyDim(g);

  for (Node* reg : g->regsrc) {
    /* set lvalue to regDst */
    for (ExpTree* tree : reg->assignTree) {
      if (tree->getlval()) {
        Assert(tree->getlval()->nodePtr, "lvalue in %s is not node", reg->name.c_str());
        tree->getlval()->nodePtr = reg->getDst();
      }
    }
    reg->getDst()->assignTree.insert(reg->getDst()->assignTree.end(), reg->assignTree.begin(), reg->assignTree.end());
    reg->assignTree.clear();

    for (ExpTree* tree : reg->getSrc()->arrayVal) {
      if (!tree) continue;
      Assert(tree->getlval()->nodePtr, "lvalue in %s is not node", reg->name.c_str());
      tree->getlval()->nodePtr = reg->getDst();
    }
    reg->getDst()->arrayVal.insert(reg->getDst()->arrayVal.end(), reg->arrayVal.begin(), reg->arrayVal.end());
    reg->arrayVal.clear();
    reg->addReset();
    reg->addUpdateTree();
  }


  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->invalidArrayOptimize();
  }
  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    updatePrevNext(it->second);
  }

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->constructSuperNode();
  }
  /* must be called after constructSuperNode all finished */
  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->constructSuperConnect();
  }
  /* find all sources: regsrc, memory rdata, input, constant node */
  for (Node* reg : g->regsrc) {
    if (reg->prev.size() == 0)
      g->supersrc.push_back(reg->super);
    if (reg->getDst()->prev.size() == 0)
      g->supersrc.push_back(reg->getDst()->super);
  }

  for (Node* input : g->input) {
    g->supersrc.push_back(input->super);
    for (ExpTree* tree : input->assignTree) Assert(tree->isInvalid(), "input %s not invalid", input->name.c_str());
    input->assignTree.clear();
  }
  for (auto it : allSignals) {
    if ((it.second->type == NODE_OTHERS || it.second->type == NODE_READER || it.second->type == NODE_WRITER ||
        it.second->type == NODE_SPECIAL || it.second->type == NODE_EXT || it.second->type == NODE_EXT_IN || it.second->type == NODE_EXT_OUT)
        && it.second->super->prev.size() == 0) {
      g->supersrc.push_back(it.second->super);
    }
    if (it.second->isArray()) {
      for (ExpTree* tree : it.second->arrayVal) {
        if(tree->isConstant()) g->halfConstantArray.insert(it.second);
      }
    }
  }
#ifdef ORDERED_TOPO_SORT
  std::sort(g->supersrc.begin(), g->supersrc.end(), [](SuperNode* a, SuperNode* b) {return a->id < b->id;});
#endif
  return g;
}

bool ExpTree::isConstant() {
  std::stack<ENode*> s;
  s.push(getRoot());
  while(!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (top->getNode()) return false;
    for (ENode* childENode : top->child) {
      if(childENode) s.push(childENode);
    }
  }
  return true;
}

bool nameExist(std::string str) {
  return allSignals.find(str) != allSignals.end();
}

void writer2Reg(ExpTree* tree) {
  std::stack<std::tuple<ENode*, ENode*, int>>s;
  s.push(std::make_tuple(tree->getRoot(), nullptr, 0));
  while (!s.empty()) {
    ENode* top, *parent;
    int idx;
    std::tie(top, parent, idx) = s.top();
    s.pop();
    if (top->opType == OP_WRITE_MEM) {
      ENode* newChild = top->getChild(1);
      if (parent) parent->setChild(idx, newChild);
      else tree->setRoot(newChild);
    } else {
      for (int i = 0; i < top->getChildNum(); i ++) {
        if (top->getChild(i)) s.push(std::make_tuple(top->getChild(i), top, i));
      }
    }
  }
}

void ExpTree::removeDummyDim(std::map<Node*, std::vector<int>>& arrayMap, std::set<ENode*>& visited) {
  std::stack<ENode*> s;
  s.push(getRoot());
  if (getlval()) s.push(getlval());
  while (!s.empty()) {
    ENode* top = s.top();
    s.pop();
    if (visited.find(top) != visited.end()) continue;
    visited.insert(top);
    if (top->getNode() && arrayMap.find(top->getNode()) != arrayMap.end()) {
      std::vector<ENode*> childENode;
      for (size_t i = 0; i < arrayMap[top->getNode()].size(); i ++) {
        int validIdx = arrayMap[top->getNode()][i];
        if (validIdx >= top->getChildNum()) break;
        childENode.push_back(top->getChild(validIdx));
      }
      if (childENode.size() == top->getChildNum()) continue;
      else {
        top->child = std::vector<ENode*>(childENode);
      }
    }
    for (ENode* child : top->child) {
      if (child) s.push(child);
    }
  }
}

void setZeroTree(Node* node) {
  ENode* zero = new ENode(OP_INT);
  zero->strVal = "h0";
  zero->width = 1;
  node->assignTree.clear();
  node->assignTree.push_back(new ExpTree(zero, node));
}

void removeDummyDim(graph* g) {
  /* remove dimensions of size 1 rom the array */
  std::map<Node*, std::vector<int>> arrayMap;
  for (auto iter : allSignals) {
    Node* node = iter.second;
    if (!node->isArray()) continue;
    std::vector<int> validIndex;
    std::vector<int> validDim;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      if (node->dimension[i] != 1) {
        validIndex.push_back(i);
        validDim.push_back(node->dimension[i]);
      }
    }
    if (validIndex.size() == node->dimension.size()) continue;
    arrayMap[node] = std::vector<int>(validIndex);

    /* array with only one element */
    if (validIndex.size() == 0) {
      node->dimension.clear();
      for (ExpTree* tree : node->arrayVal) {
        if (tree->getRoot()->opType != OP_WHEN) node->assignTree.clear();
        node->assignTree.push_back(tree);
      }
      node->arrayVal.clear();
    } else {
      node->dimension = std::vector<int>(validDim);
    }
  }
  std::set<ENode*> visited;
  for (auto iter : allSignals) {
    Node* node = iter.second;
    for (ExpTree* tree : node->arrayVal) tree->removeDummyDim(arrayMap, visited);
    for (ExpTree* tree : node->assignTree) tree->removeDummyDim(arrayMap, visited);
  }
  for (Node* mem : g->memory) {
    std::vector<int> validDim;
    for (size_t i = 0; i < mem->dimension.size(); i ++) {
      if (mem->dimension[i] != 1) {
        validDim.push_back((mem->dimension[i]));
      }
    }
    if (validDim.size() == mem->dimension.size()) continue;
    mem->dimension = std::vector<int>(validDim);
  }
  /* transform memory of depth 1 to register */
  for (Node* mem : g->memory) {
    if (mem->depth > 1) continue;
    if (mem->dimension.size() != 0) continue;
    /* TODO: all ports must share the same clock */
    Node* clock = nullptr;
    int readerCount = 0, writerCount = 0;
    for (Node* port : mem->member) {
      clock = port->clock;
      // Assert(!clock || clock == newClock, "memory %s has multiple clocks", mem->name.c_str());
      if (port->type == NODE_READER) readerCount ++;
      if (port->type == NODE_WRITER) writerCount ++;
    }
    Assert(writerCount == 1 && readerCount > 0, "memory %s has multiple writers", mem->name.c_str());
    /* creating registers */
    Node* regSrc = mem->dup(NODE_REG_SRC, mem->name);
    Node* regDst = regSrc->dup(NODE_REG_DST, mem->name + "$NEXT");
    regSrc->bindReg(regDst);
    g->addReg(regSrc);
    addSignal(regSrc->name, regSrc);
    addSignal(regDst->name, regDst);
    regSrc->clock = regDst->clock = clock;
    ENode* resetCond = new ENode(OP_INT);
    resetCond->width = 1;
    resetCond->strVal = "h0";
    regSrc->resetCond = new ExpTree(resetCond, regSrc);
    regSrc->resetVal = new ExpTree(new ENode(regSrc), regSrc);
    /* update all ports */
    for (Node* port : mem->member) {
      if (port->type == NODE_READER) {
        port->assignTree.clear();
        port->assignTree.push_back(new ExpTree(new ENode(regSrc), port));
        port->type = NODE_OTHERS;
      } else if (port->type == NODE_WRITER) {
        for (ExpTree* tree : port->assignTree) {
          writer2Reg(tree);
          regSrc->assignTree.push_back(tree);
        }
        for (ExpTree* tree : port->arrayVal) {
          writer2Reg(tree);
          regSrc->arrayVal.push_back(tree);
        }
        port->assignTree.clear();
      }
    }
      mem->status = DEAD_NODE;
  }
  g->memory.erase(
    std::remove_if(g->memory.begin(), g->memory.end(), [](const Node* n) {return n->status == DEAD_NODE; }),
    g->memory.end()
  );
}
