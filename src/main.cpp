/**
 * @file main.cpp
 * @brief start entry
 */

#include <stdio.h>

#include "common.h"
#include "graph.h"
#include "Node.h"
#include "PNode.h"
#include "syntax.hh"
#include "Parser.h"

void generator(graph* g, std::string header, std::string src);
void randomGraph(graph* g);
void preorder_traversal(PNode* root);
graph* AST2Garph(PNode* root);
void loopDetector(graph* g);
void topoSort(graph* g);
void removeDeadNodes(graph* g);
void constantPropagation(graph* g);
void instsGenerator(graph* g);
void mergeNodes(graph* g);
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

  MUX_DEBUG(std::cout << "after parser\n");

  graph* g = AST2Garph(root);

  MUX_DEBUG(preorder_traversal(root));

  loopDetector(g);

  MUX_DEBUG(std::cout << "graph generated\n");

  topoSort(g);

  removeDeadNodes(g);

  constantPropagation(g);

  mergeNodes(g);

  instsGenerator(g);

  for (Node* n : g->constant) {
    if (n->status != DEAD_NODE && n->status != CONSTANT_NODE)
      std::cout << "check: " << n->type << " " << n->status << " " << n->name << std::endl;
  }

  generator(g, "top", "top");

  return 0;
}
