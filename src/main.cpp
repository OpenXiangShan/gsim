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
  std::cout << "before parser\n";
  syntax.parse();
  std::cout << "after parser\n";
  preorder_traversal(root);
}