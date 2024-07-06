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
  if (EnableDumpGraph) g->dump("00Init");

  FUNC_WRAPPER(g->splitArray());

  FUNC_WRAPPER(g->detectLoop());
  
  FUNC_WRAPPER(inferAllWidth());

  FUNC_WRAPPER(g->topoSort());
  if (EnableDumpGraph) g->dump("01TopoSort");

  FUNC_WRAPPER(g->clockOptimize());
  if (EnableDumpGraph) g->dump("02ClockOptimize");

  FUNC_WRAPPER(g->removeDeadNodes());
  if (EnableDumpGraph) g->dump("03RemoveDeadNodes");

  FUNC_WRAPPER(g->exprOpt());
  if (EnableDumpGraph) g->dump("04ExprOpt");
  // FUNC_WRAPPER(g->traversal());
  FUNC_WRAPPER(g->usedBits());

  FUNC_WRAPPER(g->constantAnalysis());
  if (EnableDumpGraph) g->dump("05ConstantAnalysis");

  FUNC_WRAPPER(g->removeDeadNodes());
  if (EnableDumpGraph) g->dump("06RemoveDeadNodes");

  FUNC_WRAPPER(g->aliasAnalysis());
  if (EnableDumpGraph) g->dump("07AliasAnalysis");

  FUNC_WRAPPER(g->commonExpr());
  if (EnableDumpGraph) g->dump("08CommonExpr");

  FUNC_WRAPPER(g->removeDeadNodes());
  if (EnableDumpGraph) g->dump("09RemoveDeadNodes");

  FUNC_WRAPPER(g->mergeNodes());
  if (EnableDumpGraph) g->dump("10MergeNodes");

  FUNC_WRAPPER(g->mergeRegister());
  if (EnableDumpGraph) g->dump("11MergeRegister");
  FUNC_WRAPPER(g->constructRegs());
  if (EnableDumpGraph) g->dump("12ConstructRegs");

  // g->mergeRegister();
 
  FUNC_WRAPPER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter());

  if (EnableDumpGraph) g->dump("13Final");

  return 0;
}
