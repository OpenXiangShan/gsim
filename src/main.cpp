/**
 * @file main.cpp
 * @brief start entry
 */

#include <stdio.h>

#include "common.h"
#include "graph.h"

#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include "circt/Dialect/SV/SVDialect.h"
#include "circt/Dialect/SV/SVOps.h"
#include "circt/Dialect/HW/HWDialect.h"
#include "circt/Dialect/HW/HWOps.h"
#include "circt/Dialect/Comb/CombDialect.h"
#include "circt/Dialect/Comb/CombOps.h"
#include "circt/Dialect/Seq/SeqDialect.h"
#include "circt/Dialect/Seq/SeqOps.h"
#include "circt/Dialect/Emit/EmitDialect.h"
#include "circt/Dialect/Emit/EmitOps.h"
#include "circt/Dialect/OM/OMDialect.h"
#include "circt/Dialect/OM/OMOps.h"
#include "mlir/IR/AsmState.h"
#include "mlir/IR/BuiltinOps.h"
#include "mlir/IR/MLIRContext.h"
#include "mlir/Parser/Parser.h"
#include "llvm/Support/CommandLine.h"
#include "llvm/Support/ErrorOr.h"
#include "llvm/Support/MemoryBuffer.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"


using namespace mlir;
using namespace circt::hw;
using namespace circt::sv;
using namespace circt::comb;
using namespace circt::seq;
using namespace circt::emit;
using namespace circt::om;


static llvm::cl::opt<std::string>
    inputFile(llvm::cl::Positional, llvm::cl::Required,
              llvm::cl::desc("<input .mlir file>"));

PNode* parseFIR(char *strbuf);
void preorder_traversal(PNode* root);
graph* AST2Graph(PNode* root);
void inferAllWidth();

Config::Config() {
  EnableDumpGraph = false;
  OutputDir = ".";
  SuperNodeMaxSize = 35;
  cppMaxSizeKB = -1;
  sep_module = "$";
  sep_aggr = "$$";
  MergeWhenSize = 5;
  When2muxBound = 2;
}
Config globalConfig;


