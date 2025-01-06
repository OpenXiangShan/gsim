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
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

void preorder_traversal(PNode* root);
graph* AST2Graph(PNode* root);
void inferAllWidth();

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


static bool EnableDumpGraph{false};

static void printUsage(const char* ProgName) {
  std::cout << "Usage: " << ProgName << " [options] <input file>\n"
            << "Options:\n"
            << "  -h, --help           Display this help message and exit.\n"
            << "      --dump           Enable dumping of the graph.\n";
}

static char* parseCommandLine(int argc, char** argv) {
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
      case 1: return optarg; // InputFileName
      case 'd': EnableDumpGraph = true; break;
      default: {
        printUsage(argv[0]);
        exit(EXIT_SUCCESS);
      }
    }
  }
  assert(0);
}

static char* readFile(const char *InputFileName, size_t &size, size_t &mapSize) {
  int fd = open(InputFileName, O_RDONLY);
  assert(fd != -1);
  struct stat sb;
  int ret = fstat(fd, &sb);
  assert(ret != -1);
  size = sb.st_size + 1;
  mapSize = (size + 4095) & ~4095L;
  char *buf = (char *)mmap(NULL, mapSize, PROT_READ | PROT_WRITE, MAP_PRIVATE, fd, 0);
  assert(buf != (void *)-1);
  buf[size - 1] = '\0';
  close(fd);
  return buf;
}

/**
 * @brief main function.
 *
 * @param argc arg count.
 * @param argv arg value string.
 * @return exit state.
 */
int main(int argc, char** argv) {
  const char *InputFileName = parseCommandLine(argc, argv);

  size_t size = 0, mapSize = 0;
  char *strbuf;
  FUNC_TIMER(strbuf = readFile(InputFileName, size, mapSize));

  std::istringstream *streamBuf = new std::istringstream(strbuf);
  Parser::Lexical *lexical = new Parser::Lexical(*streamBuf, std::cout);
  Parser::Syntax *syntax = new Parser::Syntax(lexical);

  FUNC_TIMER(syntax->parse());
  PNode* globalRoot = lexical->root;
  delete syntax;
  delete lexical;
  delete streamBuf;

  munmap(strbuf, mapSize);

  graph* g;
  static int dumpIdx = 0;
  FUNC_WRAPPER(g = AST2Graph(globalRoot), "Init");

  FUNC_TIMER(g->splitArray());

  FUNC_TIMER(g->detectLoop());

  FUNC_WRAPPER(g->topoSort(), "TopoSort");

  FUNC_TIMER(g->inferAllWidth());

  FUNC_WRAPPER(g->clockOptimize(), "ClockOptimize");

  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");

  FUNC_WRAPPER(g->exprOpt(), "ExprOpt");

  FUNC_TIMER(g->usedBits());

  FUNC_TIMER(g->splitNodes());

  FUNC_TIMER(g->removeDeadNodes());

  FUNC_WRAPPER(g->constantAnalysis(), "ConstantAnalysis");

  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");

  FUNC_WRAPPER(g->aliasAnalysis(), "AliasAnalysis");

  FUNC_WRAPPER(g->patternDetect(), "PatternDetect");

  FUNC_WRAPPER(g->commonExpr(), "CommonExpr");

  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes");

  // FUNC_WRAPPER(g->mergeNodes(), "MergeNodes");
  FUNC_WRAPPER(g->graphPartition(), "graphPartition");

  FUNC_WRAPPER(g->replicationOpt(), "Replication");

  FUNC_WRAPPER(g->mergeRegister(), "MergeRegister");

  FUNC_WRAPPER(g->constructRegs(), "ConstructRegs");
 
  FUNC_TIMER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter(), "Final");


  return 0;
}
