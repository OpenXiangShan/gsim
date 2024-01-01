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

extern PNode* root;

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

  syntax.parse();

  MUX_DEBUG(std::cout << "parser finished\n");

  graph* g = AST2Graph(root);

  MUX_DEBUG(preorder_traversal(root));

  g->detectLoop();
  
  g->usedBits();

  g->topoSort();

  // g->traversal();

  g->instsGenerator();
  
  g->cppEmitter();

  return 0;
}