#define FUNC_WRAPPER_INTERNAL(func, name, dumpCond) \
  do { \
    struct timeval start = getTime(); \
    func; \
    struct timeval end = getTime(); \
    showTime("{" #func "}", start, end); \
    if (dumpCond && globalConfig.EnableDumpGraph) g->dump(std::to_string(dumpIdx ++) + name); \
  } while(0)

#define FUNC_TIMER(func)         FUNC_WRAPPER_INTERNAL(func, "", false)
#define FUNC_WRAPPER(func, name) FUNC_WRAPPER_INTERNAL(func, name, true)

static void printUsage(const char* ProgName) {
  std::cout << "Usage: " << ProgName << " [options] <input file>\n"
            << "Options:\n"
            << "  -h, --help                       Display this help message and exit.\n"
            << "      --dump                       Enable dumping of the graph.\n"
            << "      --dir=[dir]                  Specify the output directory.\n"
            << "      --supernode-max-size=[num]   Specify the maximum size of a superNode.\n"
            << "      --cpp-max-size-KB=[num]      Specify the maximum size (approximate) of a generated C++ file.\n"
            << "      --sep-mod=[str]              Specify the seperator for submodule (default: $).\n"
            << "      --sep-aggr=[str]             Specify the seperator for aggregate member (default: $$).\n"
            ;
}

static char* parseCommandLine(int argc, char** argv) {
  if (argc <= 1) {
    printUsage(argv[0]);
    std::cout.flush();
    fflush(nullptr);
    _exit(EXIT_SUCCESS); // avoid running atexit/destructors which crash in full-static
  }

  const struct option Table[] = {
      {"help", no_argument, nullptr, 'h'},
      {"dump", no_argument, nullptr, 'd'},
      {"dir", required_argument, nullptr, 0},
      {"supernode-max-size", required_argument, nullptr, 0},
      {"cpp-max-size-KB", required_argument, nullptr, 0},
      {"sep-mod", required_argument, nullptr, 0},
      {"sep-aggr", required_argument, nullptr, 0},
      {"when-size", required_argument, nullptr, 0},
      {"when2mux-bound", required_argument, nullptr, 0},
      {nullptr, no_argument, nullptr, 0},
  };

  int Option{0};
  int option_index;
  while ((Option = getopt_long(argc, argv, "-h", Table, &option_index)) != -1) {
    switch (Option) {
      case 0: switch (option_index) {
                case 1: globalConfig.EnableDumpGraph = true; break;
                case 2: globalConfig.OutputDir = optarg; break;
                case 3: sscanf(optarg, "%d", &globalConfig.SuperNodeMaxSize); break;
                case 4: sscanf(optarg, "%d", &globalConfig.cppMaxSizeKB); break;
                case 5: globalConfig.sep_module = optarg; break;
                case 6: globalConfig.sep_aggr = optarg; break;
                case 7: sscanf(optarg, "%d", &globalConfig.MergeWhenSize); break;
                case 8: sscanf(optarg, "%d", &globalConfig.When2muxBound); break;
                case 0:
                default: printUsage(argv[0]); std::cout.flush(); fflush(nullptr); _exit(EXIT_SUCCESS);
              }
              break;
      case 1: return optarg; // InputFileName
      case 'd': globalConfig.EnableDumpGraph = true; break;
      default: {
        printUsage(argv[0]);
        std::cout.flush();
        fflush(nullptr);
        _exit(EXIT_SUCCESS);
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

  llvm::cl::ParseCommandLineOptions(argc, argv, "CIRCT HW walk example\n");

  // 1. 创建 MLIR 上下文并注册 HW 方言
  MLIRContext context;
  context.loadDialect<HWDialect>();
  context.loadDialect<SVDialect>();
  context.loadDialect<CombDialect>();
  context.loadDialect<SeqDialect>();
  context.loadDialect<EmitDialect>();
  context.loadDialect<OMDialect>();

  // 2. 把文件读进 SourceMgr
  llvm::ErrorOr<std::unique_ptr<llvm::MemoryBuffer>> fileOrErr =
      llvm::MemoryBuffer::getFile(inputFile);
  if (std::error_code ec = fileOrErr.getError()) {
    llvm::errs() << "Cannot open '" << inputFile << "': " << ec.message() << "\n";
    return 1;
  }
  llvm::SourceMgr sourceMgr;
  sourceMgr.AddNewSourceBuffer(std::move(*fileOrErr), SMLoc());

  // 3. 解析成 ModuleOp
  OwningOpRef<ModuleOp> module = parseSourceFile<ModuleOp>(sourceMgr, &context);
  if (!module) {
    llvm::errs() << "Error parsing " << inputFile << "\n";
    return 1;
  }

  // 4. 遍历模块内所有操作（含嵌套）
  module->walk([&](Operation *op) {
    // 只打印 HW 方言的操作，可按需要过滤
    if (isa<HWDialect>(op->getDialect()))
      llvm::outs() << "HW op: " << op->getName() << " at "
                   << op->getLoc() << "\n";
  });

  return 0;

  TIMER_START(total);
  graph* g = NULL;
  static int dumpIdx = 0;
  const char *InputFileName = parseCommandLine(argc, argv);

  size_t size = 0, mapSize = 0;
  char *strbuf;
  FUNC_TIMER(strbuf = readFile(InputFileName, size, mapSize));

  PNode* globalRoot;
  FUNC_TIMER(globalRoot= parseFIR(strbuf));
  munmap(strbuf, mapSize);

  FUNC_WRAPPER(g = AST2Graph(globalRoot), "Init");

  FUNC_TIMER(g->splitArray());

  FUNC_TIMER(g->detectLoop());

  FUNC_WRAPPER(g->topoSort(), "TopoSort");

  FUNC_TIMER(g->inferAllWidth());

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

  FUNC_WRAPPER(g->graphPartition(), "graphPartition");

  FUNC_WRAPPER(g->replicationOpt(), "Replication");

  // FUNC_WRAPPER(g->mergeRegister(), "MergeRegister");

  // FUNC_WRAPPER(g->constructRegs(), "ConstructRegs");
  FUNC_TIMER(g->generateStmtTree());

  FUNC_TIMER(g->instsGenerator());

  FUNC_WRAPPER(g->cppEmitter(), "Final");

  TIMER_END(total);

  return 0;
}
