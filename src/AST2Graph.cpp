/**
 * @file
 * @brief 从 AST 转电路图
 */

#include <map>
#include <vector>

#include "common.h"
#include "graph.h"

#define SET_TYPE(x, y)   \
  do {                   \
    x->width = y->width; \
    x->sign  = y->sign;  \
    x->aggrType = y->aggrType; \
  } while (0)

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

void visitExpr(std::string prefix, Node* n, PNode* expr, bool connectRecursive);
void visitType(Node* n, PNode* ptype);
AggrType* parseAggr(std::string prefix, PNode* expr);
int getUpdateArrayIdx(std::vector<Index>index, Node* node);

int p_stoi(const char* str);
std::string cons2str(std::string s);

static inline Node* allocNode(int type = NODE_OTHERS) {
  Node* node = new Node(type);
  node->value = new ExprValue();
  node->workingVal = node->value;
  return node;
}

#define memory_member(str, _parent, w)              \
  do {                                             \
    Node* rn_##str = allocNode(NODE_MEMBER);        \
    _parent->member.push_back(rn_##str);            \
    rn_##str->parent = _parent;                    \
    rn_##str->name    = _parent->name + "$" + #str; \
    rn_##str->width   = w;                         \
    addSignal(rn_##str->name, rn_##str);           \
  } while (0)

void visitWhen(std::string prefix, graph* g, PNode* when, Node* cond, std::set<Node*>& allNodes);
void createMemberNodes(Node* node);
void aggrAdd(std::vector<Node*>& dst, Node* node);
void aggrAddLeaf(std::vector<Node*>&dst, Node* node);
void connectAdd(std::set<Node*>& dstSet, Node* node, int num);
void connectAggrAdd(std::set<Node*>& dstSet, Node* node, Node* rightNode, int num);

int log2(int x) { return 31 - __builtin_clz(x); }

int p2(int x) { return 1 << (32 - __builtin_clz(x - 1)); }

static int maxWidth(int a, int b, bool sign = 0) { return MAX(a, b); }

static int minWidth(int a, int b, bool sign = 0) { return MIN(a, b); }

static int maxWidthPlus1(int a, int b, bool sign = 0) { return MAX(a, b) + 1; }

static int sumWidth(int a, int b, bool sign = 0) { return a + b; }

static int minusWidthPos(int a, int b, bool sign = 0) { return MAX(1, a - b); }

static int minusWidth(int a, int b, bool sign = 0) { return a - b; }

static int divWidth(int a, int b, bool sign = 0) { return sign ? a + 1 : a; }

static int cvtWidth(int a, int b, bool sign = 0) { return sign ? a : a + 1; }

static int boolWidth(int a, int b, bool sign = 0) { return 1; }

static int dshlWidth(int a, int b, bool sign = 0) { return a + (1 << b) - 1; }

static int firstWidth(int a, int b, bool sign = 0) { return a; }

static int secondWidth(int a, int b, bool sign = 0) { return b; }

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
  {"and",   {0, 1, minWidth,}},
  {"or",    {0, 1, maxWidth,}},
  {"xor",   {0, 1, maxWidth,}},
  {"cat",   {0, 1, sumWidth,}},
};

// width num
std::map<std::string, std::tuple<bool, bool, int (*)(int, int, bool)>> expr1int1Map = {
    {"pad",   {1, 0, maxWidth}},
    {"shl",   {1, 0, sumWidth}},
    {"shr",   {1, 0, minusWidthPos}},
    {"head",  {0, 1, secondWidth}},
    {"tail",  {0, 1, minusWidth}},
};

// 0: uint 1: reverse child sign
std::map<std::string, std::tuple<uint8_t, int (*)(int, int, bool)>> expr1Map = {
    {"asUInt",      {SIGN_UINT, firstWidth}},
    {"asSInt",      {SIGN_SINT, firstWidth}},
    {"asClock",     {SIGN_UINT, boolWidth}},
    {"asAsyncReset",{SIGN_UINT, boolWidth}},
    {"cvt",         {SIGN_SINT, cvtWidth}},
    {"neg",         {SIGN_SINT, maxWidthPlus1}},  // a + 1 (second is zero)
    {"not",         {SIGN_UINT, firstWidth}},
    {"andr",        {SIGN_UINT, boolWidth}},
    {"orr",         {SIGN_UINT, boolWidth}},
    {"xorr",        {SIGN_UINT, boolWidth}},
};

static std::map<std::string, PNode*> moduleMap;
static std::map<std::string, Node*> allSignals;
static int whenDepth = 0;
static PNode* topWhen = NULL;
static std::vector<std::pair<bool, Node*>> whenTrace;  // cond Node trace
static std::vector<Index> emptyIdx;
void visitStmts(std::string prefix, graph* g, PNode* stmts);

/**
 * @brief add signal to allSignals
 *
 * @param s String
 * @param n Pointer to Signal of Node
 */

PNode* allocLIndex(Node* lnode, Index index, int idx) {
  PNode* pnode;
  if (index.type == INDEX_INT) {
    pnode = new PNode(P_L_CONS_INDEX);
    pnode->name = std::to_string(index.idx);
  } else {
    pnode = new PNode(P_L_INDEX);
  }
  pnode->setSign(lnode->sign);
  pnode->setWidth(lnode->width);
  pnode->lineno = idx;
  return pnode;
}

PNode* allocRIndex(Node* rnode, Index index, int idx) {
  PNode* pnode;
  if (index.type == INDEX_INT) {
    pnode = new PNode(P_CONS_INDEX);
    pnode->name = std::to_string(index.idx);
  } else
    pnode = new PNode(P_INDEX);
  pnode->setSign(rnode->sign);
  pnode->setWidth(rnode->width);
  pnode->lineno = idx;
  return pnode;
}

void addSignal(std::string s, Node* n) {
  Assert(allSignals.find(s) == allSignals.end(), "Signal %s is already in allSignals\n", s.c_str());
  allSignals[s] = n;
  // std::cout << "add " << s << " " << n << " " << n->width << std::endl;
}

void addEdge (Node* src, Node* dst) {
  if (dst->prev.find(src) == dst->prev.end()) dst->inEdge++;
  src->next.insert(dst);
  dst->prev.insert(src);  // get arguments in instGenerator
  // std::cout << src->name << " " << src << " -> " << dst->name << " " << dst << " " << dst->inEdge << std::endl;
}

void addEdgeOperand(Node* src, Node* dst, bool connectRecursive, std::vector<Index>& lindex = emptyIdx,
              std::vector<Index>& rindex = emptyIdx) { // TODO: src中成员为数组
  if (dst->aggrMember.size() != 0) {
    if (connectRecursive) {
      Assert(src->aggrType && src->aggrMember.size() == src->aggrType->aggr.size(), "error aggr type for %s\n", src->name.c_str());
      Assert(dst->aggrType && dst->aggrMember.size() == dst->aggrType->aggr.size(), "error aggr type for %s\n", dst->name.c_str());
      for (size_t i = 0; i < src->aggrMember.size(); i++) {
        if (src->aggrType->aggr[i].isFlip) addEdgeOperand(dst->aggrMember[i], src->aggrMember[i], connectRecursive, rindex, lindex);
        else addEdgeOperand(src->aggrMember[i], dst->aggrMember[i], connectRecursive, lindex, rindex);
      }
    } else {
      for (size_t i = 0; i < dst->aggrMember.size(); i++) {
        addEdgeOperand(src, dst->aggrMember[i], connectRecursive, lindex, rindex);
      }
    }
    return;
  }
  if (dst->type == NODE_REG_SRC) { dst = dst->regNext; }

  dst->workingVal->operands.push_back(src);

  if (rindex.size() != 0)  {
    int arrayIdx = getUpdateArrayIdx(rindex, src);
    Node* arraySrc = arrayIdx < 0 ? src : src->member[arrayIdx];
    for (size_t i = 0; i < rindex.size(); i ++) {
      dst->workingVal->ops.push_back(allocRIndex(src, rindex[i], i));
      if (rindex[i].type == INDEX_NODE) {
        dst->workingVal->operands.push_back(rindex[i].nidx);
        addEdge(rindex[i].nidx, dst);
      }
    }
    addEdge(arraySrc, dst);
  } else {
    if (dst->type == NODE_MEMBER) { dst = dst->parent; }
    if (src->type == NODE_MEMBER) { src = src->parent; }
    addEdge(src, dst);
  }

}

void addEdgeOperand(std::string str, Node* dst, bool connectRecursive, std::vector<Index>& lindex = emptyIdx,
              std::vector<Index>& rindex = emptyIdx) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  addEdgeOperand(allSignals[str], dst, connectRecursive, lindex, rindex);
}

