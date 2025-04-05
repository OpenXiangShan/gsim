/*
  traverse AST / graph
*/

#include "common.h"
#include <tuple>
#include <stack>
#include <map>
#include "PNode.h"

static std::map<PNodeType, const char*> pname = {
  {P_EMPTY, "P_EMPTY"},       {P_CIRCUIT, "P_CIRCUIT"},         {P_MOD, "P_MOD"},                     {P_EXTMOD, "P_EXTMOD"},
  {P_INTMOD, "P_INTMOD"},     {P_PORTS, "P_PORTS"},             {P_INPUT, "P_INPUT"},                 {P_OUTPUT, "P_OUTPUT"},
  {P_WIRE_DEF, "P_WIRE_DEF"}, {P_REG_DEF, "P_REG_DEF"},         {P_REG_RESET_DEF, "P_REG_RESET_DEF"}, {P_INST, "P_INST"},
  {P_NODE, "P_NODE"},         {P_CONNECT, "P_CONNECT"},         {P_PAR_CONNECT, "P_PAR_CONNECT"},     {P_WHEN, "P_WHEN"},
  {P_MEMORY, "P_MEMORY"},     {P_SEQ_MEMORY, "P_SEQ_MEMORY"},   {P_COMB_MEMORY, "P_COMB_MEMORY"},     {P_WRITE, "P_WRITE"},
  {P_READ, "P_READ"},         {P_INFER, "P_INFER"},             {P_MPORT, "P_MPORT"},                 {P_READER, "P_READER"},
  {P_WRITER, "P_WRITER"},     {P_READWRITER, "P_READWRITER"},   {P_RUW, "P_RUW"},                     {P_RLATENCT, "P_RLATENCT"},
  {P_WLATENCT, "P_WLATENCT"}, {P_DATATYPE, "P_DATATYPE"},       {P_DEPTH, "P_DEPTH"},                 {P_REF, "P_REF"},
  {P_REF_DOT, "P_REF_DOT"},   {P_REF_IDX_INT, "P_REF_IDX_INT"}, {P_REF_IDX_EXPR, "P_REF_IDX_EXPR"},   {P_2EXPR, "P_2EXPR"},
  {P_1EXPR, "P_1EXPR"},       {P_1EXPR1INT, "P_1EXPR1INT"},     {P_1EXPR2INT, "P_1EXPR2INT"},         {P_FIELD, "P_FIELD"},
  {P_AG_ARRAY, "P_AG_ARRAY"}, {P_FLIP_FIELD, "P_FLIP_FIELD"},   {P_AG_FIELDS, "P_AG_FIELDS"},         {P_Clock, "P_Clock"},
  {P_ASYRESET, "P_ASYRESET"}, {P_INT_TYPE, "P_INT_TYPE"},       {P_EXPR_INT_NOINIT, "P_EXPR_INT_NOINIT"}, {P_EXPR_INT_INIT, "P_EXPR_INT_INIT"},
  {P_EXPR_MUX, "P_EXPR_MUX"}, {P_STATEMENTS, "P_STATEMENTS"},   {P_PRINTF, "P_PRINTF"},               {P_EXPRS, "P_EXPRS"},
  {P_ASSERT, "P_ASSERT"},     {P_INDEX, "P_INDEX"},             {P_CONS_INDEX, "P_CONS_INDEX"},       {P_L_CONS_INDEX, "P_L_CONS_INDEX"},
  {P_L_INDEX, "P_L_INDEX"},   {P_INVALID, "P_INVALID"}
};
/* Preorder traversal of AST */
void preorder_traversal(PNode* root) {
  std::stack<std::pair<PNode*, int>> s;
  s.push(std::make_pair(root, 0));
  PNode* node;
  int depth;
  while (!s.empty()) {
    std::tie(node, depth) = s.top();
    s.pop();
    printf("%s", std::string(depth * 2, ' ').c_str());
    if(!node) {
      std::cout << "NULL\n";
      continue;
    }
    printf("%s [%s] (lineno %d, width %d): %s\n", node->name.c_str(), pname[node->type], node->lineno, node->width, node->name.c_str());
    for (int i = node->getChildNum() - 1; i >= 0; i--) {
      s.push(std::make_pair(node->getChild(i), depth + 1));
    }
  }
  fflush(stdout);
}

