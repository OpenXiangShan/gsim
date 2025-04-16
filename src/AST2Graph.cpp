/*
  Generate design graph (the intermediate representation of the input circuit) from AST
*/


#include "common.h"
#include <stack>
#include <map>
#include <utility>

/* check the node type and children num */
#define TYPE_CHECK(node, min, max,...) typeCheck(node, (const int[]){__VA_ARGS__}, sizeof((const int[]){__VA_ARGS__}) / sizeof(int), min, max)
#define SEP_MODULE globalConfig.sep_module.c_str() // seperator for module
#define SEP_AGGR   globalConfig.sep_aggr.c_str()

#define MOD_IN(module) ((module == P_EXTMOD) ? NODE_EXT_IN : (module == P_MOD ? NODE_OTHERS : NODE_INP))
#define MOD_OUT(module) ((module == P_EXTMOD) ? NODE_EXT_OUT : (module == P_MOD ? NODE_OTHERS : NODE_OUT))
#define MOD_EXT_FLIP(type) (type == NODE_INP ? NODE_OUT : \
                           (type == NODE_OUT ? NODE_INP : \
                           (type == NODE_EXT_IN ? NODE_EXT_OUT : \
                           (type == NODE_EXT_OUT ? NODE_EXT_IN : type))))

int p_stoi(const char* str);
TypeInfo* visitType(graph* g, PNode* ptype, NodeType parentType);
ASTExpTree* visitExpr(graph* g, PNode* expr);
void visitStmts(graph* g, PNode* stmts);
void visitWhen(graph* g, PNode* when);
void removeDummyDim(graph* g);
ENode* getWhenEnode(ExpTree* valTree, int depth);
void fillEmptyWhen(ExpTree* newTree, ENode* oldNode);

/* map between module name and module pnode*/
static std::map<std::string, PNode*> moduleMap;
/* prefix trace. module1, module1$module2 module1$a_b_c...*/
static std::stack<std::string> prefixTrace;
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

static inline std::string prefix(std::string ch) {
  return prefixTrace.empty() ? "" : prefixTrace.top() + ch;
}

static inline std::string topPrefix() {
  Assert(!prefixTrace.empty(), "prefix is empty");
  return prefixTrace.top();
}

static inline std::string prefixName(std::string ch, std::string name) {
  return prefix(ch) + name;
}

static inline void prefix_append(std::string ch, std::string str) {
  prefixTrace.push(prefix(ch) + str);
}

