/**
 * @file main.cpp
 * @brief start entry
 */

#include <stdio.h>

#include "common.h"
#include "graph.h"
#include "syntax.hh"
#include "Parser.h"

void preorder_traversal(PNode* root);
graph* AST2Graph(PNode* root);
void inferAllWidth();

extern PNode* root;

#define FUNC_WRAPPER(func) \
  do { \
    clock_t start = clock(); \
    func; \
    clock_t end = clock(); \
    printf("{" #func "} = %ld s\n", (end - start) / CLOCKS_PER_SEC); \
  } while(0)

/**
 * @brief main function.
 *
 * @param argc arg count.
 * @param argv arg value string.
 * @return exit state.
 */
int main(int argc, char** argv) {
  if (argc <= 1) { std::cout << "Usage: \n"; }

  std::ifstream infile(argv[1]);

  Parser::Lexical lexical{infile, std::cout};
  Parser::Syntax syntax{&lexical};

  FUNC_WRAPPER(syntax.parse());

  MUX_DEBUG(std::cout << "parser finished\n");

  graph* g;
  FUNC_WRAPPER(g = AST2Graph(root));

  MUX_DEBUG(preorder_traversal(root));

  FUNC_WRAPPER(g->splitArray());

  FUNC_WRAPPER(g->detectLoop());
  
  FUNC_WRAPPER(inferAllWidth());

  FUNC_WRAPPER(g->topoSort());

  FUNC_WRAPPER(g->clockOptimize());

  FUNC_WRAPPER(g->removeDeadNodes());

  // FUNC_WRAPPER(g->traversal());
  FUNC_WRAPPER(g->usedBits());

  FUNC_WRAPPER(g->constantAnalysis());

  FUNC_WRAPPER(g->removeDeadNodes());

  FUNC_WRAPPER(g->aliasAnalysis());

  // FUNC_WRAPPER(g->commonExpr());

  FUNC_WRAPPER(g->constructRegs());

  FUNC_WRAPPER(g->mergeNodes());

  // g->mergeRegister();

  FUNC_WRAPPER(g->addL1Tree());
 
  FUNC_WRAPPER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter());

  return 0;
}
