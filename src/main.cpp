/**
 * @file main.cpp
 * @brief start entry
 */

#include <stdio.h>

#include "common.h"
#include "graph.h"
#include "syntax.hh"
#include "Parser.h"

#include <getopt.h>

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

static std::string InputFileName{""};
static bool EnableDumpGraph{false};

static void printUsage(const char* ProgName) {
  std::cout << "Usage: " << ProgName << " [options] <input file>\n"
            << "Options:\n"
            << "  -h, --help           Display this help message and exit.\n"
            << "      --dump           Enable dumping of the graph.\n";
}

static void parseCommandLine(int argc, char** argv) {
  if (argc <= 1) {
    printUsage(argv[0]);
    exit(EXIT_SUCCESS);
  }

  const struct option Table[] = {
      {"help", no_argument, nullptr, 'h'},
      {"dump", no_argument, nullptr, 'd'},
      {nullptr, no_argument, nullptr, 0},
  };

  int Option{0};
  while ((Option = getopt_long(argc, argv, "-h", Table, nullptr)) != -1) {
    switch (Option) {
      case 1: InputFileName = optarg; return;
      case 'd': EnableDumpGraph = true; break;
      default: {
        printUsage(argv[0]);
        exit(EXIT_SUCCESS);
      }
    }
  }
}

/**
 * @brief main function.
 *
 * @param argc arg count.
 * @param argv arg value string.
 * @return exit state.
 */
int main(int argc, char** argv) {
  parseCommandLine(argc, argv);

  std::ifstream infile(InputFileName);

  Parser::Lexical lexical{infile, std::cout};
  Parser::Syntax syntax{&lexical};

  FUNC_WRAPPER(syntax.parse());
  graph* g;
  FUNC_WRAPPER(g = AST2Graph(root));

  FUNC_WRAPPER(g->splitArray());

  FUNC_WRAPPER(g->detectLoop());
  
  FUNC_WRAPPER(inferAllWidth());

  FUNC_WRAPPER(g->topoSort());

  FUNC_WRAPPER(g->clockOptimize());

  FUNC_WRAPPER(g->removeDeadNodes());

  FUNC_WRAPPER(g->exprOpt());
  // FUNC_WRAPPER(g->traversal());
  FUNC_WRAPPER(g->usedBits());

  FUNC_WRAPPER(g->constantAnalysis());

  FUNC_WRAPPER(g->removeDeadNodes());

  FUNC_WRAPPER(g->aliasAnalysis());

  FUNC_WRAPPER(g->commonExpr());

  FUNC_WRAPPER(g->removeDeadNodes());

  FUNC_WRAPPER(g->mergeNodes());

  FUNC_WRAPPER(g->mergeRegister());
  FUNC_WRAPPER(g->constructRegs());

  // g->mergeRegister();
 
  FUNC_WRAPPER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter());

  if (EnableDumpGraph)
    g->dump();

  return 0;
}