Node* str2Node(std::string str) {
  Assert(allSignals.find(str) != allSignals.end(), "Signal %s is not defined\n", str.c_str());
  return allSignals[str];
}

void saveOps2Imp(Node* node, int depth) {
  if (depth == 0) {
    if (node->dimension.size() == 0) {
      node->workingVal->ops.erase(node->workingVal->ops.begin(), node->workingVal->ops.end());
      node->workingVal->operands.erase(node->workingVal->operands.begin(), node->workingVal->operands.end());
    }
    return;
  }
  if (node->workingVal->ops.size() == 0 && node->workingVal->operands.size() == 0) {
    ExprValue* imp = new ExprValue();
    if (node->type == NODE_REG_DST) {
      imp->operands.push_back(node->regNext);
    } else {  // for invalid signal
      PNode* zeroOp = new PNode(P_EXPR_INT_INIT);
      zeroOp->setWidth(node->width);
      zeroOp->setSign(node->sign);
      zeroOp->appendExtraInfo(strdup("h0"));
      imp->ops.push_back(zeroOp);
    }
    node->imps.push_back(imp);
  } else {
    node->imps.push_back(node->workingVal);
    node->value = node->workingVal = new ExprValue();   // CHECK: alloc a new exprValue for value ??
  }

  for (int i = 1; i < depth; i ++) node->imps.push_back(node->imps.back());  // CHECK: when to free

}

void releaseExprValue(Node* node, ExprValue* exprValue) {
  node->workingVal->ops.insert(node->workingVal->ops.end(), exprValue->ops.begin(), exprValue->ops.end());
  node->workingVal->operands.insert(node->workingVal->operands.end(), exprValue->operands.begin(), exprValue->operands.end());
}

void recoverOps(Node* node) {
  Assert(node->imps.size() >= 1, "empty imps for node %s\n", node->name.c_str());
  releaseExprValue(node, node->imps.back());
}

void recoverOpsFromStack(Node* n) {
  Assert(n->whenStack.size() == 1, "Invalid when stack(%ld) for %s\n", n->whenStack.size(), n->name.c_str());
  releaseExprValue(n, n->whenStack.back());
  n->whenStack.pop_back();
}

void clearOps(Node* node) {
  node->workingVal->ops.erase(node->workingVal->ops.begin(), node->workingVal->ops.end());
  node->workingVal->operands.erase(node->workingVal->operands.begin(), node->workingVal->operands.end());
}

void saveOps2stack(Node* node) {
  node->whenStack.push_back(node->workingVal);
  node->value = node->workingVal = new ExprValue();
}

int getUpdateArrayIdx(std::vector<Index>index, Node* node) {
  Assert(index.size() <= node->dimension.size(), "%s dimension not match\n", node->name.c_str());
  int fixIndex = 0;
  for (size_t idx = 0; idx < index.size(); idx ++) {
    if (index[idx].type == INDEX_NODE) {
      fixIndex = -1;
      break;
    } else {
      fixIndex = (fixIndex * (node->dimension[idx] + 1)) + index[idx].idx;
    }
  }
  if (fixIndex >= 0) {
    for (size_t idx = index.size(); idx < node->dimension.size(); idx ++) fixIndex = fixIndex * (node->dimension[idx] + 1) + node->dimension[idx];
  }
  return fixIndex;
}

Node* getUpdateArray(std::vector<Index>index, Node* node) {
  bool isFixIndex = true;
  int fixIndex = 0;
  for (size_t idx = 0; idx < index.size(); idx ++) {
    if (index[idx].type == INDEX_NODE) {
      isFixIndex = false;
      break;
    } else {
      fixIndex = (fixIndex * (node->dimension[idx] + 1)) + index[idx].idx;
    }
  }
  Node* arrayDst = node;
  if (isFixIndex && index.size() != 0) {
    arrayDst = node->member[fixIndex];
  }
  for (size_t idx = 0; idx < index.size(); idx ++) {
    arrayDst->workingVal->ops.push_back(allocLIndex(node, index[idx], idx));
    if (index[idx].type == INDEX_NODE) {
      addEdge(index[idx].nidx, arrayDst);
      arrayDst->workingVal->operands.push_back(index[idx].nidx);
      arrayDst->workingVal->ops.push_back(NULL);
    }
  }
  return arrayDst;
}

void updateNameIdx(Node* node, std::vector<int>& nameIdx) {
  int idx = node->dimension.size() - 1;
  nameIdx[idx] ++;
  while(idx > 0) {
    if (nameIdx[idx] > node->dimension[idx]) {
      nameIdx[idx] -= node->dimension[idx] + 1;
      nameIdx[idx - 1] ++;
      idx --;
    } else {
      return;
    }
  }
}

void allocArrayMember(Node* node) {
  if (node->dimension.size() == 0) return;
  int memberNum = 1;
  for (int idx = node->dimension.size() - 1; idx >= 0; idx --) {
    if (idx == 0) memberNum *= node->dimension[idx];
    else memberNum *= node->dimension[idx] + 1;
  }
  std::vector <int> nameIdx(node->dimension.size(), 0);
  for (int i = 0; i < memberNum; i ++) {   // only need to alloc when used
    Node* member = allocNode(NODE_ARRAY_MEMBER);
    member->name = node->name + "$";
    for (size_t j = 0; j < nameIdx.size(); j ++) {
      if(nameIdx[j] == node->dimension[j]) {
        for (size_t k = j; k < node->dimension.size(); k ++) member->memberDimension.push_back(node->dimension[k]);
        break;
      }
      member->name += "__" + std::to_string(nameIdx[j]);
    }
    updateNameIdx(node, nameIdx);
    SET_TYPE(member, node);
    node->member.push_back(member);
    member->parent = node;
    member->whenDepth = node->whenDepth;
  }
}

