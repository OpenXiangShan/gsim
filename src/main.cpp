/**
 * @file main.cpp
 * @brief start entry
 */

#include <stdio.h>

#include "common.h"
#include "graph.h"

#include <sstream>
#include <getopt.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef GSIM_VERSION
#define GSIM_VERSION "UNKNOWN"
#endif

#ifndef GSIM_BUILD_DATE
#define GSIM_BUILD_DATE "UNKNOWN"
#endif

#ifndef GSIM_CXX_VERSION
#define GSIM_CXX_VERSION "UNKNOWN"
#endif

PNode* parseFIR(char *strbuf);
void preorder_traversal(PNode* root);
graph* AST2Graph(PNode* root);
void inferAllWidth();

Config::Config() {
  EnableDumpGraph = false;
  DumpGraphDot = false;
  DumpGraphJson = false;
  DumpAssignTree = false;
  DumpConstStatus = false;
  OutputDir = ".";
  SuperNodeMaxSize = 35;
  cppMaxSizeKB = -1;
  sep_module = "$";
  sep_aggr = "$$";
  MergeWhenSize = 5;
  When2muxBound = 2;
  LogLevel = 0;
}
Config globalConfig;

static std::set<std::string> parseStageList(const std::string& arg) {
  std::set<std::string> stages;
  std::stringstream ss(arg);
  std::string token;
  while (std::getline(ss, token, ',')) {
    if (!token.empty()) stages.insert(token);
  }
  return stages;
}

static inline bool shouldDumpStage(const std::string& name) {
  return globalConfig.DumpStages.empty() || globalConfig.DumpStages.count(name);
}


#define FUNC_WRAPPER_INTERNAL(func, name, dumpCond) \
  do { \
    if (globalConfig.LogLevel > 0 && strlen(name) != 0) { \
      fprintf(stderr, "[GSIM] %s begin\n", name); \
      fflush(stderr); \
    } \
    struct timeval start = getTime(); \
    func; \
    struct timeval end = getTime(); \
    const char* label = strlen(name) ? name : "{" #func "}"; \
    showTime(label, start, end); \
    if (globalConfig.LogLevel > 0 && strlen(name) != 0) { \
      fprintf(stderr, "[GSIM] %s done\n", name); \
      fflush(stderr); \
    } \
    if (dumpCond && globalConfig.EnableDumpGraph && shouldDumpStage(name)) g->dump(std::to_string(dumpIdx ++) + name); \
  } while(0)

#define FUNC_TIMER(func)         FUNC_WRAPPER_INTERNAL(func, "", false)
#define FUNC_WRAPPER(func, name) FUNC_WRAPPER_INTERNAL(func, name, true)

static void printVersionBrief() {
  std::cout << "GSIM " << GSIM_VERSION << "\n";
}

static void printVersionDetail() {
  std::cout << "GSIM " << GSIM_VERSION << "\n"
            << "Build Date: " << GSIM_BUILD_DATE << "\n"
            << "Build CXX: " << GSIM_CXX_VERSION << "\n";
}

static void printUsage(const char* ProgName) {
  printVersionBrief();
  std::cout << "Usage: " << ProgName << " [options] <input file>\n"
            << "Options:\n"
            << "  -h, --help                       Display this help message and exit.\n"
            << "  -v, --version                    Display version/build information and exit.\n"
            << "      --dump                       Enable dumping of the graph.\n"
            << "      --dir=[dir]                  Specify the output directory.\n"
            << "      --supernode-max-size=[num]   Specify the maximum size of a superNode.\n"
            << "      --cpp-max-size-KB=[num]      Specify the maximum size (approximate) of a generated C++ file.\n"
            << "      --sep-mod=[str]              Specify the seperator for submodule (default: $).\n"
            << "      --sep-aggr=[str]             Specify the seperator for aggregate member (default: $$).\n"
            << "      --when-size=[num]            Bound for merging nested when blocks (default: 5).\n"
            << "      --when2mux-bound=[num]       Bound for converting when to mux (default: 2).\n"
            << "      --log-level=[0|1|2]          Verbosity for additional logs.\n"
            << "      --dump-json                  Dump graphs in JSON (disable dot unless --dump-dot is also set).\n"
            << "      --dump-dot                   Dump graphs in DOT (disable json unless --dump-json is also set).\n"
            << "      --dump-stages=a,b,c          Dump only the listed stages (e.g., Init,TopoSort,AliasAnalysis).\n"
            << "      --dump-assign-tree           Include assignTree structure in JSON dump (can be large).\n"
            << "      --dump-const-status          Dump per-node constant-analysis status before removing constants.\n"
            ;
}