static inline void prefix_pop() {
  prefixTrace.pop();
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
TypeInfo* visitField(graph* g, PNode* field, NodeType parentType) {
  NodeType fieldType = parentType;
  if (field->type == P_FLIP_FIELD) fieldType = MOD_EXT_FLIP(fieldType);
  prefix_append(SEP_AGGR, field->name);
  TypeInfo* info = visitType(g, field->getChild(0), fieldType);
  if (field->type == P_FLIP_FIELD) info->flip();
  prefix_pop();
  return info;
  
}

/*
fields:                 { $$ = new PList(); }
    | fields ',' field  { $$ = $1; $$->append($3); }
    | field             { $$ = new PList($1); }
*/
TypeInfo* visitFields(graph* g, PNode* fields, NodeType parentType) {
  TypeInfo* info = new TypeInfo();
  for (int i = 0; i < fields->getChildNum(); i ++) {
    PNode* field = fields->getChild(i);
    TypeInfo* fieldInfo = visitField(g, field, parentType);
    NodeType curType = parentType;
    if (field->type == P_FLIP_FIELD) {
      curType = MOD_EXT_FLIP(parentType);
    }
    if (!fieldInfo->isAggr()) { // The type of field is ground
      Node* fieldNode = allocNode(curType, prefixName(SEP_AGGR, field->name), fields->lineno);
      fieldNode->updateInfo(fieldInfo);
      fieldNode->inAggr = true;
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
TypeInfo* visitType(graph* g, PNode* ptype, NodeType parentType) {
  TYPE_CHECK(ptype, 0, INT32_MAX, P_AG_FIELDS, P_AG_ARRAY, P_Clock, P_INT_TYPE, P_ASYRESET, P_RESET);
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
    case P_RESET:
      info = new TypeInfo();
      info->set_sign(false); info->set_width(1);
      break;
    case P_ASYRESET:
      info = new TypeInfo();
      info->set_sign(false); info->set_width(1);
      info->set_reset(ASYRESET);
      break;
    case P_AG_FIELDS:
      info = visitFields(g, ptype, parentType);
      info->newParent(topPrefix());
      break;
    case P_AG_ARRAY:
      TYPE_CHECK(ptype, 1, 1, P_AG_ARRAY);
      info = visitType(g, ptype->getChild(0), parentType);
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
TypeInfo* visitPort(graph* g, PNode* port, PNodeType modType) {
  TYPE_CHECK(port, 1, 1, P_INPUT, P_OUTPUT);
  NodeType type = port->type == P_INPUT ? MOD_IN(modType) : MOD_OUT(modType);
  prefix_append(SEP_MODULE, port->name);
  TypeInfo* info = visitType(g, port->getChild(0), type);

  if (!info->isAggr()) {
    Node* node = allocNode(type, topPrefix(), port->lineno);
    node->updateInfo(info);
    info->add(node, false);
  }
  prefix_pop();
  return info;
}

/*
  ports:  { $$ = new PNode(P_PORTS); }
      | ports port    { $$ = $1; $$->appendChild($2); }
*/
void visitTopPorts(graph* g, PNode* ports) {
  TYPE_CHECK(ports, 0, INT32_MAX, P_PORTS);
  // visit all ports
  for (int i = 0; i < ports->getChildNum(); i ++) {
    PNode* port = ports->getChild(i);
    TypeInfo* info = visitPort(g, port, P_CIRCUIT);
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
ASTExpTree* visitIntNoInit(graph* g, PNode* expr) {
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
ASTExpTree* visitInvalid(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 0, 0, P_INVALID);
  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(OP_INVALID);
  return ret;
}
/*
| IntType width '(' INT ')' { $$ = newNode(P_EXPR_INT_INIT, synlineno(), $1, 0); $$->setWidth($2); $$->setSign($1[0] == 'S'); $$->appendExtraInfo($4);}
*/
ASTExpTree* visitIntInit(graph* g, PNode* expr) {
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
ASTExpTree* allocIndex(graph* g, PNode* expr) {
  ASTExpTree* exprTree = visitExpr(g, expr);
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
ASTExpTree* visitReference(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 0, INT32_MAX, P_REF);
  std::string name = prefixName(SEP_MODULE, expr->name);

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
      case P_REF_IDX_EXPR: ret->addChildSameTree(allocIndex(g, child->getChild(0))); break;
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
ASTExpTree* visitMux(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 3, 3, P_EXPR_MUX);

  ASTExpTree* cond = visitExpr(g, expr->getChild(0));
  ASTExpTree* left = visitExpr(g, expr->getChild(1));
  ASTExpTree* right = visitExpr(g, expr->getChild(2));

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
ASTExpTree* visit2Expr(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 2, 2, P_2EXPR);

  ASTExpTree* left = visitExpr(g, expr->getChild(0));
  ASTExpTree* right = visitExpr(g, expr->getChild(1));

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
ASTExpTree* visit1Expr(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

  ASTExpTree* ret = new ASTExpTree(false);
  ret->setOp(str2op_expr1(expr->name));
  ret->addChildTree(1, child);

  delete child;

  return ret;
}
/*
primop_1expr1int: E1I1OP expr ',' INT ')' { $$ = newNode(P_1EXPR1INT, synlineno(), $1, 1, $2); $$->appendExtraInfo($4); }
*/
ASTExpTree* visit1Expr1Int(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR1INT);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

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
ASTExpTree* visit1Expr2Int(graph* g, PNode* expr) {
  TYPE_CHECK(expr, 1, 1, P_1EXPR2INT);

  ASTExpTree* child = visitExpr(g, expr->getChild(0));

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
ASTExpTree* visitExpr(graph* g, PNode* expr) {
  ASTExpTree* ret;
  switch (expr->type) {
    case P_EXPR_INT_NOINIT: ret = visitIntNoInit(g, expr); break;
    case P_EXPR_INT_INIT: ret = visitIntInit(g, expr); break;
    case P_REF: ret = visitReference(g, expr); break;
    case P_EXPR_MUX: ret = visitMux(g, expr); break;
    case P_2EXPR: ret = visit2Expr(g, expr); break;
    case P_1EXPR: ret = visit1Expr(g, expr); break;
    case P_1EXPR1INT: ret = visit1Expr1Int(g, expr); break;
    case P_1EXPR2INT: ret = visit1Expr2Int(g, expr); break;
    case P_INVALID: ret = visitInvalid(g, expr); break;
    default: printf("Invalid type %d\n", expr->type); Panic();
  }
  return ret;
}
/*
module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
*/
void visitModule(graph* g, PNode* module) {
  TYPE_CHECK(module, 2, 2, P_MOD);
  // printf("visit module %s\n", module->name.c_str());

  PNode* ports = module->getChild(0);
  for (int i = 0; i < ports->getChildNum(); i ++) {
    TypeInfo* portInfo = visitPort(g, ports->getChild(i), P_MOD);

    for (auto entry : portInfo->aggrMember) {
      Node* node = entry.first;
      addSignal(node->name, node);
    }
    for (AggrParentNode* dummy : portInfo->aggrParent) {
      addDummy(dummy->name, dummy);
    }
  }

  visitStmts(g, module->getChild(1));
  // printf("leave module %s\n", module->name.c_str());
}
/*
extmodule: Extmodule ALLID ':' info INDENT ports ext_defname params DEDENT  { $$ = newNode(P_EXTMOD, synlineno(), $4, $2, 1, $6); $$->appendChildList($8);}
*/
void visitExtModule(graph* g, PNode* module) {
  TYPE_CHECK(module, 1, 1, P_EXTMOD);

  if (module->getExtra(0) == CLOCK_GATE_NAME) { // ClockGate
    PNode* ports = module->getChild(0);
    Assert(ports->getChildNum() == 4, "invalid childNum %d for clock gate", ports->getChildNum());
    Assert(ports->getChild(0)->name == "TE" && ports->getChild(1)->name == "E" && ports->getChild(2)->name == "CK" && ports->getChild(3)->name == "Q", "invalid clockgate ports");
    auto addGatePort = [&](std::string name, bool isClock) {
      Node* node = allocNode(NODE_OTHERS, prefixName(SEP_MODULE, name), module->lineno);
      node->width = 1; node->sign = false; node->isClock = isClock;
      addSignal(node->name, node);
      return node;
    };
    Node* Q = addGatePort("Q", true);
    Node* CK = addGatePort("CK", true);
    Node* E = addGatePort("E", false);
    Node* TE = addGatePort("TE", false);
    TE->width = 1; TE->sign = false; TE->isClock = false;
    ENode* gateAnd = new ENode(OP_AND);
    ENode* gateOr = new ENode(OP_OR);
    gateOr->addChild(new ENode(E));
    gateOr->addChild(new ENode(TE));
    gateAnd->addChild(new ENode(CK));
    gateAnd->addChild(gateOr);
    Q->valTree = new ExpTree(gateAnd, Q);
  } else {
    Node* extNode = allocNode(NODE_EXT, module->name, module->lineno);
    extNode->extraInfo = module->getExtra(0);
    addSignal(extNode->name, extNode);
    PNode* ports = module->getChild(0);
    for (int i = 0; i < ports->getChildNum(); i ++) {
      TypeInfo* portInfo = visitPort(g, ports->getChild(i), P_EXTMOD);
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

}

/*
statement: Wire ALLID ':' type info    { $$ = newNode(P_WIRE_DEF, $4->lineno, $5, $2, 1, $4); }
*/
void visitWireDef(graph* g, PNode* wire) {
  TYPE_CHECK(wire, 1, 1, P_WIRE_DEF);

  prefix_append(SEP_MODULE, wire->name);

  TypeInfo* info = visitType(g, wire->getChild(0), NODE_OTHERS);

  for (auto entry : info->aggrMember) {
    addSignal(entry.first->name, entry.first);
  }
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  if (!info->isAggr()) {
    Node* node = allocNode(NODE_OTHERS, topPrefix(), wire->lineno);
    node->updateInfo(info);
    addSignal(node->name, node);
  }

  prefix_pop();
}

/*
statement: Reg ALLID ':' type ',' expr(1) RegWith INDENT RegReset '(' expr ',' expr ')' info DEDENT { $$ = newNode(P_REG_DEF, $4->lineno, $15, $2, 4, $4, $6, $11, $13); }
expr(1) must be clock
*/
void visitRegDef(graph* g, PNode* reg, PNodeType type) {
  TYPE_CHECK(reg, 2, 4, P_REG_DEF, P_REG_RESET_DEF);

  ASTExpTree* clkExp = visitExpr(g, reg->getChild(1));
  Assert(!clkExp->isAggr() && clkExp->getExpRoot()->getNode(), "unsupported clock in lineno %d", reg->lineno);
  Node* clockNode = clkExp->getExpRoot()->getNode();

  prefix_append(SEP_MODULE, reg->name);
  TypeInfo* info = visitType(g, reg->getChild(0), NODE_REG_SRC);
  /* alloc node for basic nodes */
  if (!info->isAggr()) {
    Node* src = allocNode(NODE_REG_SRC, topPrefix(), reg->lineno);
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

    if (type == P_REG_RESET_DEF) continue;

    // Fake reset
    ENode* cond = allocIntEnode(1, "0");

    src->resetCond = new ExpTree(cond, src);
    src->resetVal = new ExpTree(new ENode(src), src);
  }
  // only src dummy nodes are in allDummy
  for (AggrParentNode* dummy : info->aggrParent) addDummy(dummy->name, dummy);
  
  prefix_pop();

  if (type == P_REG_DEF) return;
  
  ASTExpTree* resetCond = visitExpr(g, reg->getChild(2));
  ASTExpTree* resetVal = visitExpr(g, reg->getChild(3));
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
static inline TypeInfo* visitMemType(graph* g, PNode* dataType) {
  TYPE_CHECK(dataType, 1, 1, P_DATATYPE);
  return visitType(g, dataType->getChild(0), NODE_INVALID);
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

static inline Node* add_member(Node* parent, std::string name, int idx, int width, int sign) {
  Node* member = allocNode(NODE_MEM_MEMBER, prefixName(SEP_AGGR, name));
  member->setType(width, sign);
  parent->set_member(idx, member);
  return member;
}
/*
mem_reader Reader "=>" ALLID  { $$ = $1; $$->append(newNode(P_READER, synlineno(), $4, 0));}
*/
static inline Node* visitReader(PNode* reader, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(reader, 0, 0, P_READER);

  prefix_append(SEP_MODULE, reader->name);

  Node* ret = allocNode(NODE_READER, topPrefix(), reader->lineno);

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

  prefix_pop();

  return ret;
}
/*
mem_writer Writer "=>" ALLID    { $$ = $1; $$->append(newNode(P_WRITER, synlineno(), $4, 0));}
*/
static inline Node* visitWriter(PNode* writer, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(writer, 0, 0, P_WRITER);

  prefix_append(SEP_MODULE, writer->name);

  Node* ret = allocNode(NODE_WRITER, topPrefix(), writer->lineno);

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

  prefix_pop();

  return ret;
}
/*
mem_readwriter Readwriter "=>" ALLID  { $$ = $1; $$->append(newNode(P_READWRITER, synlineno(), $4, 0));}
*/
static inline Node* visitReadWriter(PNode* readWriter, int width, int depth, bool sign, std::string suffix, Node* node) {
  TYPE_CHECK(readWriter, 0, 0, P_READWRITER);

  prefix_append(SEP_MODULE, readWriter->name);

  Node* ret = allocNode(NODE_READWRITER, topPrefix(), readWriter->lineno);

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

  prefix_pop();

  return ret;
}

Node* visitMemPort(PNode* port, int depth, int width, bool sign, std::string suffix, Node* node) {
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
void visitMemory(graph* g, PNode* mem) {
  TYPE_CHECK(mem, 5, INT32_MAX, P_MEMORY);
  prefix_append(SEP_MODULE, mem->name);
  TypeInfo* info = visitMemType(g, mem->getChild(0));
  int depth = visitMemDepth(mem->getChild(1));
  int rlatency = visitReadLatency(mem->getChild(2));
  int wlatency = visitWriteLatency(mem->getChild(3));
  visitRUW(mem->getChild(4));

  moduleInstances.insert(topPrefix());

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
      // AggrParentNode* parent = new AggrParentNode(prefixName(SEP_AGGR, mem->getChild(i)->name));
      /* data & mask is unused */
      Node* originPort = visitMemPort(mem->getChild(i), depth, 0, 0, "", NULL);
      addOriginMember(originPort, mem->getChild(i));
      std::map<Node*, Node*>allSubPort;
      /* create all ports */
      for (auto node : allSubMem) {
        std::string suffix = replacePrefix(topPrefix(), "", node->name).c_str();
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
        std::string suffix = replacePrefix(topPrefix(), "", parent->name);
        PNode* memPort = mem->getChild(i);
        if (memPort->type == P_READER) { // data only
          AggrParentNode* portAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "data" + suffix));
          for (auto entry : parent->member) {
            portAggr->addMember(allSubPort[entry.first]->get_member(READER_DATA), entry.second);
          }
          addDummy(portAggr->name, portAggr);

        } else if (memPort->type == P_WRITER) {
          AggrParentNode* maskAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "mask" + suffix));
          AggrParentNode* dataAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "data" + suffix));
          for (auto entry : parent->member) {
            maskAggr->addMember(allSubPort[entry.first]->get_member(WRITER_MASK), entry.second);
            dataAggr->addMember(allSubPort[entry.first]->get_member(WRITER_DATA), entry.second);
          }
          addDummy(maskAggr->name, maskAggr);
          addDummy(dataAggr->name, dataAggr);

        } else if (memPort->type == P_READWRITER) {
          AggrParentNode* wmaskAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "wmask" + suffix));
          AggrParentNode* rdataAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "rdata" + suffix));
          AggrParentNode* wdataAggr = new AggrParentNode(prefixName(SEP_MODULE, memPort->name + SEP_AGGR + "wdata" + suffix));
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
    Node* memNode = allocNode(NODE_MEMORY, topPrefix(), mem->lineno);
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
  prefix_pop();

}

static Node* allocAddrNode(graph* g, Node* port) {
  ENode* addr_enode = port->memTree->getRoot()->getChild(0);
  Node* addr_src = allocNode(NODE_REG_SRC, format("%s%s%s", port->name.c_str(), SEP_AGGR, "ADDR"), port->lineno);
  g->addReg(addr_src);
  Node* addr_dst = addr_src->dup();
  addr_dst->type = NODE_REG_DST;
  addr_dst->name += format("%s%s", SEP_AGGR, "NEXT");
  addSignal(addr_src->name, addr_src);
  addSignal(addr_dst->name, addr_dst);
  addr_src->bindReg(addr_dst);
  addr_src->clock = addr_dst->clock = port->clock;
  addr_src->valTree = new ExpTree(addr_enode, addr_src);
  ENode* resetCond = allocIntEnode(1, "0");
  addr_src->resetCond = new ExpTree(resetCond, addr_src);
  addr_src->resetVal = new ExpTree(new ENode(addr_src), addr_src);
  return addr_src;
}

static Node* visitChirrtlPort(graph* g, PNode* port, int width, int depth, bool sign, std::string suffix, Node* node, ENode* addr_enode, Node* clock_node) {
  assert(port->type == P_READ || port->type == P_WRITE || port->type == P_INFER || port->type == P_READWRITER);
  // Add prefix port name
  prefix_append(SEP_MODULE, port->name);
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
  } else if (port->type == P_READWRITER) {
    type = NODE_READWRITER;
    op = OP_READ_MEM;
  } else TODO();

  Node* ret = allocNode(type, topPrefix() + suffix, port->lineno);
  ret->dimension = node->dimension;
  ENode* enode = new ENode(op);
  enode->memoryNode = node;
  enode->addChild(addr_enode);
  ret->memTree = new ExpTree(enode, ret);
  ret->clock = clock_node;

  prefix_pop();
  return ret;
}

static void visitChirrtlMemPort(graph* g, PNode* port) {
  // If we are in the top module, 
  //    the memory name does not need to have the prefix added.
  ASTExpTree* addr = visitExpr(g, port->getChild(0));
  ENode* addr_enode = addr->getExpRoot();
  ASTExpTree* clock = visitExpr(g, port->getChild(1));
  Assert(clock->getExpRoot()->getNode() && !clock->getExpRoot()->getNode()->isArray(), "unsupported clock in lineno %d", port->lineno);
  Node* clock_node = clock->getExpRoot()->getNode();
  std::string memName = prefixName(SEP_MODULE, port->getExtra(0));

  Assert(memoryMap.find(memName) != memoryMap.end(), "Could not find memory: %s", memName.c_str());
  std::vector<Node*> memoryMembers;
  for (Node* mem : memoryMap[memName].first) {
    int depth = mem->depth;
    bool sign = mem->sign;
    int width = mem->width;
    std::string suffix = replacePrefix(memName, "", mem->name).c_str();
    Node* portNode = visitChirrtlPort(g, port, width, depth, sign, suffix, mem, addr_enode, clock_node);
    mem->add_member(portNode);
    addSignal(portNode->name, portNode);
    memoryMembers.push_back(portNode);
  }

  for (AggrParentNode* memParent : memoryMap[memName].second) {
    std::string suffix = replacePrefix(memName, "", memParent->name).c_str();
    AggrParentNode* parent = new AggrParentNode(prefixName(SEP_MODULE, port->name) + suffix);
    for (auto entry : memParent->member) {
      std::string memberSuffix = replacePrefix(memName, "", entry.first->name).c_str();
      Node* memberNode = getSignal(prefixName(SEP_MODULE, port->name) + memberSuffix);
      parent->addMember(memberNode, entry.second);
    }
    addDummy(parent->name, parent);
  }
}

// TODO: Comb memory support
static void visitChirrtlMemory(graph* g, PNode* mem) {
  assert(mem->type == P_SEQ_MEMORY || mem->type == P_COMB_MEMORY);
  prefix_append(SEP_MODULE, mem->name);
  bool isSeq = mem->type == P_SEQ_MEMORY;

  // Transform the vector type into memory type
  // UInt<32>[1][2]
  //    =>
  //        type : UInt<32>[1]
  //        depth: 2
  TypeInfo* type = visitMemType(g, mem->getChild(0));
  int depth = type->dimension.front();
  type->dimension.erase(type->dimension.begin());
  // Convert to firrtl memory
  int rlatency = isSeq;
  int wlatency = 1;
  std::string ruw = "undefined";
  if (isSeq && mem->getExtraNum() > 0) {
    ruw = mem->getExtra(0);
  }

  moduleInstances.insert(topPrefix());

  if (type->isAggr()) {
    memoryMap[topPrefix()] = std::make_pair(std::vector<Node*>(), std::vector<AggrParentNode*>());
    for (auto& entry : type->aggrMember) {
      entry.first->dimension.erase(entry.first->dimension.begin());
      auto* node = entry.first;
      node->type = NODE_MEMORY;
      node->extraInfo = ruw;
      memoryMap[topPrefix()].first.push_back(node);
      g->memory.push_back(node);
      node->set_memory(depth, rlatency, wlatency);
    }
    memoryMap[topPrefix()].second = type->aggrParent;
  } else {
    auto* memNode = allocNode(NODE_MEMORY, topPrefix(), mem->lineno);
    memNode->extraInfo = ruw;
    memoryMap[topPrefix()] = std::make_pair(std::vector<Node*>{memNode}, std::vector<AggrParentNode*>());
    g->memory.push_back(memNode);
    memNode->updateInfo(type);
    memNode->set_memory(depth, rlatency, wlatency);
  }

  prefix_pop();
}

/*
| Inst ALLID Of ALLID info    { $$ = newNode(P_INST, synlineno(), $5, $2, 0); $$->appendExtraInfo($4); }
*/
void visitInst(graph* g, PNode* inst) {
  TYPE_CHECK(inst, 0, 0, P_INST);
  Assert(inst->getExtraNum() >= 1 && moduleMap.find(inst->getExtra(0)) != moduleMap.end(),
               "Module %s is not defined!\n", inst->getExtra(0).c_str());
  PNode* module = moduleMap[inst->getExtra(0)];
  prefix_append(SEP_MODULE, inst->name);
  moduleInstances.insert(topPrefix());
  switch(module->type) {
    case P_MOD: visitModule(g, module); break;
    case P_EXTMOD: visitExtModule(g, module); break;
    case P_INTMOD: TODO();
    default:
      Panic();
  }
  prefix_pop();
}

AggrParentNode* allocNodeFromAggr(graph* g, AggrParentNode* parent) {
  AggrParentNode* ret = new AggrParentNode(topPrefix());
  std::string oldPrefix = parent->name;
  /* alloc all real nodes */
  for (auto entry : parent->member) {
    Node* member = entry.first;
    std::string name = replacePrefix(oldPrefix, topPrefix(), member->name);
    /* the type of parent can be registers, thus the node->type cannot set to member->type */
    Node* node = member->dup(NODE_OTHERS, name); // SEP_AGGR is already in name
  
    addSignal(node->name, node);
    ret->addMember(node, entry.second);
  }
  /* alloc all dummy nodes, and connect them to real nodes stored in allSignals */
  for (AggrParentNode* aggrMember : parent->parent) {
    // create new aggr node
    AggrParentNode* aggrNode = new AggrParentNode(replacePrefix(oldPrefix, topPrefix(), aggrMember->name));
    // update member and parent in new aggrNode
    for (auto entry : aggrMember->member) {
      aggrNode->addMember(getSignal(replacePrefix(oldPrefix, topPrefix(), entry.first->name)), entry.second);
    }
    // the children of aggrMember are earlier than it
    for (AggrParentNode* parent : aggrMember->parent) {
      aggrNode->addParent(getDummy(replacePrefix(oldPrefix, topPrefix(), parent->name)));
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
void visitNode(graph* g, PNode* node) {
  TYPE_CHECK(node, 1, 1, P_NODE);
  ASTExpTree* exp = visitExpr(g, node->getChild(0));
  prefix_append(SEP_MODULE, node->name);
  if (exp->isAggr()) {// create all nodes in aggregate
    AggrParentNode* aggrNode = allocNodeFromAggr(g, exp->getParent());
    Assert(aggrNode->size() == exp->getAggrNum(), "aggrMember num %d tree num %d", aggrNode->size(), exp->getAggrNum());
    for (int i = 0; i < aggrNode->size(); i ++) {
      aggrNode->member[i].first->valTree = new ExpTree(exp->getAggr(i), aggrNode->member[i].first);
      aggrNode->member[i].first->dimension.clear();
      std::vector<int> dims = exp->getAggr(i)->getDim();
      aggrNode->member[i].first->dimension.insert(aggrNode->member[i].first->dimension.end(), dims.begin(), dims.end());
    }
    addDummy(aggrNode->name, aggrNode);
  } else {
    Node* n = allocNode(NODE_OTHERS, topPrefix(), node->lineno);
    std::vector<int> dims = exp->getExpRoot()->getDim();
    n->dimension.insert(n->dimension.end(), dims.begin(), dims.end());

    n->valTree = new ExpTree(exp->getExpRoot(), n);
    addSignal(n->name, n);
  }
  prefix_pop();
}
/*
| reference "<=" expr info  { $$ = newNode(P_CONNECT, $1->lineno, $4, NULL, 2, $1, $3); }
*/
void visitConnect(graph* g, PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(g, connect->getChild(0));
  ASTExpTree* exp = visitExpr(g, connect->getChild(1));

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

void visitPartialConnect(graph* g, PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_PAR_CONNECT);
  ASTExpTree* ref = visitReference(g, connect->getChild(0));
  ASTExpTree* exp = visitExpr(g, connect->getChild(1));
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
  Assert(enode->getChildNum() == 3, "invalid child num %ld", enode->getChildNum());
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
std::pair<ExpTree*, ENode*> growWhenTrace(Node* node, ExpTree* valTree, size_t depth) {
  ENode* oldParent = nullptr;
  size_t maxDepth = depth;
  if (valTree) std::tie(oldParent, maxDepth) = getDeepestWhen(valTree, depth);
  if (maxDepth == whenTrace.size()) {
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
  for (size_t depth = whenTrace.size() - 1; depth >= maxDepth ; depth --) {
    ENode* whenNode = new ENode(OP_WHEN); // return the deepest whenNode
    if (depth == whenTrace.size() - 1) retENode = whenNode;
    ENode* condNode = new ENode(whenTrace[depth].second);
    childRoot = childRoot ? childRoot->dup() : childRoot;
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
    if (depth == 0) break;
  }
  if (!oldRoot) {
    if (maxDepth == depth) { // depth: depth the node defined; maxDepth: deepest when that matches valTree
      if (valTree) valTree->setRoot(newRoot);
      else valTree = new ExpTree(newRoot);
    } else {
      oldParent->setChild(whenTrace[maxDepth-1].first ? 1 : 2, newRoot);
    }
  } else {
    if (maxDepth == depth) {
      if (valTree) node->assignTree.push_back(valTree);
      valTree = new ExpTree(newRoot);
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

void whenConnect(graph* g, Node* node, ENode* lvalue, ENode* rvalue, PNode* connect) {
  if (!node) {
    printf("connect lineno %d\n", connect->lineno);
    TODO();
  }
  ExpTree* valTree = nullptr;
  ENode* whenNode;
  size_t connectDepth = (node->type == NODE_WRITER || node->type == NODE_INFER || node->type == NODE_READWRITER) ? 0 : node->whenDepth;
  if (node->isArray()) {
    std::tie(valTree, whenNode) = growWhenTrace(node, nullptr, connectDepth);
  } else {
    std::tie(valTree, whenNode) = growWhenTrace(node, node->valTree, connectDepth);
  }
  valTree->setlval(lvalue);
  if (node->type == NODE_WRITER || node->type == NODE_INFER || node->type == NODE_READWRITER) {
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
void visitWhenConnect(graph* g, PNode* connect) {
  TYPE_CHECK(connect, 2, 2, P_CONNECT);
  ASTExpTree* ref = visitReference(g, connect->getChild(0));
  ASTExpTree* exp = visitExpr(g, connect->getChild(1));
  Assert(!(ref->isAggr() ^ exp->isAggr()), "type not match, ref aggr %d exp aggr %d", ref->isAggr(), exp->isAggr());

  if (ref->isAggr()) {
    for (int i = 0; i < ref->getAggrNum(); i++) {
      if (exp->getFlip(i)) {
        Node* node = exp->getAggr(i)->getNode();
        stmtsNodes.insert(node);
        whenConnect(g, node, exp->getAggr(i), ref->getAggr(i), connect);
      } else {
        Node* node = ref->getAggr(i)->getNode();
        stmtsNodes.insert(node);
        whenConnect(g, node, ref->getAggr(i), exp->getAggr(i), connect);
      }
    }
  } else {
    Node* node = ref->getExpRoot()->getNode();
    stmtsNodes.insert(node);
    whenConnect(g, node, ref->getExpRoot(), exp->getExpRoot(), connect);
  }
}
/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void visitWhenPrintf(graph* g, PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, "PRINTF_" + std::to_string(print->lineno)), print->lineno);
  ASTExpTree* exp = visitExpr(g, print->getChild(1)); // cond

  ENode* expRoot = exp->getExpRoot();

  ExpTree* valTree = nullptr;
  ENode* whenNode;
  std::tie(valTree, whenNode) = growWhenTrace(n, nullptr, 0);

  ENode* printENode = new ENode(OP_PRINTF);
  printENode->strVal = print->getExtra(0);
  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(g, exprs->getChild(i));
    printENode->addChild(val->getExpRoot());
  }

  ENode* when = new ENode(OP_WHEN);
  when->addChild(expRoot);
  when->addChild(printENode);
  when->addChild(nullptr);

  whenNode->setChild(whenTrace.back().first ? 1 : 2, when);

  valTree->setlval(new ENode(n));
  n->valTree = valTree;
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

void visitWhenAssert(graph* g, PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, ass->name), ass->lineno);

  ASTExpTree* pred = visitExpr(g, ass->getChild(1));
  ASTExpTree* en = visitExpr(g, ass->getChild(2));

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

void visitWhenStop(graph* g, PNode* stop) {
  TYPE_CHECK(stop, 2, 2, P_STOP);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, stop->name), stop->lineno);

  ASTExpTree* exp = visitExpr(g, stop->getChild(1));

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
void visitWhenStmt(graph* g, PNode* stmt) {
  switch (stmt->type) {
    case P_NODE: visitNode(g, stmt); break; // local nodes
    case P_CONNECT: visitWhenConnect(g, stmt); break;
    case P_WHEN: visitWhen(g, stmt); break;
    case P_WIRE_DEF: visitWireDef(g, stmt); break;
    case P_PRINTF: visitWhenPrintf(g, stmt); break;
    case P_ASSERT: visitWhenAssert(g, stmt); break;
    case P_STOP: visitWhenStop(g, stmt); break;
    case P_INST: visitInst(g, stmt); break;
    case P_REG_DEF: visitRegDef(g, stmt, P_REG_DEF); break;
    case P_REG_RESET_DEF: visitRegDef(g, stmt, P_REG_RESET_DEF); break;
    case P_READ :
    case P_WRITE :
    case P_READWRITER:
    case P_INFER : visitChirrtlMemPort(g, stmt); break;
    default: printf("Invalid type %d %d\n", stmt->type, stmt->lineno); Panic();
  }
}
void visitWhenStmts(graph* g, PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitWhenStmt(g, stmts->getChild(i));
  }
}

Node* allocCondNode(ASTExpTree* condExp, PNode* when) {
  Node* cond = allocNode(NODE_OTHERS, prefixName(SEP_MODULE, "WHEN_COND_" + std::to_string(when->lineno)), when->lineno);
  cond->valTree = new ExpTree(condExp->getExpRoot(), cond);
  addSignal(cond->name, cond);
  return cond;
}

/*
| When expr ':' info INDENT statements DEDENT when_else   { $$ = newNode(P_WHEN, $2->lineno, $4, NULL, 3, $2, $6, $8); }
*/
void visitWhen(graph* g, PNode* when) {
  TYPE_CHECK(when, 3, 3, P_WHEN);
  ASTExpTree* condExp = visitExpr(g, when->getChild(0));
  Node* condNode = allocCondNode(condExp, when);
  // allocWhenId(when); distinguish when through condNode rather than id
  whenTrace.push_back(std::make_pair(true, condNode));
  visitWhenStmts(g, when->getChild(1));
  
  whenTrace.back().first = false;
  visitWhenStmts(g, when->getChild(2));
  
  whenTrace.pop_back();

}

/*
  | Printf '(' expr ',' expr ',' String exprs ')' ':' ALLID info { $$ = newNode(P_PRINTF, synlineno(), $12, $11, 3, $3, $5, $8); $$->appendExtraInfo($7); }
  | Printf '(' expr ',' expr ',' String exprs ')' info    { $$ = newNode(P_PRINTF, synlineno(), $10, NULL, 3, $3, $5, $8); $$->appendExtraInfo($7); }
*/
void visitPrintf(graph* g, PNode* print) {
  TYPE_CHECK(print, 3, 3, P_PRINTF);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, print->name), print->lineno);
  ASTExpTree* exp = visitExpr(g, print->getChild(1));

  ENode* enode = new ENode(OP_PRINTF);
  enode->strVal = print->getExtra(0);

  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i ++) {
    ASTExpTree* val = visitExpr(g, exprs->getChild(i));
    enode->addChild(val->getExpRoot());
  }

  ENode* whenENode = new ENode(OP_WHEN);
  whenENode->addChild(exp->getExpRoot());
  whenENode->addChild(enode);
  whenENode->addChild(nullptr);

  n->valTree = new ExpTree(whenENode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}
/*
  | Stop '(' expr ',' expr ',' INT ')' info   { $$ = newNode(P_STOP, synlineno(), $9, NULL, 2, $3, $5); $$->appendExtraInfo($7); }
  | Stop '(' expr ',' expr ',' INT ')' ':' ALLID info   { $$ = newNode(P_STOP, synlineno(), $11, $10, 2, $3, $5); $$->appendExtraInfo($7); }
*/
void visitStop(graph* g, PNode* stop) {
  TYPE_CHECK(stop, 2, 2, P_STOP);

  ASTExpTree* exp = visitExpr(g, stop->getChild(1));
  ENode* enode = new ENode(OP_EXIT);
  enode->addChild(exp->getExpRoot());
  enode->strVal = stop->getExtra(0);

  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, stop->name), stop->lineno);
  n->valTree = new ExpTree(enode, new ENode(n));
  addSignal(n->name, n);
  g->specialNodes.push_back(n);
}

/*
    | Assert '(' expr ',' expr ',' expr ',' String ')' ':' ALLID info { $$ = newNode(P_ASSERT, synlineno(), $13, $12, 3, $3, $5, $7); $$->appendExtraInfo($9); }
    | Assert '(' expr ',' expr ',' expr ',' String ')' info { $$ = newNode(P_ASSERT, synlineno(), $11, NULL, 3, $3, $5, $7); $$->appendExtraInfo($9); }
*/
void visitAssert(graph* g, PNode* ass) {
  TYPE_CHECK(ass, 3, 3, P_ASSERT);
  Node* n = allocNode(NODE_SPECIAL, prefixName(SEP_MODULE, ass->name), ass->lineno);

  ASTExpTree* pred = visitExpr(g, ass->getChild(1));
  ASTExpTree* en = visitExpr(g, ass->getChild(2));

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
      } else if (countEmptyWhen(node->valTree) != 0) {
        node->assignTree.push_back(node->valTree);
        node->valTree = nullptr;
      } else if (countEmptyWhen(node->valTree) == 0 && node->valTree->getRoot()->opType != OP_INVALID) {
        node->assignTree.clear();
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
void visitStmt(graph* g, PNode* stmt) {
  stmtsNodes.clear();
  switch (stmt->type) {
    case P_WIRE_DEF: visitWireDef(g, stmt); break;
    case P_REG_DEF: visitRegDef(g, stmt, P_REG_DEF); break;
    case P_REG_RESET_DEF: visitRegDef(g, stmt, P_REG_RESET_DEF); break;
    case P_INST: visitInst(g, stmt); break;
    case P_MEMORY: visitMemory(g, stmt); break;
    case P_SEQ_MEMORY : visitChirrtlMemory(g, stmt); break;
    case P_COMB_MEMORY: visitChirrtlMemory(g, stmt); break;
    case P_READ:
    case P_WRITE:
    case P_INFER: visitChirrtlMemPort(g, stmt); break;
    case P_NODE: visitNode(g, stmt); break;
    case P_CONNECT: visitConnect(g, stmt); break;
    case P_PAR_CONNECT: visitPartialConnect(g, stmt); break;
    case P_WHEN:
      visitWhen(g, stmt);
      saveWhenTree();
      break;
    case P_PRINTF: visitPrintf(g, stmt); break;
    case P_ASSERT: visitAssert(g, stmt); break;
    case P_STOP: visitStop(g, stmt); break;
    default:
      printf("invalid stmt type %d in lineno %d\n", stmt->type, stmt->lineno);
      Panic();
  }
}

/*
statements: { $$ = new PNode(P_STATEMENTS, synlineno()); }
    | statements statement { $$ =  $1; $1->appendChild($2); }
*/
void visitStmts(graph* g, PNode* stmts) {
  TYPE_CHECK(stmts, 0, INT32_MAX, P_STATEMENTS);
  for (int i = 0; i < stmts->getChildNum(); i ++) {
    visitStmt(g, stmts->getChild(i));
  }
}

/*
  module: Module ALLID ':' info INDENT ports statements DEDENT { $$ = newNode(P_MOD, synlineno(), $4, $2, 2, $6, $7); }
  children: ports, statments
*/
void visitTopModule(graph* g, PNode* topModule) {
  TYPE_CHECK(topModule, 2, 2, P_MOD);
  visitTopPorts(g, topModule->getChild(0));
  visitStmts(g, topModule->getChild(1));
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
  graph* g = new graph();
  g->name = root->name;

  PNode* topModule = NULL;

  for (int i = 0; i < root->getChildNum(); i++) {
    PNode* module = root->getChild(i);
    if (module->name == root->name) { topModule = module; }
    moduleMap[module->name] = module;
  }
  Assert(topModule, "Top module can not be NULL\n");
  visitTopModule(g, topModule);

/* infer memory port */

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
  }

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    Node* node = it->second;
    if (node->type == NODE_INFER || node->type == NODE_READER) {
      if (node->parent->rlatency == 1) {
        Node* addrReg = allocAddrNode(g, node);
        node->memTree->getRoot()->setChild(0, new ENode(addrReg));
      }
      node->memTree->getRoot()->opType = OP_READ_MEM;
      node->valTree = node->memTree;
      node->set_reader();
    }
    if (node->valTree) {
      node->assignTree.push_back(node->valTree);
      node->valTree = nullptr;
    }
    if (node->type == NODE_READWRITER) {
      if (node->parent->rlatency == 1) {
        Node* addrReg = allocAddrNode(g, node);
        node->memTree->getRoot()->setChild(0, new ENode(addrReg));
      }

      if (node->parent->extraInfo == "new") {
        node->assignTree.push_back(node->memTree);
      } else {
        node->assignTree.insert(node->assignTree.begin(), node->memTree);
      }
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

    reg->addReset();
    reg->addUpdateTree();
  }
  g->clockOptimize(allSignals);

  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    it->second->invalidArrayOptimize();
  }
  for (auto it = allSignals.begin(); it != allSignals.end(); it ++) {
    updatePrevNext(it->second);
    if (it->second->type == NODE_REG_SRC) it->second->updateDep();
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
    if ((it.second->type == NODE_OTHERS || it.second->type == NODE_READER || it.second->type == NODE_WRITER || it.second->type == NODE_READWRITER ||
        it.second->type == NODE_SPECIAL || it.second->type == NODE_EXT || it.second->type == NODE_EXT_IN || it.second->type == NODE_EXT_OUT)
        && it.second->super->prev.size() == 0) {
      g->supersrc.push_back(it.second->super);
    }
    if (it.second->isArray()) {
      for (ExpTree* tree : it.second->assignTree) {
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

void changeName(std::string oldName, std::string newName) {
  Assert(allSignals.find(oldName) != allSignals.end(), "signal %s not found", oldName.c_str());
  allSignals[oldName]->name = newName;
  allSignals[newName] = allSignals[oldName];
  allSignals.erase(oldName);
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
      for (size_t i = 0; i < top->getChildNum(); i ++) {
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
        size_t validIdx = arrayMap[top->getNode()][i];
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
    } else {
      node->dimension = std::vector<int>(validDim);
    }
  }
  std::set<ENode*> visited;
  for (auto iter : allSignals) {
    Node* node = iter.second;
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
    ENode* resetCond = allocIntEnode(1, "0");
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