void aggrAllocArrayMember(Node* node) {
  if (!node->aggrType) {
    allocArrayMember(node);
    return;
  }
  for (Node* member : node->aggrMember) {
    aggrAllocArrayMember(member);
  }
}

void inline connectNodeWhen(graph* g, std::set<Node*>& nodes, PNode* when) {
  g->when2NodeSet[when].insert(nodes.begin(), nodes.end());
  for (Node* n : nodes) {
    g->node2WhenSet[n].insert(when);
  }
}

void visitPorts(std::string prefix, graph* g, PNode* ports) {  // treat as node
  for (int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = allocNode();
    io->name = prefix + (prefix.length() == 0 ? "" : "$") + port->name;
    visitType(io, port->getChild(0));
    SET_TYPE(port, io);
    addSignal(io->name, io);
  }
}

void visitModule(std::string prefix, graph* g, PNode* module) {
  MUX_DEBUG(std::cout << "visit " << module->name << std::endl);
  visitPorts(prefix, g, module->getChild(0));
  visitStmts(prefix, g, module->getChild(1));
  MUX_DEBUG(std::cout << "leave " << module->name << std::endl);
}

void setAggrConsZero(Node* node, bool isOut) {
  if (!node->aggrType && isOut) {
    node->status = CONSTANT_NODE;
    node->workingVal->consVal = "0";
    return;
  }
  for (size_t i = 0; i < node->aggrMember.size(); i++) {
    if (node->aggrType->aggr[i].isFlip) setAggrConsZero(node->aggrMember[i], !isOut);
    else setAggrConsZero(node->aggrMember[i], isOut);
  }
}

void visitExtPorts(std::string prefix, graph* g, PNode* ports) {  // treat as node
  for (int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());
    Node* io = allocNode();
    io->name = prefix + (prefix.length() == 0 ? "" : "$") + port->name;
    visitType(io, port->getChild(0));
    SET_TYPE(port, io);
    addSignal(io->name, io);
    if(port->type == P_OUTPUT) {
      setAggrConsZero(io, true);
    }
  }
}

void visitExtModule(std::string prefix, graph* g, PNode* module) {
  MUX_DEBUG(std::cout << "visit " << module->name << std::endl);
  visitExtPorts(prefix, g, module->getChild(0));
  MUX_DEBUG(std::cout << "leave " << module->name << std::endl);
}

void addAggrMemberDimension(Node* node, int dimension) {
  if (!node->aggrType) {
    node->dimension.insert(node->dimension.begin(), dimension);
    node->entryNum *= dimension;
    return;
  }
  for (Node* member : node->aggrMember) {
    addAggrMemberDimension(member, dimension);
  }
}

void addAggrMemberOps(Node* node, PNode* op) {
  if (!node->aggrType) {
    node->workingVal->ops.push_back(op);
    return;
  }
  for (Node* member : node->aggrMember) {
    addAggrMemberOps(member, op);
  }
}

void addAggrMemberOperands(Node* node, Node* operand) {
  if (!node->aggrType) {
    node->workingVal->operands.push_back(operand);
    return;
  }
  for (Node* member : node->aggrMember) {
    addAggrMemberOperands(member, operand);
  }
}

void visitFields(Node* n, PNode* fields) {
  AggrType* aggrType = new AggrType();

  for(int i = 0; i < fields->getChildNum(); i++) {
    PNode* field = fields->getChild(i);
    Node* newField = allocNode(n->type);
    if(field->type == P_FLIP_FIELD) {
      if(n->type == NODE_INP || n->type == NODE_OUT)
        newField->type = NODE_INP + NODE_OUT - n->type;
    }
    newField->name = n->name + "$" + field->name;

    visitType(newField, field->getChild(0));
    addSignal(newField->name, newField);
    aggrAllocArrayMember(newField);

    n->aggrMember.push_back(newField);
    if (n->type == NODE_REG_SRC) {
      Node* nextField = allocNode(NODE_REG_DST);
      nextField->name = newField->name + "$next";
      addSignal(nextField->name, nextField);
      nextField->regNext = newField;
      newField->regNext = nextField;
      n->regNext->aggrMember.push_back(nextField);
      SET_TYPE(nextField, newField);
    }
    aggrType->aggr.push_back({newField->width, newField->sign, field->type == P_FLIP_FIELD, field->name, newField->aggrType});
  }
  n->aggrType = aggrType;
}

/**
 * @brief Visits PNode type
 *
 * @param n Pointer to the node
 * @param ptype Pointer to the PNode of ast
 * n->type must be set for P_AG_FIELDS before visitType
 */
void visitType(Node* n, PNode* ptype) {
  switch (ptype->type) {
    case P_Clock: n->width = 1; break;
    case P_INT_TYPE: SET_TYPE(n, ptype); break;
    case P_AG_FIELDS:
      Assert(n->name.length() > 0, "name must be set before visitType\n");
      visitFields(n, ptype);
      break;
    case P_AG_TYPE:
      visitType(n, ptype->getChild(0));
      addAggrMemberDimension(n, p_stoi(ptype->getExtra(0).c_str()));
      break;
    default: std::cout << ptype->type << std::endl; TODO();
  }
}

void visit1Expr1Int(std::string prefix, Node* n, PNode* expr, bool connectRecursive) {  // pad|shl|shr|head|tail
  Assert(expr1int1Map.find(expr->name) != expr1int1Map.end(), "Operation %s not found\n", expr->name.c_str());
  n->workingVal->ops.push_back(expr);
  std::tuple<bool, bool, int (*)(int, int, bool)> info = expr1int1Map[expr->name];
  visitExpr(prefix, n, expr->getChild(0), connectRecursive);
  expr->sign  = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  int arg     = p_stoi(expr->getExtra(0).c_str());
  expr->width = std::get<2>(info)(expr->getChild(0)->width, arg, false);
  if (expr->getChild(0)->status == CONSTANT_PNODE) { expr->status = CONSTANT_PNODE; }
}

void visit1Expr2Int(std::string prefix, Node* n, PNode* expr, bool connectRecursive) {  // bits
  n->workingVal->ops.push_back(expr);
  visitExpr(prefix, n, expr->getChild(0), connectRecursive);
  expr->sign  = 0;
  expr->width = p_stoi(expr->getExtra(0).c_str()) - p_stoi(expr->getExtra(1).c_str()) + 1;
  if (expr->getChild(0)->status == CONSTANT_PNODE) { expr->status = CONSTANT_PNODE; }
}