static std::map<OPType, const char*> OP2Name = {
  {OP_INVALID, "invalid"}, {OP_MUX, "mux"}, {OP_ADD, "add"}, {OP_SUB, "sub"}, {OP_MUL, "mul"},
  {OP_DIV, "div"}, {OP_REM, "rem"}, {OP_LT, "lt"}, {OP_LEQ, "leq"}, {OP_GT, "gt"}, {OP_GEQ, "geq"},
  {OP_EQ, "eq"}, {OP_NEQ, "neq"}, {OP_DSHL, "dshl"}, {OP_DSHR, "dshr"}, {OP_AND, "and"},
  {OP_OR, "or"}, {OP_XOR, "xor"}, {OP_CAT, "cat"}, {OP_ASUINT, "asuint"}, {OP_ASSINT, "assint"},
  {OP_ASCLOCK, "asclock"}, {OP_ASASYNCRESET, "asasyncreset"}, {OP_CVT, "cvt"}, {OP_NEG, "neg"},
  {OP_NOT, "not"}, {OP_ANDR, "andr"}, {OP_ORR, "orr"}, {OP_XORR, "xorr"}, {OP_PAD, "pad"}, {OP_SHL, "shl"},
  {OP_SHR, "shr"}, {OP_HEAD, "head"}, {OP_TAIL, "tail"}, {OP_BITS, "bits"}, {OP_INDEX_INT, "index_int"},
  {OP_INDEX, "index"}, {OP_WHEN, "when"}, {OP_PRINTF, "printf"}, {OP_ASSERT, "assert"}, {OP_INT, "int"},
  {OP_READ_MEM, "readMem"}, {OP_WRITE_MEM, "writeMem"}, {OP_INFER_MEM, "inferMem"},
  {OP_RESET, "reset"}, {OP_STMT, "stmts"}, {OP_SEXT, "sext"}, {OP_BITS_NOSHIFT, "bits_noshift"},
  {OP_GROUP, "group"}, {OP_EXIT, "exit"}, {OP_EXT_FUNC, "ext_func"},
  {OP_STMT_SEQ, "stmt_seq"}, {OP_STMT_WHEN, "stmt_when"}, {OP_STMT_NODE, "stmt_node"}
};

static std::map<NodeType, const char*> NodeType2Name = {
  {NODE_INVALID, "invalid"}, {NODE_REG_SRC, "reg_src"}, {NODE_REG_DST, "reg_dst"}, {NODE_SPECIAL, "special"},
  {NODE_INP, "inp"}, {NODE_OUT, "out"}, {NODE_MEMORY, "memory"}, {NODE_READER, "reader"},
  {NODE_WRITER, "writer"}, {NODE_READWRITER, "readwriter"}, {NODE_MEM_MEMBER, "mem_member"},
  {NODE_OTHERS, "others"}, {NODE_REG_RESET, "reg_reset"}, {NODE_EXT, "ext"}, {NODE_EXT_IN, "ext_in"},
  {NODE_EXT_OUT, "ext_out"}
};

static std::map<NodeStatus, const char*> NodeStatus2Name = {
  {VALID_NODE, "valid"}, {DEAD_NODE, "dead"}, {CONSTANT_NODE, "constant"}, {MERGED_NODE, "merged"},
  {REPLICATION_NODE, "replication"}, {SPLITTED_NODE, "splitted"}
};

void ExpTree::display(int depth) {
  if (getlval()) getlval()->display(depth);
  if (getRoot()) getRoot()->display(depth);
}

