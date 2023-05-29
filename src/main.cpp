#include <stdio.h>
#include "common.h"
#include "Node.h"
#include "graph.h"
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
extern PNode* root;

int main(int argc, char** argv) {
  // graph g;
  // randomGraph(&g);
  // generator(&g, "top", "top");
  if(argc <= 1) {
    std::cout << "Usage: \n";
  }
  std::ifstream infile(argv[1]);
  Parser::Lexical lexical{infile, std::cout};
  Parser::Syntax syntax{&lexical};
  syntax.parse();
  MUX_DEBUG(std::cout << "after parser\n");
  MUX_DEBUG(preorder_traversal(root));
  graph* g = AST2Garph(root);
  loopDetector(g);
  MUX_DEBUG(std::cout << "graph generated\n");
  topoSort(g);
  removeDeadNodes(g);
  generator(g, "top", "top");
  return 0;
}