void visit2Expr(std::string prefix, Node* n, PNode* expr, bool connectRecursive) {  // add|sub|mul|div|mod|lt|leq|gt|geq|eq|neq|dshl|dshr|and|or|xor|cat
  Assert(expr->getChildNum() == 2, "Invalid childNum for expr %s\n", expr->name.c_str());

  n->workingVal->ops.push_back(expr);
  visitExpr(prefix, n, expr->getChild(0), connectRecursive);
  n->workingVal->ops.push_back(NULL);
  visitExpr(prefix, n, expr->getChild(1), connectRecursive);
  Assert(expr2Map.find(expr->name) != expr2Map.end(), "Operation %s not found\n", expr->name.c_str());

  std::tuple<bool, bool, int (*)(int, int, bool)> info = expr2Map[expr->name];
  expr->sign = std::get<0>(info) ? expr->getChild(0)->sign : 0;
  expr->width = std::get<2>(info)(expr->getChild(0)->width, expr->getChild(1)->width, expr->getChild(0)->sign);
  if (expr->getChild(0)->status == CONSTANT_PNODE && expr->getChild(1)->status == CONSTANT_PNODE) {
    expr->status = CONSTANT_PNODE;
  }
}

void visit1Expr(std::string prefix, Node* n, PNode* expr, bool connectRecursive) {  // asUInt|asSInt|asClock|cvt|neg|not|andr|orr|xorr
  n->workingVal->ops.push_back(expr);
  std::tuple<uint8_t, int (*)(int, int, bool)> info = expr1Map[expr->name];
  visitExpr(prefix, n, expr->getChild(0), connectRecursive);
  expr->sign  = std::get<0>(info);
  expr->width = std::get<1>(info)(expr->getChild(0)->width, 0, expr->getChild(0)->sign);
  if (expr->getChild(0)->status == CONSTANT_PNODE) { expr->status = CONSTANT_PNODE; }
}

std::string visitReference(std::string prefix, PNode* expr, std::vector<Index>& index, Node* n = NULL) {  // return ref name
  std::string ret = prefix.length() == 0 ? expr->name : (prefix + "$" + expr->name);
  std::string tmp;
  std::vector<Index> tmpIdx;
  Node* node;
  for (int i = 0; i < expr->getChildNum(); i++) {
    PNode* child = expr->getChild(i);
    switch (child->type) {
      case P_REF_IDX_EXPR:
        Assert(child->getChild(0)->type == P_REF || child->getChild(0)->type == P_EXPR_INT_INIT, "Invalid expr as index in line %d\n", child->lineno);
        if (child->getChild(0)->type == P_REF) {
          tmp = visitReference(prefix, child->getChild(0), tmpIdx);
          if (tmpIdx.size() != 0) TODO();
          node = str2Node(tmp);
          index.push_back({INDEX_NODE, -1, node});
        } else {
          index.push_back({INDEX_INT, p_stoi(child->getChild(0)->getExtra(0).c_str()), NULL});
        }
        break;
      case P_REF_DOT:
        ret += "$" + child->name;
        break;
      case P_REF_IDX_INT:
        index.push_back({INDEX_INT, p_stoi(child->name.c_str()), NULL});
        break;
      default: Assert(0, "TODO: invalid ref child type(%d) for %s\n", child->type, expr->name.c_str());
    }
  }
  return ret;
}

void passOps2Child(Node* n, int opIdx = 0) {
  if (!n->aggrType) return;
  for (int i = opIdx; i < (int)n->workingVal->ops.size(); i++) {
    addAggrMemberOps(n, n->workingVal->ops[i]);
  }
}

void passOperand2Child(Node* n, int operandIdx = 0) {
  if (!n->aggrType) return;
  for (int i = operandIdx; i < (int)n->workingVal->operands.size(); i ++) {
    addAggrMemberOperands(n, n->workingVal->operands[i]);
  }
}

void visitMux(std::string prefix, Node* n, PNode* mux, bool connectRecursive) {
  Assert(mux->getChildNum() == 3, "Invalid childNum(%d) for Mux\n", mux->getChildNum());

  addAggrMemberOps(n, mux);
  visitExpr(prefix, n, mux->getChild(0), false);

  addAggrMemberOps(n, NULL);
  visitExpr(prefix, n, mux->getChild(1), true);

  addAggrMemberOps(n, NULL);
  visitExpr(prefix, n, mux->getChild(2), true);

  if (mux->getChild(0)->status == CONSTANT_NODE && mux->getChild(1)->status == CONSTANT_NODE &&
      mux->getChild(2)->status == CONSTANT_NODE) {
    mux->status = CONSTANT_NODE;
  }
  mux->getChild(1)->width = mux->getChild(2)->width = MAX(mux->getChild(1)->width, mux->getChild(2)->width);
  SET_TYPE(mux, mux->getChild(1));
}

std::string cons2str(std::string s) {
  if (s.length() <= 1) { return s; }

  std::string ret;
  int idx = 1;

  if (s[1] == '-') {
    ret += "-";
    idx = 2;
  }

  if (s[0] == 'b') ret += "0b";
  else if (s[0] == 'o') ret += "0";
  else if (s[0] == 'h') ret += "0x";
  else idx = 0;

  ret += s.substr(idx);

  return ret;
}

void visitExpr(std::string prefix, Node* n, PNode* expr, bool connectRecursive) {  // return varName & update connect
  std::string ret;
  std::vector<Index> index;
  switch (expr->type) {
    case P_1EXPR1INT: visit1Expr1Int(prefix, n, expr, connectRecursive); break;
    case P_2EXPR: visit2Expr(prefix, n, expr, connectRecursive); break;
    case P_REF: {
      ret = visitReference(prefix, expr, index, n);
      if(allSignals[ret]->type == NODE_LOCAL) {
        n->workingVal->operands.insert(n->workingVal->operands.end(), allSignals[ret]->workingVal->operands.begin(), allSignals[ret]->workingVal->operands.end());
        n->workingVal->ops.insert(n->workingVal->ops.end(), allSignals[ret]->workingVal->ops.begin(), allSignals[ret]->workingVal->ops.end());
        // prev can also be updated here
      } else {
        addEdgeOperand(ret, n, connectRecursive, n->index, index);
      }
      SET_TYPE(expr, allSignals[ret]);
      if (allSignals[ret]->status == CONSTANT_NODE) { expr->status = CONSTANT_NODE; }
      break;
    }
    case P_EXPR_MUX: visitMux(prefix, n, expr, connectRecursive); break;
    case P_EXPR_INT_INIT: {
      n->workingVal->ops.push_back(expr);
      expr->status = CONSTANT_NODE;
      break;
    }
    case P_1EXPR: visit1Expr(prefix, n, expr, connectRecursive); break;
    case P_1EXPR2INT: visit1Expr2Int(prefix, n, expr, connectRecursive); break;
    default: {
      std::cout << expr->type << std::endl;
      TODO();
    }
  }
}

void visitWhenConnectExpr(std::string prefix, Node* n, PNode* expr, bool connectRecursive,
                   std::set<Node*>& dstSet) {  // return varName & update connect
  if (!n->aggrType) {
    connectAdd(dstSet, n, whenDepth - n->imps.size());
    visitExpr(prefix, n, expr, connectRecursive);
    return;
  }
  int opIdx = n->workingVal->ops.size();
  std::string dst;
  std::vector<Index> index;
  switch (expr->type) {
    case P_REF:
      dst = visitReference(prefix, expr, index, n);
      passOps2Child(n, opIdx);
      connectAggrAdd(dstSet, n, allSignals[dst], whenDepth);

      addEdgeOperand(dst, n, connectRecursive, n->index, index);
      SET_TYPE(expr, allSignals[dst]);
      if (allSignals[dst]->status == CONSTANT_NODE) { expr->status = CONSTANT_NODE; }
      break;
    default:
      TODO();
  }

}

