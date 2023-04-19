#include <stack>
#include <tuple>
#include "PNode.h"

const char* typeName[] = { "P_INVALID", "P_CIRCUIT", "P_CIR_MODS", "P_MOD", "P_EXTMOD", "P_INTMOD", "P_PORTS", "P_INPUT", "P_OUTPUT", \
                  "P_WIRE_DEF", "P_REG_DEF", "P_INST", "P_NODE", "P_CONNECT", "P_PAR_CONNECT", "P_WHEN", "P_ELSE", \
                  "P_MEMORY", "P_READER", "P_WRITER", "P_READWRITER", \
                  "P_RUW", "P_RLATENCT", "P_WLATENCT", "P_DATATYPE", "P_DEPTH", "P_REF", "P_REF_DOT", "P_REF_IDX", "P_EXPR_INT", \
                  "P_2EXPR", "P_1EXPR", "P_1EXPR1INT", "P_1EXPR2INT", "P_FIELD", "P_FLIP_FIELD", "P_AG_TYPE", "P_AG_FIELDS", \
                  "P_Clock", "P_INT_TYPE", "P_EXPR_INT_NOINIT", "P_EXPR_INT_INIT", "P_EXPR_MUX", "P_STATEMENTS", \
                  "P_PRINTF", "P_EXPRS", "P_ASSERT"};

void preorder_traversal(PNode* root) {
  std::stack<std::pair<PNode*, int>>s;
  s.push(std::make_pair(root, 0));
  PNode* node;
  int depth;
  while(!s.empty()) {
    std::tie(node, depth) = s.top();
    for(int i = 0; i < depth; i++) std::cout << "  ";
    std::cout << typeName[node->type] << " : " << node->name << std::endl;
    s.pop();
    for(int i = node->getChildNum()-1; i >= 0; i--) {
      s.push(std::make_pair(node->getChild(i), depth + 1));
    }
  }
}
