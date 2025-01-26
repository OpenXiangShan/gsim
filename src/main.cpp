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

#define FUNC_TIMER(func) \
  do { \
    clock_t start = clock(); \
    func; \
    clock_t end = clock(); \
    printf("{" #func "} = %ld s\n", (end - start) / CLOCKS_PER_SEC); \
  } while(0)

#define FUNC_WRAPPER(func, name) \
  do { \
    clock_t start = clock(); \
    func; \
    clock_t end = clock(); \
    printf("{" #func "} = %ld s\n", (end - start) / CLOCKS_PER_SEC); \
    if (EnableDumpGraph) g->dump(std::to_string(dumpIdx ++) + name); \
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

  FUNC_TIMER(syntax.parse());
  graph* g;
  static int dumpIdx = 0;
  FUNC_WRAPPER(g = AST2Graph(root), "Init");

  FUNC_TIMER(g->splitArray());

  FUNC_TIMER(g->detectLoop());

  FUNC_TIMER(inferAllWidth());

  FUNC_WRAPPER(g->topoSort(), "TopoSort");
  g->countIR();

#if NODE_EXPR_OPT
  FUNC_WRAPPER(g->clockOptimize(), "ClockOptimize");
#if DUPLICATE_OPT
  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");
#endif
  FUNC_WRAPPER(g->exprOpt(), "ExprOpt");

  // FUNC_TIMER(g->usedBits());

#endif

  // FUNC_TIMER(g->splitNodes());
#if DUPLICATE_OPT
  FUNC_TIMER(g->removeDeadNodes());
#endif

#if NODE_EXPR_OPT
  FUNC_WRAPPER(g->constantAnalysis(), "ConstantAnalysis");
#if DUPLICATE_OPT
  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");
#endif
#endif
#if DUPLICATE_OPT
  FUNC_WRAPPER(g->aliasAnalysis(), "AliasAnalysis");
#endif


#if NODE_EXPR_OPT
  FUNC_WRAPPER(g->patternDetect(), "PatternDetect");
#endif

#if NODE_DUP_OPT
  FUNC_WRAPPER(g->commonExpr(), "CommonExpr");
#endif
#if DUPLICATE_OPT
  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");
#endif
// g->traversal();
  // FUNC_WRAPPER(g->mergeNodes(), "MergeNodes");
  //  FUNC_WRAPPER(g->graphPartition(), "graphPartition");
// g->traversal();
  // FUNC_WRAPPER(g->replicationOpt(), "Replication");
// g->traversal();
  FUNC_WRAPPER(g->mergeRegister(), "MergeRegister");

  FUNC_WRAPPER(g->constructRegs(), "ConstructRegs");

  FUNC_TIMER(g->perfAnalysis());

  FUNC_TIMER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter(), "Final");


  return 0;
}