void createMemberNodes(Node* node) {
  for (auto member : node->aggrType->aggr) {
    Node* newNode = allocNode(node->type);
    if (member.isFlip && (node->type == NODE_INP || node->type == NODE_OUT))
      newNode->type = NODE_INP + NODE_OUT - node->type;
    newNode->name = node->name + "$" + member.name;
    newNode->sign = member.sign;
    newNode->width = member.width;
    newNode->aggrType = member.aggrType;
    node->aggrMember.push_back(newNode);
    addSignal(newNode->name, newNode);
    if(member.aggrType) createMemberNodes(newNode);
  }
}

AggrType* parseMux(std::string prefix, PNode* expr) {
  return parseAggr(prefix, expr->getChild(1));
}

AggrType* parseAggr(std::string prefix, PNode* expr) {
  std::vector<Index>index;
  std::string ref;
  Node* refNode;
  switch (expr->type) {
    case P_REF:
      ref = visitReference(prefix, expr, index, NULL);
      refNode = str2Node(ref);
      return refNode->aggrType;

    case P_EXPR_MUX:
      return parseMux(prefix, expr);

    default:
      return NULL;
  }
}

Node* visitNode(std::string prefix, PNode* node, int type = NODE_OTHERS) {  // generate new node and connect
  Node* newSig = allocNode(type);
  newSig->name = prefix + (prefix.length() == 0 ? "" : "$") + node->name;
  Assert(node->getChildNum() >= 1, "Invalid childNum for node %s\n", node->name.c_str());
  newSig->aggrType = parseAggr(prefix, node->getChild(0)); // refer aggrType
  if (newSig->aggrType) createMemberNodes(newSig);
  visitExpr(prefix, newSig, node->getChild(0), true);
  SET_TYPE(newSig, node->getChild(0));
  addSignal(newSig->name, newSig);
  SET_TYPE(node, newSig);
  aggrAllocArrayMember(newSig);
  return newSig;
}

void visitConnect(std::string prefix, PNode* connect) {
  Assert(connect->getChildNum() == 2, "Invalid childNum for connect %s\n", connect->name.c_str());
  std::vector<Index> index;
  std::string strDst = visitReference(prefix, connect->getChild(0), index);
  Node* dst = str2Node(strDst);
  if (dst->type == NODE_REG_SRC) { dst = dst->regNext; }
  dst->index.erase(dst->index.begin(), dst->index.end());
  dst->index.insert(dst->index.end(), index.begin(), index.end());  // TODO: check & remove
  if (dst->dimension.size() == 0 && (dst->workingVal->ops.size() != 0 || dst->workingVal->operands.size() != 0)) { // override previous connection
    for (Node* prev : dst->prev) {
      Assert(prev->next.find(dst) != prev->next.end(), "no connection between %s and %s\n", prev->name.c_str(), dst->name.c_str());
      prev->next.erase(dst);
    }
    dst->workingVal->ops.erase(dst->workingVal->ops.begin(), dst->workingVal->ops.end());
    dst->workingVal->operands.erase(dst->workingVal->operands.begin(), dst->workingVal->operands.end());
    dst->prev.erase(dst->prev.begin(), dst->prev.end());
    dst->inEdge = 0;
  }

  int opIdx = dst->workingVal->ops.size();

  if (dst->dimension.size() == 0) {
    visitExpr(prefix, dst, connect->getChild(1), true);
    SET_TYPE(connect->getChild(0), connect->getChild(1));
    if (connect->getChild(1)->status == CONSTANT_NODE) { connect->status = CONSTANT_NODE; }
    passOps2Child(dst, opIdx);
  } else {
    Node* arrayDst = getUpdateArray(index, dst);
    visitExpr(prefix, arrayDst, connect->getChild(1), true);
    SET_TYPE(connect->getChild(0), connect->getChild(1));
    if (connect->getChild(1)->status == CONSTANT_NODE) { connect->status = CONSTANT_NODE; }
    if (dst->aggrType) TODO();
    passOps2Child(dst, opIdx);

  }
}

void aggrAllocResetCond(Node* node) {
  if (!node->aggrType) node->resetCond = node->workingVal = new ExprValue();
  else {
    for (Node* n : node->aggrMember) aggrAllocResetCond(n);
  }
}

void aggrAllocResetVal(Node* node) {
  if (!node->aggrType) node->resetVal = node->workingVal = new ExprValue();
  else {
    for (Node* n : node->aggrMember) aggrAllocResetVal(n);
  }
}

void aggrRecoverWorkingVal(Node* node) {
  if (!node->aggrType) node->workingVal = node->value;
  else {
    for (Node* n : node->aggrMember) aggrRecoverWorkingVal(n);
  }
}

void aggrAllocWorkingVal(Node* node) {
  if (!node->aggrType) node->workingVal = new ExprValue();
  else {
    for (Node* n : node->aggrMember) aggrAllocWorkingVal(n);
  }
}

void aggrSetiValue(Node* node) {
  if (!node->aggrType) node->iValue = node->workingVal;
  else {
    for (Node* n : node->aggrMember) aggrSetiValue(n);
  }
}

void aggrSetWorkingVal(Node* node) {
  if (!node->aggrType) node->workingVal = node->value;
  else {
    for (Node* n : node->aggrMember) aggrSetWorkingVal(n);
  }
}

void visitRegDef(std::string prefix, graph* g, PNode* reg) {
  Node* newReg = allocNode(NODE_REG_SRC);
  newReg->name = prefix + (prefix.length() == 0 ? "" : "$") + reg->name;

  Node* nextReg = allocNode(NODE_REG_DST);
  nextReg->name = newReg->name + "$next";

  newReg->regNext  = nextReg;
  nextReg->regNext = newReg;

  visitType(newReg, reg->getChild(0));
  SET_TYPE(nextReg, newReg);

  addSignal(newReg->name, newReg);
  addSignal(nextReg->name, nextReg);
  for (int index : newReg->dimension) {
    nextReg->dimension.push_back(index);
    nextReg->entryNum *= index;
  }
  aggrAllocArrayMember(newReg);
  aggrAllocArrayMember(nextReg);

  aggrAddLeaf(g->sources, newReg);

  aggrAllocResetCond(nextReg);
  visitExpr(prefix, nextReg, reg->getChild(2), false);

  aggrAllocResetVal(nextReg);
  visitExpr(prefix, nextReg, reg->getChild(3), true);

  aggrRecoverWorkingVal(nextReg);
}