void StmtTree::display() {
  if (!root) return;
  std::stack<std::pair<StmtNode*, int>> s;
  s.push(std::make_pair(root, 1));
  while (!s.empty()) {
    StmtNode* top;
    int depth;
    std::tie(top, depth) = s.top();
    s.pop();
    if (!top) {
      printf("%s(EMPTY)\n",std::string(depth * 2, ' ').c_str());
      continue;
    }
    printf("%s(%d %s) childNum %ld\n", std::string(depth * 2, ' ').c_str(), top->type,
           OP2Name[top->type], top->child.size()
          );
    if (top->type == OP_STMT_NODE) {
      if (top->isENode) top->enode->display(depth + 1);
      else top->tree->display(depth + 1);
    }
    for (int i = top->child.size() - 1; i >= 0; i --) {
      StmtNode* child = top->child[i];
      s.push(std::make_pair(child, depth + 1));
    }
  }
}

/* traverse graph */
void graph::traversal() {
  for (SuperNode* super : sortedSuper) {
    super->display();
  }
}

void SuperNode::display() {
  printf("----super %d(type=%d)----:\n", id, superType);
  for (Node* node : member) {
    node->display();
  }
  printf("[stmtTree]\n");
  if (stmtTree) stmtTree->display();
}


void Node::display() {
  printf("node %s[width %d sign %d status=%s type=%s lineno=%d][", name.c_str(), width, sign, NodeStatus2Name[status], NodeType2Name[type], lineno);
  for (int dim : dimension) printf(" %d", dim);
  printf(" ]\n");
  for (size_t i = 0; i < assignTree.size(); i ++) {
    printf("[assign] %ld\n", i);
    assignTree[i]->display();
  }
  if (resetTree) {
    printf("[resetTree]:\n");
    resetTree->display();
  }
#if 0
  for (Node* nextNode : next) {
    printf("    next %p %s\n", nextNode, nextNode->name.c_str());
  }
  for (Node* prevNode : prev) {
    printf("    prev %p %s\n", prevNode, prevNode->name.c_str());
  }
#endif
}

void ENode::display(int depth) {
  std::stack<std::pair<ENode*, int>> enodes;
  enodes.push(std::make_pair(this, depth));
  while (!enodes.empty()) {
    ENode* top;
    int depth;
    std::tie(top, depth) = enodes.top();
    enodes.pop();
    if (!top) {
      printf("%s(EMPTY)\n",std::string(depth * 2, ' ').c_str());
      continue;
    }
    printf("%s(%d %s %p) %s %s [width=%d, sign=%d, type=%s, lineno=%d]", std::string(depth * 2, ' ').c_str(), top->opType,
           OP2Name[top->opType], top, (top->nodePtr) ? top->nodePtr->name.c_str() : (top->opType == OP_READ_MEM || top->opType == OP_WRITE_MEM ? top->memoryNode->name.c_str() : ""),
           top->strVal.c_str(), top->width,
           top->sign, (top->nodePtr) ? NodeType2Name[top->nodePtr->type] : NodeType2Name[NODE_INVALID], top->nodePtr ? top->nodePtr->lineno : -1);
    for (int val : top->values) printf(" %d", val);
    printf("\n");
    for (int i = top->child.size() - 1; i >= 0; i --) {
      ENode* childENode = top->child[i];
      enodes.push(std::make_pair(childENode, depth + 1));
    }
  }
}

void graph::traversalNoTree() {
  int nodeIdx = 0;
  for (size_t idx = 0; idx < sortedSuper.size(); idx ++) {
    SuperNode* super = sortedSuper[idx];
    printf("---%ld super id = %d---\n", idx, super->id);
    for (Node* node : super->member) {
      printf("%d: %s (super %d)\n", nodeIdx ++, node->name.c_str(), super->id);
    }
  }
}