static char* parseCommandLine(int argc, char** argv) {
  if (argc <= 1) {
    printUsage(argv[0]);
    std::cout.flush();
    fflush(nullptr);
    _exit(EXIT_SUCCESS); // avoid running atexit/destructors which crash in full-static
  }

  enum LongOptionIndex {
    OPT_HELP = 0,
    OPT_VERSION,
    OPT_DUMP,
    OPT_DIR,
    OPT_SUPER_MAX,
    OPT_CPP_MAX,
    OPT_SEP_MOD,
    OPT_SEP_AGGR,
    OPT_WHEN_SIZE,
    OPT_WHEN2MUX,
    OPT_LOG_LEVEL,
    OPT_DUMP_JSON,
    OPT_DUMP_DOT,
    OPT_DUMP_STAGES,
    OPT_DUMP_ASSIGN_TREE,
    OPT_DUMP_CONST_STATUS,
  };

  const struct option Table[] = {
      {"help", no_argument, nullptr, 'h'},
      {"version", no_argument, nullptr, 'v'},
      {"dump", no_argument, nullptr, 'd'},
      {"dir", required_argument, nullptr, 0},
      {"supernode-max-size", required_argument, nullptr, 0},
      {"cpp-max-size-KB", required_argument, nullptr, 0},
      {"sep-mod", required_argument, nullptr, 0},
      {"sep-aggr", required_argument, nullptr, 0},
      {"when-size", required_argument, nullptr, 0},
      {"when2mux-bound", required_argument, nullptr, 0},
      {"log-level", required_argument, nullptr, 0},
      {"dump-json", no_argument, nullptr, 0},
      {"dump-dot", no_argument, nullptr, 0},
      {"dump-stages", required_argument, nullptr, 0},
      {"dump-assign-tree", no_argument, nullptr, 0},
      {"dump-const-status", no_argument, nullptr, 0},
      {nullptr, no_argument, nullptr, 0},
  };

  bool explicitJson = false;
  bool explicitDot = false;
  int Option{0};
  int option_index;
  while ((Option = getopt_long(argc, argv, "-hv", Table, &option_index)) != -1) {
    switch (Option) {
      case 0: switch (option_index) {
                case OPT_DUMP:
                  globalConfig.EnableDumpGraph = true;
                  globalConfig.DumpGraphDot = true;
                  globalConfig.DumpGraphJson = true;
                  explicitJson = explicitDot = false;
                  break;
                case OPT_DIR: globalConfig.OutputDir = optarg; break;
                case OPT_SUPER_MAX: sscanf(optarg, "%d", &globalConfig.SuperNodeMaxSize); break;
                case OPT_CPP_MAX: sscanf(optarg, "%d", &globalConfig.cppMaxSizeKB); break;
                case OPT_SEP_MOD: globalConfig.sep_module = optarg; break;
                case OPT_SEP_AGGR: globalConfig.sep_aggr = optarg; break;
                case OPT_WHEN_SIZE: sscanf(optarg, "%d", &globalConfig.MergeWhenSize); break;
                case OPT_WHEN2MUX: sscanf(optarg, "%d", &globalConfig.When2muxBound); break;
                case OPT_LOG_LEVEL: sscanf(optarg, "%d", &globalConfig.LogLevel); break;
                case OPT_DUMP_JSON:
                  if (explicitDot) {
                    fprintf(stderr, "Error: --dump-json and --dump-dot cannot be used together.\n");
                    printUsage(argv[0]);
                    std::cout.flush();
                    fflush(nullptr);
                    _exit(EXIT_FAILURE);
                  }
                  globalConfig.EnableDumpGraph = true;
                  globalConfig.DumpGraphJson = true;
                  explicitJson = true;
                  globalConfig.DumpGraphDot = false;
                  break;
                case OPT_DUMP_DOT:
                  if (explicitJson) {
                    fprintf(stderr, "Error: --dump-json and --dump-dot cannot be used together.\n");
                    printUsage(argv[0]);
                    std::cout.flush();
                    fflush(nullptr);
                    _exit(EXIT_FAILURE);
                  }
                  globalConfig.EnableDumpGraph = true;
                  globalConfig.DumpGraphDot = true;
                  explicitDot = true;
                  globalConfig.DumpGraphJson = false;
                  break;
                case OPT_DUMP_STAGES:
                  globalConfig.EnableDumpGraph = true;
                  globalConfig.DumpStages = parseStageList(optarg);
                  break;
                case OPT_DUMP_ASSIGN_TREE:
                  globalConfig.DumpAssignTree = true;
                  break;
                case OPT_DUMP_CONST_STATUS:
                  globalConfig.DumpConstStatus = true;
                  break;
                default: printUsage(argv[0]); std::cout.flush(); fflush(nullptr); _exit(EXIT_SUCCESS);
              }
              break;
      case 1: return optarg; // InputFileName
      case 'd':
        globalConfig.EnableDumpGraph = true;
        globalConfig.DumpGraphDot = true;
        globalConfig.DumpGraphJson = true;
        break;
      case 'v':
        printVersionDetail();
        std::cout.flush();
        fflush(nullptr);
        _exit(EXIT_SUCCESS);
      case 'h':
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

  if (globalConfig.EnableDumpGraph && shouldDumpStage("AfterSplitNodes")) g->dump(std::to_string(dumpIdx ++) + "AfterSplitNodes");

  FUNC_WRAPPER(g->removeDeadNodes(), "RemoveDeadNodes1");

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