void visitPrintf(std::string prefix, graph* g, PNode* print) {
  Node* n = allocNode(NODE_ACTIVE);
  n->name = prefix + (prefix.length() == 0 ? "" : "$") + print->name;
  n->workingVal->ops.push_back(print);
  visitExpr(prefix, n, print->getChild(1), false);

  PNode* exprs = print->getChild(2);

  for (int i = 0; i < exprs->getChildNum(); i++) {
    n->workingVal->ops.push_back(NULL);
    visitExpr(prefix, n, exprs->getChild(i), false);
  }

  g->active.push_back(n);
}

void visitAssert(std::string prefix, graph* g, PNode* ass) {
  Node* n = allocNode(NODE_ACTIVE);
  n->name = prefix + (prefix.length() == 0 ? "" : "$") + ass->name;
  n->workingVal->ops.push_back(ass);

  visitExpr(prefix, n, ass->getChild(1), false);
  n->workingVal->ops.push_back(NULL);
  visitExpr(prefix, n, ass->getChild(2), false);

  g->active.push_back(n);
}

void visitMemory(std::string prefix, graph* g, PNode* memory) {
  Assert(memory->getChildNum() >= 5, "Invalid childNum(%d) ", memory->getChildNum());
  Assert(memory->getChild(0)->type == P_DATATYPE, "Invalid child0 type(%d)\n", memory->getChild(0)->type);

  Node* n = allocNode(NODE_MEMORY);
  n->name = prefix + (prefix.length() == 0 ? "" : "$") + memory->name;
  g->memory.push_back(n);
  visitType(n, memory->getChild(0)->getChild(0));
  if(n->aggrType) TODO();
  Assert(memory->getChild(1)->type == P_DEPTH, "Invalid child0 type(%d)\n", memory->getChild(0)->type);

  int width = n->width;
  if (n->width < 8) n->width = 8;
  else n->width = p2(n->width);

  Assert(n->width % 8 == 0, "invalid memory width %d for %s\n", n->width, n->name.c_str());

  int depth        = p_stoi(memory->getChild(1)->name.c_str());
  int readLatency  = p_stoi(memory->getChild(2)->name.c_str());
  int writeLatency = p_stoi(memory->getChild(3)->name.c_str());

  Assert(readLatency <= 1 && writeLatency <= 1, "Invalid readLatency(%d) or writeLatency(%d)\n", readLatency,
         writeLatency);

  n->latency[0] = readLatency;
  n->latency[1] = writeLatency;
  n->val        = depth;

  Assert(memory->getChild(4)->name == "undefined", "Invalid ruw %s\n", memory->getChild(4)->name.c_str());

  // readers
  for (int i = 5; i < memory->getChildNum(); i++) {
    PNode* rw = memory->getChild(i);

    Node* rn = allocNode();
    rn->name = n->name + "$" + rw->name;
    n->member.push_back(rn);
    rn->parent = n;

    memory_member(addr, rn, log2(depth));
    memory_member(en, rn, 1);
    memory_member(clk, rn, 1);

    if (rw->type == P_READER) {
      rn->type = NODE_READER;
      memory_member(data, rn, width);
      if (readLatency == 1) {
        rn->member[3]->type = NODE_L1_RDATA;
        g->memRdata1.push_back(rn->member[3]);
      }
    } else if (rw->type == P_WRITER) {
      rn->type = NODE_WRITER;
      memory_member(data, rn, width);
      memory_member(mask, rn, 1);
    } else if (rw->type == P_READWRITER) {
      rn->type = NODE_READWRITER;
      memory_member(rdata, rn, width);
      memory_member(wdata, rn, width);
      memory_member(wmask, rn, 1);
      memory_member(wmode, rn, 1);
      if (readLatency == 1) {
        rn->member[3]->type = NODE_L1_RDATA;
        g->memRdata1.push_back(rn->member[3]);
      }
    } else {
      Assert(0, "Invalid rw type %d\n", rw->type);
    }
  }
}

void visitWireDef(std::string prefix, graph* g, PNode* wire, int type = NODE_OTHERS) {
  Node* newWire = allocNode(type);
  newWire->name = prefix + (prefix.length() == 0 ? "" : "$") + wire->name;
  newWire->whenDepth = whenDepth;

  visitType(newWire, wire->getChild(0));
  aggrAllocArrayMember(newWire);
  addSignal(newWire->name, newWire);
}

void visitWhenConnect(std::string prefix, graph* g, PNode* connect,
                      std::set<Node*>& trueNodes, std::set<Node*>& falseNodes, bool condTrue, Node* cond, PNode* when) {
  std::vector<Index> index;
  std::string strDst = visitReference(prefix, connect->getChild(0), index); // TODO：创建一个临时结构体用于存储中间数据
  Node* dst = str2Node(strDst);
  if (dst->type == NODE_REG_SRC) { dst = dst->regNext; }

  dst->index.erase(dst->index.begin(), dst->index.end());
  dst->index.insert(dst->index.end(), index.begin(), index.end());
  if (dst->dimension.size() == 0) {
    visitWhenConnectExpr(prefix, dst, connect->getChild(1), true, (condTrue ? trueNodes : falseNodes));
  } else {
    int opIdx = 0, operandIdx = 0;
    if ((size_t)whenDepth == dst->imps.size()) {
      opIdx = dst->workingVal->ops.size();
      operandIdx = dst->workingVal->operands.size();
    }
    int arrayIdx = getUpdateArrayIdx(index, dst);

    Node* arrayDst = (index.size() == 0 || arrayIdx < 0) ? dst : dst->member[arrayIdx];
    if ((size_t)whenDepth == arrayDst->imps.size()) {
      opIdx = arrayDst->workingVal->ops.size();
      operandIdx = arrayDst->workingVal->operands.size();
    }
    visitWhenConnectExpr(prefix, arrayDst, connect->getChild(1), true, (condTrue ? trueNodes : falseNodes));
    for (size_t idx = 0; idx < index.size(); idx ++) {
      arrayDst->workingVal->ops.insert(arrayDst->workingVal->ops.begin() + opIdx, allocLIndex(arrayDst, index[idx], idx));
      opIdx ++;
      if (index[idx].type == INDEX_NODE) {
        addEdge(index[idx].nidx, arrayDst);
        arrayDst->workingVal->operands.insert(arrayDst->workingVal->operands.begin() + operandIdx, index[idx].nidx);
        operandIdx ++;
        arrayDst->workingVal->ops.insert(arrayDst->workingVal->ops.begin() + opIdx, NULL);
        opIdx ++;
      }
    }
  }

  SET_TYPE(connect->getChild(0), connect->getChild(1));
}

void visitWhenPrintf(std::string prefix, graph* g, PNode* print, Node* cond) {
  if(print->getChild(1)->type == P_EXPR_INT_INIT && p_stoi(print->getChild(1)->name.c_str()) == 0) return;
  Node* n = allocNode(NODE_ACTIVE);
  n->name = prefix + (prefix.length() == 0 ? "" : "$") + print->name;
  addSignal(n->name, n); // TODO:remove
  n->workingVal->ops.push_back(print);
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    addEdge(whenTrace[i].second, n);
    PNode* pnode = new PNode(P_2EXPR);
    pnode->name = "and";
    pnode->width = 1;
    n->workingVal->ops.push_back(pnode);
    if (!whenTrace[i].first) {
      PNode* pnode_not = new PNode(P_1EXPR);
      pnode_not->name = "not";
      pnode_not->width = 1;
      n->workingVal->ops.push_back(pnode_not);
    }
    n->workingVal->operands.push_back(whenTrace[i].second);
    n->workingVal->ops.push_back(NULL);
  }
  visitExpr(prefix, n, print->getChild(1), false);

  PNode* exprs = print->getChild(2);
  for (int i = 0; i < exprs->getChildNum(); i++) {
    n->workingVal->ops.push_back(NULL);
    visitExpr(prefix, n, exprs->getChild(i), false);
  }

  g->active.push_back(n);
}

void visitWhenAssert(std::string prefix, graph* g, PNode* ass, Node* cond) {
  Node* n = allocNode(NODE_ACTIVE);
  n->name = prefix + (prefix.length() == 0 ? "" : "$") + ass->name;
  n->workingVal->ops.push_back(ass);
  addSignal(n->name, n); // TODO: remove
  visitExpr(prefix, n, ass->getChild(1), false);
  n->workingVal->ops.push_back(NULL);
  for (size_t i = 0; i < whenTrace.size(); i ++) {
    addEdge(whenTrace[i].second, n);
    PNode* pnode = new PNode(P_2EXPR);
    pnode->name = "and";
    n->workingVal->ops.push_back(pnode);
    if (!whenTrace[i].first) {
      PNode* pnode_not = new PNode(P_1EXPR);
      pnode_not->name = "not";
      n->workingVal->ops.push_back(pnode_not);
    }
    n->workingVal->operands.push_back(whenTrace[i].second);
    n->workingVal->ops.push_back(NULL);
  }
  visitExpr(prefix, n, ass->getChild(2), false);

  g->active.push_back(n);
}

void visitWhenStmt(std::string prefix, graph* g, PNode* stmt, bool condTrue, Node* cond, std::set<Node*>& trueNodes,
                   std::set<Node*>& falseNodes, PNode* when) {

  switch (stmt->type) {
    case P_NODE:
      visitNode(prefix, stmt);
      break;
    case P_CONNECT:
      visitWhenConnect(prefix, g, stmt, trueNodes, falseNodes, condTrue, cond, when);
      break;
    case P_WHEN:
      visitWhen(prefix, g, stmt, cond, condTrue ? trueNodes : falseNodes);
      break;
    case P_WIRE_DEF:
      visitWireDef(prefix, g, stmt);
      break;
    case P_PRINTF:
      visitWhenPrintf(prefix, g, stmt, cond);
      break;
    case P_ASSERT:
      visitWhenAssert(prefix, g, stmt, cond);
      break;
    default: std::cout << stmt->type << " " << stmt->name << std::endl; TODO();
  }
}

void updateWhenNode(Node* n, Node* cond, PNode* when, bool flip) {
  Assert(n->whenStack.size() >= 2, "invalid whenStack(%ld) for %s in depth %d\n", n->whenStack.size(), n->name.c_str(), whenDepth);
  clearOps(n);
  n->workingVal->ops.push_back(when);
  addEdgeOperand(cond, n, false);
  n->workingVal->ops.push_back(NULL);
  ExprValue* condTrue, *condFalse;
  if (flip) {
    condTrue = n->whenStack.back();
    condFalse = n->whenStack[n->whenStack.size() - 2];
  } else {
    condTrue = n->whenStack[n->whenStack.size() - 2];
    condFalse = n->whenStack.back();
  }
  Assert(condTrue || condFalse, "Invalid whenStack for %s\n", n->name.c_str());

  if (condTrue) releaseExprValue(n, condTrue);
  else recoverOps(n);
  n->workingVal->ops.push_back(NULL);
  if (condFalse) releaseExprValue(n, condFalse);
  else recoverOps(n);

  n->whenStack.pop_back();
  n->whenStack.pop_back();
}

void visitWhen(std::string prefix, graph* g, PNode* when, Node* prevCond, std::set<Node*>& allNodes) { 
  // TODO: remove cStack & rename nextStack
  if (whenDepth == 0) topWhen = when;
  whenDepth ++;
  Node* cond = allocNode(NODE_OTHERS);
  cond->name = prefix + "$WHEN_COND_" + std::to_string(when->lineno);
  cond->width = 1;
  addSignal(cond->name, cond); // for debug
  visitExpr(prefix, cond, when->getChild(0), false);

  std::set<Node*> trueNodes;
  std::set<Node*> falseNodes;

  whenTrace.push_back(std::make_pair(true, cond));
  for (int i = 0; i < when->getChild(1)->getChildNum(); i ++) {
    visitWhenStmt(prefix, g, when->getChild(1)->getChild(i), true, cond, trueNodes, falseNodes, when);
  }
  for (Node* n : trueNodes) {
    if (n->whenDepth == whenDepth) continue;
    saveOps2stack(n);
    recoverOps(n);
  }

  whenTrace.back().first = false;
  for (int i = 0; i < when->getChild(2)->getChildNum(); i ++) {
    visitWhenStmt(prefix, g, when->getChild(2)->getChild(i), false, cond, trueNodes, falseNodes, when);
  }
  for (Node* n : falseNodes) {
    if (n->whenDepth == whenDepth) continue;
    saveOps2stack(n);
    recoverOps(n);
  }

  std::set<Node*> curNodes;
  curNodes.insert(trueNodes.begin(), trueNodes.end());
  curNodes.insert(falseNodes.begin(), falseNodes.end());

  connectNodeWhen(g, curNodes, topWhen);

  for (auto iter = curNodes.begin(); iter != curNodes.end(); ) {
    if ((*iter)->whenDepth >= whenDepth) iter = curNodes.erase(iter);
    else iter ++;
  }

  for (Node* n : curNodes) {
    if (trueNodes.find(n) == trueNodes.end()) {
      n->whenStack.push_back(NULL);
      updateWhenNode(n, cond, when, true);
    } else if(falseNodes.find(n) == falseNodes.end()) {
      n->whenStack.push_back(NULL);
      updateWhenNode(n, cond, when, false);
    } else {
      updateWhenNode(n, cond, when, false);
    }
  }

  allNodes.insert(curNodes.begin(), curNodes.end());

  for (Node* n : curNodes) {
    Assert(n->imps.size() == (size_t)whenDepth, "imps size %ld depth %d for node %s in line %d\n", n->imps.size(), whenDepth, n->name.c_str(), when->lineno);
    n->imps.pop_back();
  }

  whenDepth --;
  whenTrace.pop_back();
}

/**
 * @brief Visits statements
 *
 * @param prefix
 * @param g Pointer to graph
 * @param stmts Pointer to the statements
 */
void visitStmts(std::string prefix, graph* g, PNode* stmts) {
  PNode* module;
  for (int i = 0; i < stmts->getChildNum(); i++) {
    PNode* stmt = stmts->getChild(i);
    std::set<Node*> allNodes;

    switch (stmt->type) {
      case P_INST: {
        Assert(stmt->getExtraNum() >= 1 && moduleMap.find(stmt->getExtra(0)) != moduleMap.end(),
               "Module %s is not defined!\n", stmt->name.c_str());

        module = moduleMap[stmt->getExtra(0)];
        if (module->type == P_MOD)
          visitModule(prefix.length() == 0 ? stmt->name : (prefix + (prefix.length() == 0 ? "" : "$") + stmt->name), g, module);
        else if (module->type == P_EXTMOD)
          visitExtModule(prefix.length() == 0 ? stmt->name : (prefix + (prefix.length() == 0 ? "" : "$") + stmt->name), g, module);
        break;
      }
      case P_NODE: visitNode(prefix, stmt); break;
      case P_CONNECT: visitConnect(prefix, stmt); break;
      case P_REG_DEF: visitRegDef(prefix, g, stmt); break;
      case P_PRINTF: visitPrintf(prefix, g, stmt); break;
      case P_ASSERT: visitAssert(prefix, g, stmt); break;
      case P_MEMORY: visitMemory(prefix, g, stmt); break;
      case P_WIRE_DEF: visitWireDef(prefix, g, stmt); break;
      case P_WHEN: visitWhen(prefix, g, stmt, NULL, allNodes); break;
      default: Assert(0, "Invalid stmt %s(%d)\n", stmt->name.c_str(), stmt->type);
    }

  }
}

void aggrAdd(std::vector<Node*>& dst, Node* node) {
  dst.push_back(node);
  if (!node->aggrType) return;
  for (Node* n : node->aggrMember) aggrAdd(dst, n);
}

void aggrAddLeaf(std::vector<Node*>&dst, Node* node) {
  if (!node->aggrType) {
    dst.push_back(node);
    return;
  }
  for (Node* n : node->aggrMember) aggrAddLeaf(dst, n);
}

void connectAdd(std::set<Node*>& dstSet, Node* node, int num) {
  dstSet.insert(node);
  saveOps2Imp(node, num);
}

void connectAggrAdd(std::set<Node*>& dstSet, Node* node, Node* rightNode, int depth) {
  if (!node->aggrType) {
    dstSet.insert(node);
    saveOps2Imp(node, depth - node->imps.size());
    return;
  }
  for (size_t i = 0; i < node->aggrMember.size(); i++) {
    if (node->aggrType->aggr[i].isFlip) connectAggrAdd(dstSet, rightNode->aggrMember[i], node->aggrMember[i], depth);
    else connectAggrAdd(dstSet, node->aggrMember[i], rightNode->aggrMember[i], depth);
  }
}

/**
 * @brief Visits ports of the topmodule.
 *
 * @param g Pointer to graph structure to be converted and stored
 * @param ports input/output ports PList
 */
void visitTopPorts(graph* g, PNode* ports) {
  for (int i = 0; i < ports->getChildNum(); i++) {
    PNode* port = ports->getChild(i);
    Assert(port->getChildNum() == 1, "Invalid port %s\n", port->name.c_str());

    Node* io = allocNode();
    io->name = port->name;

    if (port->type == P_INPUT) io->type = NODE_INP;
    else if (port->type == P_OUTPUT) io->type = NODE_OUT;
    else Assert(0, "Invalid port %s with type %d\n", port->name.c_str(), port->type);

    visitType(io, port->getChild(0));
    addSignal(io->name, io);
    aggrAddLeaf(port->type == P_INPUT ? g->input : g->output, io);
  }
}

/**
 * @brief Visits the top-level module of a hardware description language design and extracts
 * information about its ports and statements.
 *
 * @param g Pointer to graph structure to be converted and stored.
 * @param topModule Pointer to a PNode object representing the top-level module of the HDL design.
 */
void visitTopModule(graph* g, PNode* topModule) {
  bool isValidTopModule = topModule->getChildNum() >= 2 && topModule->getChild(0)->type == P_PORTS;
  Assert(isValidTopModule, "Invalid top module %s\n", topModule->name.c_str());

  visitTopPorts(g, topModule->getChild(0));
  visitStmts("", g, topModule->getChild(1));
}

void aggrMergeResetCond(Node* node) {
  if(!node->aggrType) {
    node->workingVal->ops.insert(node->workingVal->ops.end(), node->resetCond->ops.begin(), node->resetCond->ops.end());
    node->workingVal->operands.insert(node->workingVal->operands.end(), node->resetCond->operands.begin(), node->resetCond->operands.end());
  } else {
    for (Node* n : node->aggrMember) aggrMergeResetCond(n);
  }
}

void aggrMergeResetValue(Node* node) {
  if(!node->aggrType) {
    node->workingVal->ops.insert(node->workingVal->ops.end(), node->resetVal->ops.begin(), node->resetVal->ops.end());
    node->workingVal->operands.insert(node->workingVal->operands.end(), node->resetVal->operands.begin(), node->resetVal->operands.end());
  } else {
    for (Node* n : node->aggrMember) aggrMergeResetValue(n);
  }
}

void aggrMergeValue(Node* node) {
  if(!node->aggrType) {
    if (node->value->ops.size() == 0 && node->value->operands.size() == 0) {
      node->workingVal->operands.push_back(node->regNext);
    } else {
      node->workingVal->ops.insert(node->workingVal->ops.end(), node->value->ops.begin(), node->value->ops.end());
      node->workingVal->operands.insert(node->workingVal->operands.end(), node->value->operands.begin(), node->value->operands.end());
    }
  } else {
    for (Node* n : node->aggrMember) aggrMergeValue(n);
  }
}

void addRegReset(graph* g) {
  for (Node* n : g->sources) {
    PNode* mux = new PNode(P_WHEN);
    Node* regDst = n->regNext;
    mux->width = regDst->width;
    aggrAllocWorkingVal(regDst);
    addAggrMemberOps(regDst, mux);
    aggrMergeResetCond(regDst);
    addAggrMemberOps(regDst, NULL);
    aggrMergeResetValue(regDst);
    addAggrMemberOps(regDst, NULL);
    aggrMergeValue(regDst);
    aggrSetiValue(regDst);
    aggrSetWorkingVal(regDst);
  }
}

/**
 * @brief tranform the ast to graph structure
 *
 * @param root the root node of ast.
 * @return graph * return the graph structure.
 */
graph* AST2Garph(PNode* root) {
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
  for (auto n : allSignals) {
    if (n.second->type == NODE_OTHERS && n.second->inEdge == 0 && !n.second->aggrType && n.second->dimension.size() == 0) {
      g->constant.push_back(n.second);
      n.second->status = CONSTANT_NODE;
    }
    if (n.second->dimension.size() != 0) {
      g->array.push_back(n.second);
      for (Node* member : n.second->member) {
        n.second->inEdge += member->inEdge;  // TODO: superNode & member
      }
    }
  }

  addRegReset(g);

  // Node* test = str2Node("dfa_0$m$finish");
  // std::cout << "inEdge " << test->inEdge << " status " << test->status << " dimension " << test->dimension.size() << std::endl;
  // for (size_t i = 0; i < test->dimension.size(); i ++) std::cout << test->dimension[i] << " ";
  // std::cout << std::endl;
  // DISP_INFO(test->workingVal);

  return g;
}
