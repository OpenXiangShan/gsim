/*
  cppEmitter: emit C++ files for simulation
*/

#include "common.h"
#include "util.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#define ACTIVE_WIDTH 8
static constexpr size_t kResetAssignmentsPerChunk = 1024;

#define ENABLE_ACTIVATOR false

#ifdef DIFFTEST_PER_SIG
FILE* sigFile = nullptr;
#endif

#define RESET_NAME(node) (node->name + "$RESET")
#define emitFuncDecl(indent, ...) __emitSrc(indent, true, true, NULL, __VA_ARGS__)
#define emitBodyLock(indent, ...) __emitSrc(indent, false, false, NULL, __VA_ARGS__)

#define ActiveType std::tuple<uint64_t, std::string, int>
#define ACTIVE_MASK(active) std::get<0>(active)
#define ACTIVE_COMMENT(active) std::get<1>(active)
#define ACTIVE_UNIQUE(active) std::get<2>(active)

static int superId = 0;
static int activeFlagNum = 0;
static std::set<Node*> definedNode;
static std::map<int, SuperNode*> cppId2Super;
static std::set<int> alwaysActive;
static std::set<int> rcAlways[64];
static int rcFlagWords[64];
static int rcCurPart = -1;
static bool rcInWorker = false;
static std::string gActiveFlags = "activeFlags";
static int rcEmitPart = -1;
static bool rcEmitEval = false;
static int rcChunks[64];
static int rcCommitChunks[64];
static bool rcDeferForeign = false;
static bool rcHasCommitPend = false;
static FILE* rcHeader = nullptr;
// One bit per committed register that has readers on another partition.
// The owner sets it while comparing old and new; readers apply it after the
// commit barrier. Local readers still update their own activeFlags directly.
struct RcRegChange {
  int owner = 0;
  int index = 0;
  std::map<int, std::set<int>> destIds;
};
static std::vector<RcRegChange> rcRegChanges;
static std::unordered_map<Node*, size_t> rcRegChangeOf;
static int rcRegChangeWords[64];
static bool rcHasRegChanges = false;
static std::vector<SuperNode*> rcEvalSupers[64];
static std::vector<SuperNode*> rcCommitSupers[64];
static std::vector<SuperNode*> rcResetSupers;
static std::unordered_map<Node*, std::vector<std::pair<int, int>>> rcForeign;
static std::unordered_map<Node*, std::vector<Node*>> rcReaders;
struct RcMemWakeGroup {
  int part = 0;
  std::set<int> ids;
};

struct RcMemFlushWake {
  Node* memory = nullptr;
  int writerPart = 0;
  std::vector<RcMemWakeGroup> groups;
};

// Snapshot readers are activated when the log is written into the array,
// not when a writer merely runs. Local and remote readers share this list.
static std::vector<RcMemFlushWake> rcMemFlushWakes;
static bool rcMemFlushReady = false;
static int rcSuperPart(SuperNode* super);
static void repCutActivate(Node* node);
static void buildRcMemFlushWakes();

static int rcThreads() {
  const int threads = globalConfig.RepCutThreads;
  return threads > 0 ? threads : 1;
}

struct RcFlags {
  std::string savedFlags;
  int savedPart;
  RcFlags(std::string flags, int part)
      : savedFlags(gActiveFlags), savedPart(rcCurPart) {
    gActiveFlags = std::move(flags);
    rcCurPart = part;
  }
  ~RcFlags() {
    gActiveFlags = std::move(savedFlags);
    rcCurPart = savedPart;
  }
  RcFlags(const RcFlags&) = delete;
  RcFlags& operator=(const RcFlags&) = delete;
};

static bool rcTrackedReader(Node* writer, Node* port) {
  if (port == nullptr || port == writer || port->status != VALID_NODE) return false;
  if (writer->type == NODE_WRITER) return port->type == NODE_READER;
  if (writer->type != NODE_READWRITER) return false;
  return port->type == NODE_READER || port->type == NODE_READWRITER;
}

static std::map<Node*, std::pair<int, int>> super2ResetId;  // uint & async reset

extern int maxConcatNum;
bool nameExist(std::string str);
static int resetFuncNum = 0;

static bool isAlwaysActive(int cppId) {
  if (globalConfig.RepCutMode && rcCurPart >= 0) return rcAlways[rcCurPart].count(cppId) != 0;
  return alwaysActive.find(cppId) != alwaysActive.end();
}

std::pair<int, int> cppId2flagIdx(int cppId) {
  int id = cppId / ACTIVE_WIDTH;
  int bit = cppId % ACTIVE_WIDTH;
  return std::make_pair(id, bit);
}

std::pair<int, uint64_t>setIdxMask(int cppId) {
  int id, bit;
  std::tie(id, bit) = cppId2flagIdx(cppId);
  uint64_t mask = (uint64_t)1 << bit;
  return std::make_pair(id, mask);
}

std::pair<int, uint64_t>clearIdxMask(int cppId) {
  int id, bit;
  std::tie(id, bit) = cppId2flagIdx(cppId);
  uint64_t mask = (uint64_t)1 << bit;
  if (ACTIVE_WIDTH == 64) mask = ~mask;
  else mask = (~mask) & (((uint64_t)1 << ACTIVE_WIDTH) - 1);
  return std::make_pair(id, mask);
}

ActiveType activeSet2bitMap(std::set<int>& activeId, std::map<uint64_t, ActiveType>& bitMapInfo, int curId) {
  uint64_t ret = 0;
  std::string comment = "";
  int uniqueIdx = 0;
  for (int id : activeId) {
    if (isAlwaysActive(id)) continue;
    int bitMapId;
    uint64_t bitMapMask;
    std::tie(bitMapId, bitMapMask) = setIdxMask(id);
    int num = 64 / ACTIVE_WIDTH;
    if (curId >= 0 && id > curId && bitMapId == curId / ACTIVE_WIDTH) {
      if (ret == 0) uniqueIdx = id % ACTIVE_WIDTH;
      else uniqueIdx = -1;
      ret |= bitMapMask;
      comment += std::to_string(id) + " ";
    } else {
      int beg = bitMapId - bitMapId % num;
      int end = beg + num;
      int findType = 0;
      uint64_t newMask = bitMapMask << ((bitMapId - beg) * ACTIVE_WIDTH);
      std::string newComment = std::to_string(id);
      if (bitMapInfo.find(bitMapId) != bitMapInfo.end()) {
        ACTIVE_MASK(bitMapInfo[bitMapId]) |= bitMapMask;
        ACTIVE_COMMENT(bitMapInfo[bitMapId]) += " " + std::to_string(id);
        ACTIVE_UNIQUE(bitMapInfo[bitMapId]) = -1;
        findType = 1; // no nothing
      } else {
        for (int newId = beg; newId < end; newId ++) {
          if (bitMapInfo.find(newId) != bitMapInfo.end()) {
            newMask |= ACTIVE_MASK(bitMapInfo[newId]) << ((newId - beg) * ACTIVE_WIDTH);
            newComment += " " + ACTIVE_COMMENT(bitMapInfo[newId]);
            findType = 2;  // find to merge
            bitMapInfo.erase(newId);
          }
        }
      }
      if (findType == 0) bitMapInfo[bitMapId] = std::make_tuple(bitMapMask, std::to_string(id), id % ACTIVE_WIDTH);
      else if (findType == 2) bitMapInfo[beg] = std::make_tuple(newMask, newComment, -1);
    }
  }
  return std::make_tuple(ret, comment, uniqueIdx);
}

std::string updateActiveStr(int idx, uint64_t mask) {
  if (mask <= MAX_U8) return format("%s[%d] |= 0x%lx;", gActiveFlags.c_str(), idx, mask);
  if (mask <= MAX_U16) return format("*(uint16_t*)&%s[%d] |= 0x%lx;", gActiveFlags.c_str(), idx, mask);
  if (mask <= MAX_U32) return format("*(uint32_t*)&%s[%d] |= 0x%lx;", gActiveFlags.c_str(), idx, mask);
  return format("*(uint64_t*)&%s[%d] |= 0x%lx;", gActiveFlags.c_str(), idx, mask);
}

std::string updateActiveStr(int idx, uint64_t mask, std::string& cond, int uniqueId) {
  auto activeFlags = gActiveFlags + "[" + std::to_string(idx) + "]";

  if (mask <= MAX_U8) {
    if (uniqueId >= 0) return format("%s |= %s%s;", activeFlags.c_str(), cond.c_str(), shiftBits(uniqueId, ShiftDir::Left).c_str());
    else return format("%s |= -(uint8_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  }
  if (mask <= MAX_U16)
    return format("*(uint16_t*)&%s |= -(uint16_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  if (mask <= MAX_U32)
    return format("*(uint32_t*)&%s |= -(uint32_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
  return format("*(uint64_t*)&%s |= -(uint64_t)%s & 0x%lx;", activeFlags.c_str(), cond.c_str(), mask, activeFlags.c_str());
}

static void inline includeLib(FILE* fp, std::string lib, bool isStd) {
  std::string format = isStd ? "#include <%s>\n" : "#include \"%s\"\n";
  fprintf(fp, format.c_str(), lib.c_str());
}

static void inline newLine(FILE* fp) {
  fprintf(fp, "\n");
}

std::string strReplace(std::string s, std::string oldStr, std::string newStr) {
  size_t pos;
  while ((pos = s.find(oldStr)) != std::string::npos) {
    s.replace(pos, oldStr.length(), newStr);
  }
  return s;
}

FILE* graph::genHeaderStart() {
  FILE* header = std::fopen((globalConfig.OutputDir + "/" + name + ".h").c_str(), "w");

  fprintf(header, "#ifndef %s_H\n#define %s_H\n", name.c_str(), name.c_str());
  /* include all libs */
  includeLib(header, "iostream", true);
  includeLib(header, "vector", true);
  includeLib(header, "assert.h", true);
  includeLib(header, "stdlib.h", true);
  includeLib(header, "cstdint", true);
  includeLib(header, "ctime", true);
  includeLib(header, "iomanip", true);
  includeLib(header, "cstring", true);
  includeLib(header, "map", true);
  includeLib(header, "cstdarg", true);
  if (globalConfig.RepCutMode) {
    includeLib(header, "pthread.h", true);
    includeLib(header, "sched.h", true);
    includeLib(header, "atomic", true);
    includeLib(header, "cstddef", true);
  }
  newLine(header);

  fprintf(header, "\n// User configuration\n");
  fprintf(header, "//#define ENABLE_LOG\n");
  fprintf(header, "//#define RANDOMIZE_INIT\n");

  fprintf(header, "\n#define gAssert(cond, ...) do {"
                     "if (!(cond)) {"
                       "fprintf(stderr, \"\\33[1;31m\");"
                       "fprintf(stderr, __VA_ARGS__);"
                       "fprintf(stderr, \"\\33[0m\\n\");"
                       "assert(cond);"
                     "}"
                   "} while (0)\n");
  fprintf(header, "#define gdiv(a, b) ((b) == 0 ? 0 : (a) / (b))\n");

  fprintf(header, "#ifndef __BITINT_MAXWIDTH__\n");
  fprintf(header, "#error  BITINT support is required\n");
  fprintf(header, "#endif\n\n");

  /* There is some bugs with _BitInt in clang 18 */
  fprintf(header, "#ifdef __clang__\n");
  fprintf(header, "#if __clang_major__ < 19\n");
  fprintf(header, "#error  Please compile with clang 19 or above\n");
  fprintf(header, "#endif\n");
  fprintf(header, "#endif // __clang__ \n\n");

  fprintf(header, "#define likely(x) __builtin_expect(!!(x), 1)\n");
  fprintf(header, "#define unlikely(x) __builtin_expect(!!(x), 0)\n");
  fprintf(header, "void gprintf(const char *fmt, ...);\n\n");

  for (int num = 2; num <= maxConcatNum; num ++) {
    std::string param;
    for (int i = num; i > 0; i --) param += format(i == num ? "_%d" : ", _%d", i);
    std::string value;
    std::string type = widthUType(num * 64);
    for (int i = num; i > 1; i --) {
      value += format(i == num ? "((%s)_%d << %d) " : "| ((%s)_%d << %d)", type.c_str(), i, (i-1) * 64);
    }
    value += format("| ((%s)_1)", type.c_str());
    fprintf(header, "#define UINT_CONCAT%d(%s) (%s)\n", num, param.c_str(), value.c_str());
  }
  for (std::string str : extDecl) fprintf(header, "%s\n", str.c_str());
  newLine(header);
  return header;
}

void graph::emitActiveMask(std::set<int>& activeId, int indent) {
  std::map<uint64_t, ActiveType> bitMapInfo;
  activeSet2bitMap(activeId, bitMapInfo, -1);
  for (auto& iter : bitMapInfo) {
    emitBodyLock(indent, "%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
  }
}

void graph::genInterfaceInput(Node* input) {
  /* set by string */
  emitFuncDecl(0, "void S%s::set_%s(%s val) {\n", name.c_str(), input->name.c_str(), widthUType(input->width).c_str());
  emitBodyLock(1, "if (%s != val) { \n", input->name.c_str());
  emitBodyLock(2, "%s = val;\n", input->name.c_str());
  /* update nodes in the same superNode */
  if (!globalConfig.RepCutMode) {
    std::set<int> allNext;
    for (Node* next : input->next) {
      if (next->super->cppId >= 0) allNext.insert(next->super->cppId);
    }
    emitActiveMask(allNext, 2);
  } else {
    std::map<int, std::set<int>> groups;
    auto note = [&](Node* next) {
      if (next->super->cppId >= 0) groups[rcSuperPart(next->super)].insert(next->super->cppId);
    };
    for (Node* next : input->next) note(next);
    for (Node* next : repCutBoundaryNext(input)) note(next);
    for (auto& group : groups) {
      RcFlags guard("activeFlags" + std::to_string(group.first), group.first);
      emitActiveMask(group.second, 2);
    }
  }
  emitBodyLock(1, "}\n");
  emitBodyLock(0, "}\n");
}

void graph::genInterfaceOutput(Node* output) {
  emitFuncDecl(0, "%s S%s::get_%s() {\n"
               "  return %s;\n"
               "}\n",
               widthUType(output->width).c_str(), name.c_str(),
               output->name.c_str(), output->status == CONSTANT_NODE ? output->computeInfo->valStr.c_str() : output->name.c_str());
}

void graph::genHeaderEnd(FILE* fp) {
  fprintf(fp, "};\n");
  fprintf(fp, "#endif\n");
}

#if defined(DIFFTEST_PER_SIG) && defined(GSIM_DIFF)
void graph::genDiffSig(FILE* fp, Node* node) {
  std::set<std::string> allNames;
  std::string diffNodeName = node->name;
  std::string originName = node->name;
  if (node->type == NODE_MEMORY){

  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      allNames.insert(diffNodeName + suffix[i]);
    }
  } else {
    allNames.insert(diffNodeName);
  }
  for (auto iter : allNames)
    fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.c_str(), iter.c_str());
}
#endif

#if defined(DIFFTEST_PER_SIG) && defined(VERILATOR_DIFF)
void graph::genDiffSig(FILE* fp, Node* node) {
  std::string verilatorName = name + "__DOT__" + node->name;
  size_t pos;
  while ((pos = verilatorName.find("$$")) != std::string::npos) {
    verilatorName.replace(pos, 2, "_");
  }
  while ((pos = verilatorName.find("$")) != std::string::npos) {
    verilatorName.replace(pos, 1, "__DOT__");
  }
  std::map<std::string, std::string> allNames;
  std::string diffNodeName = node->name;
  std::string originName = node->name;
  if (node->type == NODE_MEMORY){

  } else if (node->isArray()) {
    int num = node->arrayEntryNum();
    std::vector<std::string> suffix(num);
    std::vector<std::string> verilatorSuffix(num);
    int pairNum = 1;
    for (size_t i = 0; i < node->dimension.size(); i ++) {
      int suffixIdx = 0;
      for (int l = 0; l < pairNum; l ++) {
        for (int j = 0; j < node->dimension[i]; j ++) {
          int suffixNum = num / node->dimension[i];
          for (int k = 0; k < suffixNum; k ++) {
            verilatorSuffix[suffixIdx] += "_" + std::to_string(j);
            suffix[suffixIdx] += "[" + std::to_string(j) + "]";
            suffixIdx ++;
          }
        }
      }
      num = num / node->dimension[i];
      pairNum *= node->dimension[i];
    }
    for (size_t i = 0; i < suffix.size(); i ++) {
      if (!nameExist(originName + verilatorSuffix[i])) {
        allNames[diffNodeName + suffix[i]] = verilatorName + verilatorSuffix[i];
      }
    }
  } else {
    allNames[diffNodeName] = verilatorName;
  }
  for (auto iter : allNames)
    fprintf(sigFile, "%d %d %s %s\n", node->sign, node->width, iter.first.c_str(), iter.second.c_str());
}
#endif

void graph::genNodeDef(FILE* fp, Node* node) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || (node->status != VALID_NODE)) return;
  if (node->type == NODE_REG_DST && !node->regSplit) return;
  if (node->type == NODE_WRITER) return;
  if (node->isLocal()) return;
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  genDiffSig(fp, node);
#endif
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  auto emitDecl = [&](const std::string& declName) {
    fprintf(fp, "%s %s", widthUType(node->width).c_str(), declName.c_str());
    if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
    for (int dim : node->dimension) fprintf(fp, "[%d]", upperPower2(dim));
    fprintf(fp, "; // width = %d, lineno = %d\n", node->width, node->lineno);
  };
  emitDecl(node->name);
  if (node->type == NODE_MEMORY && repCutSnapshotMemory(node)) {
    const std::string elem = widthUType(node->width);
    fprintf(fp, "struct RcStore$%s { uint64_t elem; %s data; };\n", node->name.c_str(), elem.c_str());
    for (int part : repCutSnapshotWriterParts(node)) {
      fprintf(fp, "alignas(64) std::vector<RcStore$%s> rcLog$%s$%d;\n",
              node->name.c_str(), node->name.c_str(), part);
      fprintf(fp, "void rcWrite$%s$%d(uint64_t elem, %s data);\n",
              node->name.c_str(), part, elem.c_str());
      fprintf(fp, "%s rcRead$%s$%d(uint64_t elem);\n",
              elem.c_str(), node->name.c_str(), part);
    }
  }
  int w = node->width;
  bool needInitMask = (node->type != NODE_MEMORY && node->type != NODE_WRITER) &&
    (((w < 64) && (w != 8 && w != 16 && w != 32 && w != 64)) || ((w > 64) && (w % 32 != 0)));
  if (needInitMask) {
    if (node->dimension.empty()) {
      emitBodyLock(1, "%s &= %s;\n", node->name.c_str(), bitMask(w).c_str());
    } else {
      int indent = 1;
      int dims = node->dimension.size();
      for (int i = 0; i < dims; i ++) {
        emitBodyLock(indent ++, "for (int i%d = 0; i%d < %d; i%d ++) {\n", i, i, node->dimension[i], i);
      }
      emitBodyLock(indent, "%s", node->name.c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock(0, "[i%d]", i); }
      emitBodyLock(0, "&= %s;\n", bitMask(w).c_str());
      for (int i = 0; i < dims; i ++) { emitBodyLock(-- indent, "}\n"); }
    }
  }

  /* save reset registers */
  if (node->isReset() && node->type == NODE_REG_SRC) {
    Assert(!node->isArray() && node->width <= BASIC_WIDTH, "%s is treated as reset (isArray: %d width: %d)", node->name.c_str(), node->isArray(), node->width);
    fprintf(fp, "%s %s;\n", widthUType(node->width).c_str(), RESET_NAME(node).c_str());
    if (needInitMask) {
      emitBodyLock(1, "%s = %s & %s;\n", RESET_NAME(node).c_str(), RESET_NAME(node).c_str(), bitMask(w).c_str());
    }
  }
}

struct RcActGroup {
  std::string storage;
  int part = 0;
  int pendSrc = -1;
  std::set<int> ids;
};

static void addRcGroup(std::vector<RcActGroup>& groups, std::string storage, int part, int pendSrc, const std::set<int>& ids) {
  if (ids.empty()) return;
  for (RcActGroup& group : groups) {
    if (group.storage == storage) {
      group.ids.insert(ids.begin(), ids.end());
      return;
    }
  }
  groups.push_back(RcActGroup{std::move(storage), part, pendSrc, ids});
}

// Commit workers write another partition's active bits into a private buffer.
// The bits are ORed after every commit finishes; OR does not depend on order.
// Each source records destinations in its own cache line. Consumers only read
// that mask after the commit barrier, so commit does not bounce a shared flag.
static bool rcUsesRegChange(Node* node) {
  return rcRegChangeOf.find(node) != rcRegChangeOf.end();
}

static bool rcRegChangedAt(Node* node, int& owner, int& word, int& bit) {
  auto found = rcRegChangeOf.find(node);
  if (found == rcRegChangeOf.end()) return false;
  const RcRegChange& change = rcRegChanges[found->second];
  owner = change.owner;
  word = change.index / 64;
  bit = change.index % 64;
  return true;
}

static void addRepCutGroups(Node* node, const std::set<int>& local, std::vector<RcActGroup>& groups) {
  addRcGroup(groups, gActiveFlags, rcCurPart, -1, local);
  // Foreign register readers are woken from the change bit after the barrier.
  if (rcUsesRegChange(node)) return;
  auto foreign = rcForeign.find(node);
  if (foreign == rcForeign.end()) return;
  std::map<int, std::set<int>> byPart;
  for (const auto& edge : foreign->second) byPart[edge.first].insert(edge.second);
  for (auto& item : byPart) {
    const int dest = item.first;
    if (rcDeferForeign && dest != rcEmitPart) {
      addRcGroup(groups, format("rcCommitPend%d_%d", rcEmitPart, dest), dest, rcEmitPart, item.second);
    } else {
      if (rcInWorker && dest != rcEmitPart) {
        Assert(false, "RepCut worker %d node %s activates partition %d", rcEmitPart, node->name.c_str(), dest);
      }
      addRcGroup(groups, "activeFlags" + std::to_string(dest), dest, -1, item.second);
    }
  }
}


void graph::activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName, int indent,
                         std::string newName) {
  if (globalConfig.RepCutMode) {
    const bool regChange = rcUsesRegChange(node);
    std::vector<RcActGroup> groups;
    addRepCutGroups(node, nextNodeId, groups);
    if (groups.empty() && !regChange) return;
    std::string changedName = "cond_" + node->name;
    bool branchless = !inStep && !regChange && groups.size() == 1 && groups[0].pendSrc < 0;
    std::map<uint64_t, ActiveType> conditionalMasks;
    if (branchless) {
      activeSet2bitMap(groups[0].ids, conditionalMasks, -1);
      branchless = !conditionalMasks.empty() && conditionalMasks.size() < 4;
    }
    if (branchless) {
      emitBodyLock(indent, "bool %s = %s != %s;\n", changedName.c_str(),
                   (newName.empty() ? node->name : newName).c_str(), oldName.c_str());
      RcFlags guard(groups[0].storage, groups[0].part);
      for (const auto& mask : conditionalMasks) {
        emitBodyLock(indent, "%s // %s\n",
                     updateActiveStr(static_cast<int>(mask.first), ACTIVE_MASK(mask.second),
                                     changedName, ACTIVE_UNIQUE(mask.second)).c_str(),
                     ACTIVE_COMMENT(mask.second).c_str());
      }
      return;
    }
    emitBodyLock(indent++, "if (%s != %s) {\n", (newName.empty() ? node->name : newName).c_str(), oldName.c_str());
    for (RcActGroup& group : groups) {
      RcFlags guard(group.storage, group.part);
      // IDs are local to each partition. Never use the source cppId/flagName
      // for a destination partition's same-byte shortcut.
      emitActiveMask(group.ids, indent);
    }
    if (regChange) {
      int owner = 0, word = 0, bit = 0;
      if (rcRegChangedAt(node, owner, word, bit)) {
        emitBodyLock(indent, "rcRegChanged%d[%d] |= 1ull << %d;\n", owner, word, bit);
      }
    }
    emitBodyLock(--indent, "}\n");
    return;
  }
  std::string nodeName = node->name;
  auto condName = std::string("cond_") + nodeName;
  bool opt{false};

  std::map<uint64_t, ActiveType> bitMapInfo;
  ActiveType curMask;
  if (node->isAsyncReset()) {
    emitBodyLock(indent ++, "if (%s || (%s != %s)) {\n", oldName.c_str(), nodeName.c_str(), oldName.c_str());
  } else {
    curMask = activeSet2bitMap(nextNodeId, bitMapInfo, node->super->cppId);
    opt = ((ACTIVE_MASK(curMask) != 0) + bitMapInfo.size()) <= 3;
    if (opt) {
      if (node->width == 1) emitBodyLock(indent, "bool %s = %s ^ %s;\n", condName.c_str(), nodeName.c_str(), oldName.c_str());
      else emitBodyLock(indent, "bool %s = %s != %s;\n", condName.c_str(), nodeName.c_str(), oldName.c_str());
    }
    else {
      emitBodyLock(indent ++, "if (%s != %s) {\n", nodeName.c_str(), oldName.c_str());
    }
  }
  if (inStep) {
    if (node->isReset() && node->type == NODE_REG_SRC) emitBodyLock(indent, "%s = %s;\n", RESET_NAME(node).c_str(), newName(node).c_str());
    emitBodyLock(indent, "%s = %s;\n", node->name.c_str(), newName(node).c_str());
  }
  if (node->isAsyncReset()) {
    Assert(!opt, "invalid opt");
    emitBodyLock(indent, "activateAll();\n");
    emitBodyLock(indent, "%s = -1;\n", flagName.c_str());
  } else {
    if (ACTIVE_MASK(curMask) != 0) {
      if (opt) emitBodyLock(indent, "%s |= -(uint%d_t)%s & 0x%lx; // %s\n", flagName.c_str(), ACTIVE_WIDTH, condName.c_str() ,ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
      else emitBodyLock(indent, "%s |= 0x%lx; // %s\n", flagName.c_str(), ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
    }
    for (auto iter : bitMapInfo) {
      auto str = opt ? updateActiveStr(iter.first, ACTIVE_MASK(iter.second), condName, ACTIVE_UNIQUE(iter.second)) : updateActiveStr(iter.first, ACTIVE_MASK(iter.second));
      emitBodyLock(indent, "%s // %s\n", str.c_str(), ACTIVE_COMMENT(iter.second).c_str());
    }
  #ifdef PERF
    #if ENABLE_ACTIVATOR
    for (int id : nextNodeId) {
      emitBodyLock(indent, "if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\nactivator[%d][%d] ++;\n",
                  id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
    }
    #endif
    if (inStep && node->type != NODE_EXT_OUT) emitBodyLock(indent, "isActivateValid = true;\n");
  #endif
  }
  if (!opt) emitBodyLock(-- indent, "}\n");
}

void graph::activateUncondNext(Node* node, std::set<int>& activateId, bool inStep, std::string flagName, int indent) {
  if (globalConfig.RepCutMode) {
    const bool regChange = rcUsesRegChange(node);
    std::vector<RcActGroup> groups;
    addRepCutGroups(node, activateId, groups);
    for (RcActGroup& group : groups) {
      RcFlags guard(group.storage, group.part);
      emitActiveMask(group.ids, indent);
    }
    if (regChange) {
      int owner = 0, word = 0, bit = 0;
      if (rcRegChangedAt(node, owner, word, bit)) {
        emitBodyLock(indent, "rcRegChanged%d[%d] |= 1ull << %d;\n", owner, word, bit);
      }
    }
    return;
  }
  std::map<uint64_t, ActiveType> bitMapInfo;
  auto curMask = activeSet2bitMap(activateId, bitMapInfo, node->super->cppId);
  if (ACTIVE_MASK(curMask) != 0) emitBodyLock(indent, "%s |= 0x%lx; // %s\n", flagName.c_str(), ACTIVE_MASK(curMask), ACTIVE_COMMENT(curMask).c_str());
  for (auto iter : bitMapInfo) {
    emitBodyLock(indent, "%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
  }
#ifdef PERF
  #if ENABLE_ACTIVATOR
  for (int id : activateId) {
    emitBodyLock(indent, "if (activator[%d].find(%d) == activator[%d].end()) activator[%d][%d] = 0;\n activator[%d][%d] ++;\n",
                id, node->super->cppId, id, id, node->super->cppId, id, node->super->cppId);
  }
  #endif
  if (inStep) emitBodyLock(indent, "isActivateValid = true;\n");
#endif
}

int graph::genNodeStepStart(SuperNode* node, uint64_t mask, int idx, std::string flagName, int indent) {
  nodeNum ++;
  if (!isAlwaysActive(node->cppId)) {
    emitBodyLock(indent ++, rcEmitEval ? "if(%s & 0x%lx) { // id=%d\n"
                                      : "if(unlikely(%s & 0x%lx)) { // id=%d\n",
                 flagName.c_str(), mask, idx);
  }
  int id;
  uint64_t newMask;
  std::tie(id, newMask) = clearIdxMask(node->cppId);
#ifdef PERF
  emitBodyLock(indent, "activeTimes[%d] ++;\n", node->cppId);
  if (node->superType != SUPER_EXTMOD) {
    emitBodyLock(indent, "bool isActivateValid = false;\n");
  }
#endif
  return indent;
}

void graph::nodeDisplay(Node* member, int indent) {
#define emit_display(varname, width, indent) \
  do { \
    int n = ROUNDUP(width, 64) / 64; \
    std::string s = "printf(\"%%lx"; \
    for (int i = n - 2; i >= 0; i --) { \
      s += "|%%lx"; \
    } \
    s += "\", "; \
    for (n --; n > 0; n --) { \
      s += format("(uint64_t)(%s >> %d)", varname, n * 64); \
      s += ", "; \
    } \
    s += format("(uint64_t)%s",varname);\
    s += ");"; \
    emitBodyLock(indent, s.c_str()); \
  } while (0)

  if (member->status != VALID_NODE) return;
  if (member->type == NODE_WRITER) return;
  emitBodyLock(indent, "printf(\"%%ld %d %s: \", cycles);\n", member->super->cppId, member->name.c_str());
  if (member->dimension.size() != 0) {
    std::string idxStr;
    for (size_t i = 0; i < member->dimension.size(); i ++) {
      emitBodyLock(indent ++, "for(int i%ld = 0; i%ld < %d; i%ld ++) {\n", i, i, member->dimension[i], i);
      idxStr += "[i" + std::to_string(i) + "]";
    }
    std::string nameIdx = member->name + idxStr;
    emit_display(nameIdx.c_str(), member->width, indent);
    emitBodyLock(indent, "printf(\" \");\n");
    for (size_t i = 0; i < member->dimension.size(); i ++) {
      emitBodyLock(-- indent, "}\n");
    }
  } else {
    if (member->anyNextActive() || member->type != NODE_SPECIAL) {
      emit_display(member->name.c_str(), member->width, indent);
    }
  }
  emitBodyLock(indent, "printf(\"\\n\");\n");
}

int graph::genNodeStepEnd(SuperNode* node, int indent) {
#ifdef PERF
  if (node->superType != SUPER_EXTMOD) {
    emitBodyLock(indent, "validActive[%d] += isActivateValid;\n", node->cppId);
  }
#endif

  if(!isAlwaysActive(node->cppId)) {
    emitBodyLock(-- indent, "}\n");
  }
  return indent;
}

int graph::translateInst(InstInfo inst, int indent, std::string flagName) {
  switch (inst.infoType) {
    case SUPER_INFO_IF:
      emitBodyLock(indent ++, "%s\n", inst.inst.c_str());
      break;
    case SUPER_INFO_ELSE:
      emitBodyLock(indent - 1,  "%s\n", inst.inst.c_str());
      break;
    case SUPER_INFO_DEDENT:
      emitBodyLock(--indent, "%s\n", inst.inst.c_str());
      break;
    case SUPER_INFO_STR:
      emitBodyLock(indent, "%s\n", inst.inst.c_str());
      break;
    case SUPER_INFO_ASSIGN_BEG:
      if (inst.node->isLocal() || inst.node->isArray() || inst.node->type == NODE_WRITER) break;
      emitBodyLock(indent, "%s %s = %s;\n", widthUType(inst.node->width).c_str(), oldName(inst.node).c_str(), inst.node->name.c_str());
      break;
    case SUPER_INFO_ASSIGN_END:
      if (inst.node->isLocal() || !inst.node->needActivate()) break;
      if (inst.node->isArray() || inst.node->type == NODE_WRITER) activateUncondNext(inst.node, inst.node->nextActiveId, false, flagName, indent);
      else activateNext(inst.node, inst.node->nextActiveId, oldName(inst.node), false, flagName, indent);
      break;
    default:
      break;
  }
  return indent;
}

static std::vector<Node*> rcBulkCommitNodes(SuperNode* super);

void graph::genSuperEval(SuperNode* super, std::string flagName, int indent) { // current indent = 2
  if (super->superType == SUPER_EXTMOD) { // TODO: normalize
    auto emitExtAsyncReset = [&](Node* extOut) {
      if (globalConfig.RepCutMode || !extOut->isAsyncReset()) return;
      auto resetId = super2ResetId.find(extOut);
      Assert(resetId != super2ResetId.end() && resetId->second.second >= 0, "missing async reset id for %s", extOut->name.c_str());
      emitBodyLock(indent, "subReset%d();\n", resetId->second.second);
    };
    for (size_t i = 1; i < super->member.size(); i ++) {
      emitExtAsyncReset(super->member[i]);
    }
    /* save old EXT_OUT*/
    for (size_t i = 1; i < super->member.size(); i ++) {
      if (!super->member[i]->needActivate()) continue;
      Node* extOut = super->member[i];
      emitBodyLock(indent, "%s %s = %s;\n", widthUType(extOut->width).c_str(), oldName(extOut).c_str(), extOut->name.c_str());
    }
    for (InstInfo inst : super->insts) {
      indent = translateInst(inst, indent, flagName);
    }
    for (size_t i = 1; i < super->member.size(); i ++) {
      emitExtAsyncReset(super->member[i]);
    }
    for (size_t i = 1; i < super->member.size(); i ++) {
      if (!super->member[i]->needActivate()) continue;
      if (super->member[i]->isArray()) activateUncondNext(super->member[i], super->member[i]->nextActiveId, false, flagName, indent);
      else activateNext(super->member[i], super->member[i]->nextActiveId, oldName(super->member[i]), false, flagName, indent);
    }
  } else {
    if (!globalConfig.RepCutMode && super->superType == SUPER_ASYNC_RESET) {
      emitBodyLock(indent, "subReset%d();\n", super2ResetId[super->resetNode].second);
    }
    /* local nodes definition */
    for (Node* n : super->member) {
      if (n->isLocal()) {
        emitBodyLock(indent, "%s %s;\n", widthUType(n->width).c_str(), n->name.c_str());
      }
    }
    const std::vector<Node*> bulk = rcBulkCommitNodes(super);
    if (bulk.empty()) {
      for (InstInfo inst : super->insts) {
        indent = translateInst(inst, indent, flagName);
      }
      if (!super->stmtTree->sinkDebugInsts.empty()) {
        emitBodyLock(indent, "#ifdef ENABLE_LOG\n");
        for (const InstInfo& inst : super->stmtTree->sinkDebugInsts) {
          if (inst.infoType == SUPER_INFO_STR) emitBodyLock(indent, "%s\n", inst.inst.c_str());
        }
        emitBodyLock(indent, "#endif\n");
      }
    } else {
      const size_t count = bulk.size();
      const std::string& first = bulk.front()->name;
      const std::string& last = bulk.back()->name;
      const std::string& firstNext = bulk.front()->getDst()->name;
      const std::string& lastNext = bulk.back()->getDst()->name;
      emitBodyLock(indent, "static_assert(offsetof(S%s, %s) - offsetof(S%s, %s) == %zu * sizeof(uint64_t));\n",
                   name.c_str(), last.c_str(), name.c_str(), first.c_str(), count - 1);
      emitBodyLock(indent, "static_assert(offsetof(S%s, %s) - offsetof(S%s, %s) == %zu * sizeof(uint64_t));\n",
                   name.c_str(), lastNext.c_str(), name.c_str(), firstNext.c_str(), count - 1);
      std::set<int> destinations = bulk.front()->nextActiveId;
      bool sameDestination = !destinations.empty();
      for (Node* node : bulk) {
        if (node->nextActiveId != destinations || rcUsesRegChange(node) ||
            !rcForeign[node].empty()) sameDestination = false;
      }
      if (sameDestination) {
        emitBodyLock(indent++, "if (memcmp((const char*)this + offsetof(S%s, %s), (const char*)this + offsetof(S%s, %s), %zu * sizeof(uint64_t)) != 0) {\n",
                     name.c_str(), first.c_str(), name.c_str(), firstNext.c_str(), count);
        emitActiveMask(destinations, indent);
        emitBodyLock(--indent, "}\n");
      } else {
        for (size_t i = 0; i < count; ++i) {
          activateNext(bulk[i], bulk[i]->nextActiveId, bulk[i]->name, false, flagName, indent,
                       bulk[i]->getDst()->name);
        }
      }
      emitBodyLock(indent, "memcpy((char*)this + offsetof(S%s, %s), (const char*)this + offsetof(S%s, %s), %zu * sizeof(uint64_t));\n",
                   name.c_str(), first.c_str(), name.c_str(), firstNext.c_str(), count);
    }
    if (!globalConfig.RepCutMode && super->superType == SUPER_ASYNC_RESET) emitBodyLock(indent, "subReset%d();\n", super2ResetId[super->resetNode].second);
    emitBodyLock(indent, "#ifdef ENABLE_LOG\n");
    emitBodyLock(indent ++, "if (cycles >= LOG_START && cycles <= LOG_END) {\n");
    for (Node* n : super->member) nodeDisplay(n, indent);
    emitBodyLock(-- indent, "}\n");
    emitBodyLock(indent, "#endif\n");
  }
}


int graph::genActivate() {
    emitFuncDecl(0, "void S%s::subStep0() {\n", name.c_str());
    int indent = 1;
    int nextSubStepIdx = 1;
    std::string nextFuncDef = format("void S%s::subStep%d()", name.c_str(), nextSubStepIdx);
    bool prevActiveWhole = false;
    for (int idx = 0; idx < superId; idx ++) {
      int id;
      uint64_t mask;
      std::tie(id, mask) = setIdxMask(idx);
      int offset = idx % ACTIVE_WIDTH;
      if (offset == 0) {
        if (prevActiveWhole) {
          emitBodyLock(--indent, "}\n");
        }
        prevActiveWhole = true;
        for (int j = 0; j < ACTIVE_WIDTH && idx + j < superId; j ++) {
          if (isAlwaysActive(idx + j)) prevActiveWhole = false;
        }
        if (prevActiveWhole) {
          bool newFile = __emitSrc(indent ++, true, false, nextFuncDef.c_str(), "if(unlikely(activeFlags[%d] != 0)) {\n", id);
          if (newFile) {
            nextFuncDef = format("void S%s::subStep%d()", name.c_str(), ++ nextSubStepIdx);
          }
          emitBodyLock(indent, "uint%d_t oldFlag = activeFlags[%d];\n", ACTIVE_WIDTH, id);
          emitBodyLock(indent, "activeFlags[%d] = 0;\n", id);
        }
      }
      SuperNode* super = cppId2Super[idx];
      std::string flagName = prevActiveWhole ? "oldFlag" : format("activeFlags[%d]", id);
      indent = genNodeStepStart(super, mask, idx, flagName, indent);
      genSuperEval(super, flagName, indent);
      indent = genNodeStepEnd(super, indent);
    }
    emitBodyLock(--indent, "}\n");
    if (prevActiveWhole) emitBodyLock(--indent, "}\n");

    return nextSubStepIdx - 1; // return the maxinum subStepIdx currently used
}

// Outline only complete scalar assignments: declarations must keep their scope,
// and array loops/control flow must remain intact. Reference captures preserve
// any enclosing loop variables. Each lambda is called immediately and cannot
// be inlined back into the giant reset function.
static bool isScalarResetAssignment(const InstInfo& inst) {
  if (inst.infoType != SUPER_INFO_STR) return false;
  const std::string& text = inst.inst;
  size_t pos = text.find_first_not_of(" \t");
  if (pos == std::string::npos) return false;
  const auto identifier = [](unsigned char c) {
    return std::isalnum(c) || c == '_' || c == '$';
  };
  if (!identifier(text[pos]) || std::isdigit(static_cast<unsigned char>(text[pos]))) return false;
  while (pos < text.size() && identifier(text[pos])) ++pos;
  pos = text.find_first_not_of(" \t", pos);
  if (pos == std::string::npos || text[pos] != '=' ||
      (pos + 1 < text.size() && text[pos + 1] == '=')) return false;
  return text.find('\n') == std::string::npos &&
         text.find(';') == text.find_last_not_of(" \t\r");
}

void graph::genResetDef(SuperNode* super, bool isUIntReset, int indent) {
  const int generatedResetId = resetFuncNum;
  emitBodyLock(indent ++, "void S%s::subReset%d(){ // %s reset\n", name.c_str(), resetFuncNum, isUIntReset ? "uint" : "async");
  if (super2ResetId.find(super->resetNode) == super2ResetId.end()) {
    super2ResetId[super->resetNode] = std::make_pair(-1, -1);
  }
  if (isUIntReset) super2ResetId[super->resetNode].first = resetFuncNum;
  else super2ResetId[super->resetNode].second = resetFuncNum;
  resetFuncNum ++;
  std::string resetName = super->resetNode->type == NODE_REG_SRC ? RESET_NAME(super->resetNode).c_str() : super->resetNode->name.c_str();
  if (globalConfig.RepCutMode && !isUIntReset) resetName = "rcResetActive" + std::to_string(generatedResetId);
  emitBodyLock(indent ++, "if(unlikely(%s)) {\n", resetName.c_str());
  std::map<int, std::set<int>> rcGroups;
  std::set<int> allNext;
  for (size_t i = 0; i < super->member.size(); i ++) {
    Node* node = super->member[i];
    if (node->type == NODE_REG_RESET) node = node->getResetSrc();
    auto note = [&](Node* next) {
      if (next->super == nullptr || next->super->cppId < 0) return;
      if (globalConfig.RepCutMode) rcGroups[rcSuperPart(next->super)].insert(next->super->cppId);
      else allNext.insert(next->super->cppId);
    };
    for (Node* next : node->next) note(next);
    if (globalConfig.RepCutMode) {
      for (Node* next : repCutBoundaryNext(node)) note(next);
      // Reset writes both current state and the next-state shadow. Even a
      // feed-forward next-state expression with unchanged inputs must run again.
      if (node->type == NODE_REG_SRC && node->getDst()) note(node->getDst());
    }
  }

  size_t rcCount = 0;
  for (const auto& group : rcGroups) rcCount += group.second.size();
  if ((globalConfig.RepCutMode ? rcCount : allNext.size()) > 100) emitBodyLock(indent, "activateAll();\n");
  else if (!globalConfig.RepCutMode) emitActiveMask(allNext, indent);
  else {
    for (auto& group : rcGroups) {
      RcFlags guard("activeFlags" + std::to_string(group.first), group.first);
      emitActiveMask(group.second, indent);
    }
  }
  emitBodyLock(-- indent, "}\n");
  bool resetGuardEmitted = false;
  for (size_t index = 0; index < super->insts.size();) {
    size_t end = index;
    while (end < super->insts.size() && isScalarResetAssignment(super->insts[end])) ++end;
    if (end - index > kResetAssignmentsPerChunk) {
      // Keep the original reset condition outside the chunks. Rechecking it in
      // each helper could change behavior if an earlier assignment changes it.
      for (; index < end;) {
        const size_t chunkEnd = std::min(end, index + kResetAssignmentsPerChunk);
        emitBodyLock(indent++, "[&]() __attribute__((noinline)) {\n");
        for (; index < chunkEnd; ++index) {
          emitBodyLock(indent, "%s\n", super->insts[index].inst.c_str());
        }
        emitBodyLock(--indent, "}();\n");
      }
      continue;
    }
    if (end > index) {
      for (; index < end; ++index) {
        emitBodyLock(indent, "%s\n", super->insts[index].inst.c_str());
      }
      continue;
    }
    const InstInfo& inst = super->insts[index++];
    switch (inst.infoType) {
      case SUPER_INFO_IF:
        if (globalConfig.RepCutMode && !isUIntReset && !resetGuardEmitted) {
          emitBodyLock(indent++, "if (%s) {\n", resetName.c_str());
        } else {
          emitBodyLock(indent++, "%s\n", inst.inst.c_str());
        }
        resetGuardEmitted = true;
        break;
      case SUPER_INFO_DEDENT:
        emitBodyLock(--indent, "%s\n", inst.inst.c_str());
        break;
      case SUPER_INFO_ELSE:
      case SUPER_INFO_STR:
        emitBodyLock(indent, "%s\n", inst.inst.c_str());
        break;
      default:
        break;
    }
  }
  emitBodyLock(-- indent, "}\n");
}

void graph::genResetActivation(SuperNode* super, bool isUIntReset, int indent, int resetId) {
  emitBodyLock(indent, "subReset%d();\n", resetId);
}

void graph::genResetAll() {
  std::vector<SuperNode*> resetSuper;
  for (SuperNode* super : allReset) {
    if (super->resetNode->status == CONSTANT_NODE) {
      Assert(mpz_sgn(super->resetNode->computeInfo->consVal) == 0, "reset %s is always true", super->resetNode->name.c_str());
      continue;
    }
    genResetDef(super, super->superType == SUPER_UINT_RESET, 0);
    resetSuper.push_back(super);
  }

  emitFuncDecl(0, "void S%s::resetAll(){\n", name.c_str());
  for (size_t i = 0; i < resetSuper.size(); i ++) {
    if (resetSuper[i]->superType == SUPER_ASYNC_RESET) continue;
    genResetActivation(resetSuper[i], true, 1, i);
  }
  emitBodyLock(0, "}\n");
}

void graph::genStep(int subStepIdxMax) {
  emitFuncDecl(0, "void S%s::step() {\n", name.c_str());
  emitBodyLock(1, "resetAll();\n");
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->isReset() && member->type == NODE_REG_SRC) {
        emitBodyLock(1, "%s = %s;\n", RESET_NAME(member).c_str(), member->name.c_str());
      }
    }
  }
  for (int i = 0; i <= subStepIdxMax; i ++) {
    emitBodyLock(1, "subStep%d();\n", i);
  }

  emitBodyLock(1, "cycles ++;\n");
  emitBodyLock(0, "}\n");
}

bool graph::__emitSrc(int indent, bool canNewFile, bool alreadyEndFunc, const char *nextFuncDef, const char *fmt, ...) {
  bool newFile = false;
  if (srcFp == NULL || (srcFileBytes > (globalConfig.cppMaxSizeKB * 1024) && canNewFile)) {
    if (srcFp != NULL) {
      if (!alreadyEndFunc) fprintf(srcFp, "}"); // the end of the current function
      fclose(srcFp);
    }
    srcFp = std::fopen(format("%s%d.cpp", (globalConfig.OutputDir + "/" + name).c_str(), srcFileIdx).c_str(), "w");
    srcFileIdx ++;
    assert(srcFp != NULL);
    srcFileBytes = fprintf(srcFp, "#include \"%s.h\"\n", name.c_str());
    if (nextFuncDef != NULL) {
      srcFileBytes += fprintf(srcFp, "%s {\n", nextFuncDef);
    }
    newFile = true;
  }
  for (int i = 0; i < indent; i ++) fprintf(srcFp, "  ");
  va_list args;
  va_start(args, fmt);
  int bytes = vfprintf(srcFp, fmt, args);
  assert(bytes > 0);
  va_end(args);
  srcFileBytes += bytes;
  return newFile;
}

void graph::emitPrintf() {
  emitFuncDecl(0, "void gprintf(const char *fmt, ...) {\n");
  emitBodyLock(0,
  "  FILE *fp = stderr;\n"
  "  va_list args;\n"
  "  va_start(args, fmt);\n"
  "  int fmt_idx = 0;\n"
  "  while (true) {\n"
  "    char c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case '%%': break;\n"
  "      case 0: return;\n"
  "      default: fputc(c, fp); continue;\n"
  "    }\n"
  "\n"
  "    uint64_t lval = 0;\n"
  "    int bits = va_arg(args, uint32_t);\n"
  "    if      (bits <= 32) { lval = va_arg(args, uint32_t); }\n"
  "    else if (bits <= 64) { lval = va_arg(args, uint64_t); }\n"
  "    else                 { assert(0); }\n"
  "\n"
  "    c = fmt[fmt_idx ++];\n"
  "    switch (c) {\n"
  "      case 'd': fprintf(fp, \"%%ld\", lval); break;\n"
  "      case 'c': fputc(lval & 0xff, fp); break;\n"
  "      case 'x': fprintf(fp, \"%%lx\", lval); break;\n"
  "      default: assert(0);\n"
  "    }\n"
  "  }\n"
  "}\n"
  );
}

static int rcSuperPart(SuperNode* super) {
  const int part = repCutPartOf(super);
  return part >= 0 && part < rcThreads() ? part : 0;
}

static bool rcNeedsId(SuperNode* super) {
  return !super->instsEmpty() || super->superType == SUPER_EXTMOD || super->superType == SUPER_ASYNC_RESET;
}

// Only a complete run of plain scalar register copies can be laid out and
// copied together without changing the commit node's control flow.
static std::vector<Node*> rcBulkCommitNodes(SuperNode* super) {
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  return {};
#endif
  if (!globalConfig.RepCutMode || !repCutCommitOnly(super) ||
      super->superType != SUPER_VALID || super->insts.size() % 3 != 0 ||
      super->insts.size() < 24 || super->insts.size() > 3072 ||
      !super->stmtTree->sinkDebugInsts.empty()) return {};
  const size_t count = super->insts.size() / 3;
  std::vector<Node*> nodes;
  nodes.reserve(count);
  std::set<Node*> seen;
  for (size_t i = 0; i < count; ++i) {
    const InstInfo& begin = super->insts[i];
    if (begin.infoType != SUPER_INFO_ASSIGN_BEG || begin.node == nullptr) return {};
    Node* node = begin.node;
    if (node->type != NODE_REG_SRC || node->status != VALID_NODE ||
        node->width != 64 || node->isArray() || node->isReset() ||
        node->super != super || !node->needActivate() || !seen.insert(node).second)
      return {};
    Node* next = node->getDst();
    if (next == nullptr || next->type != NODE_REG_DST || !next->regSplit ||
        next->status != VALID_NODE || next->width != 64 ||
        next->isArray() || next->isReset()) return {};
    const InstInfo& copy = super->insts[count + i];
    const InstInfo& end = super->insts[2 * count + i];
    if (copy.infoType != SUPER_INFO_STR ||
        copy.inst != node->name + " = " + next->name + ";" ||
        end.infoType != SUPER_INFO_ASSIGN_END || end.node != node) return {};
    nodes.push_back(node);
  }
  return nodes;
}

static void repCutActivate(Node* node) {
  node->nextActiveId.clear();
  node->nextNeedActivate.clear();
  const int srcPart = rcSuperPart(node->super);
  auto add = [&](SuperNode* dst) {
    if (dst == nullptr || dst->cppId < 0) return;
    const int part = rcSuperPart(dst);
    if (part == srcPart) {
      node->nextActiveId.insert(dst->cppId);
      if (rcAlways[part].count(dst->cppId) == 0) node->nextNeedActivate.insert(dst->cppId);
    } else if (rcAlways[part].count(dst->cppId) == 0) {
      rcForeign[node].push_back({part, dst->cppId});
      node->nextNeedActivate.insert(dst->cppId);
    }
  };
  for (Node* nextNode : node->next) {
    if (nextNode->super != node->super) add(nextNode->super);
    else if (node->super->findIndex(node) >= node->super->findIndex(nextNode)) add(node->super);
  }
  for (Node* nextNode : repCutBoundaryNext(node)) add(nextNode->super);
  if (node->type == NODE_REG_DST) add(node->getSrc()->super);
  if ((node->type == NODE_WRITER || node->type == NODE_READWRITER) && node->parent != nullptr) {
    auto readers = rcReaders.find(node->parent);
    if (readers != rcReaders.end()) {
      for (Node* port : readers->second) {
        if (port == nullptr || port->status != VALID_NODE || port->super == nullptr || port->super->cppId < 0) continue;
        if (port == node) {
          if (node->type == NODE_READWRITER && node->parent->extraInfo != "new") add(node->super);
          continue;
        }
        if (!rcTrackedReader(node, port)) continue;
        const int part = rcSuperPart(port->super);
        // Same-partition readers used to be flagged beside rcWrite. The array
        // does not change until flush, so they join the flush wake list instead.
        if (part == srcPart && rcThreads() > 1 && repCutSnapshotMemory(node->parent)) continue;
        if (part == srcPart) add(port->super);
        else {
          Assert(repCutSnapshotMemory(node->parent),
                 "RepCut memory reader %s crosses partitions without a snapshot",
                 port->name.c_str());
        }
      }
    }
  }
}

static void assignRepCutIds(const std::vector<SuperNode*>& sortedSuper) {
  const int threads = rcThreads();
  superId = 0;
  cppId2Super.clear();
  alwaysActive.clear();
  rcForeign.clear();
  rcReaders.clear();
  rcRegChanges.clear();
  rcRegChangeOf.clear();
  rcHasRegChanges = false;
  rcMemFlushWakes.clear();
  rcMemFlushReady = false;
  rcResetSupers.clear();
  for (int part = 0; part < 64; ++part) {
    rcEvalSupers[part].clear();
    rcCommitSupers[part].clear();
    rcAlways[part].clear();
    rcChunks[part] = 0;
    rcCommitChunks[part] = 0;
    rcFlagWords[part] = 8;
    rcRegChangeWords[part] = 0;
  }
  for (SuperNode* super : sortedSuper) {
    super->cppId = -1;
    for (Node* member : super->member) {
      if (member->parent == nullptr) continue;
      if (member->type == NODE_READER || member->type == NODE_READWRITER) {
        rcReaders[member->parent].push_back(member);
      }
    }
  }

  struct Item {
    SuperNode* super;
    int part;
    bool commit;
  };
  std::vector<Item> items;
  items.reserve(sortedSuper.size());
  for (SuperNode* super : sortedSuper) {
    if (!rcNeedsId(super)) continue;
    items.push_back({super, rcSuperPart(super), repCutCommitOnly(super)});
  }

  std::vector<int> local(threads, 0);
  for (const Item& item : items) {
    const int id = local[item.part]++;
    item.super->cppId = id;
    if (item.super->superType == SUPER_EXTMOD) rcAlways[item.part].insert(id);
    if (item.super->member.front()->rcResetSource) rcResetSupers.push_back(item.super);
    else if (item.commit) rcCommitSupers[item.part].push_back(item.super);
    else rcEvalSupers[item.part].push_back(item.super);
    ++superId;
  }
  for (int part = 0; part < threads; ++part) {
    const int words = (local[part] + ACTIVE_WIDTH - 1) / ACTIVE_WIDTH;
    const int rounded = (int)ROUNDUP(words, 8);
    rcFlagWords[part] = rounded < 8 ? 8 : rounded;
  }
}

void graph::emitRepCutFns(const std::vector<SuperNode*>& supers, const std::string& prefix, int& chunks) {
  chunks = 0;
  if (supers.empty()) return;
  auto beginChunk = [&](int chunk) {
    if (rcHeader != nullptr) fprintf(rcHeader, "void %s_%d();\n", prefix.c_str(), chunk);
    emitFuncDecl(0, "void S%s::%s_%d() {\n", name.c_str(), prefix.c_str(), chunk);
  };
  auto endChunk = [&]() {
    emitBodyLock(0, "}\n");
  };
  beginChunk(0);
  for (size_t index = 0; index < supers.size(); ++index) {
    if (index > 0 && srcFileBytes > (globalConfig.cppMaxSizeKB * 1024)) {
      endChunk();
      beginChunk(++chunks);
    }
    SuperNode* super = supers[index];
    const int part = rcSuperPart(super);
    const std::string flags = "activeFlags" + std::to_string(part);
    RcFlags guard(flags, part);
    int word = 0;
    int bit = 0;
    std::tie(word, bit) = cppId2flagIdx(super->cppId);
    const uint64_t mask = (uint64_t)1 << bit;
    const std::string flagName = format("%s[%d]", flags.c_str(), word);
    int indent = genNodeStepStart(super, mask, super->cppId, flagName, 1);
    if (!isAlwaysActive(super->cppId)) {
      uint64_t clearMask = 0;
      std::tie(word, clearMask) = clearIdxMask(super->cppId);
      emitBodyLock(indent, "%s &= 0x%lx;\n", flagName.c_str(), clearMask);
    }
    genSuperEval(super, flagName, indent);
    genNodeStepEnd(super, indent);
  }
  endChunk();
  chunks += 1;
}

void graph::genRepCutEval(int part) {
  rcEmitPart = part;
  rcInWorker = true;
  rcEmitEval = true;
  emitRepCutFns(rcEvalSupers[part], format("evalPart%d", part), rcChunks[part]);
  rcEmitEval = false;
  rcInWorker = false;
  rcEmitPart = -1;
}

static void buildRcMemFlushWakes() {
  if (rcMemFlushReady) return;
  rcMemFlushReady = true;
  rcMemFlushWakes.clear();
  const int threads = rcThreads();
  if (threads <= 1) return;
  for (Node* memory : repCutSnapshotMemories()) {
    auto readers = rcReaders.find(memory);
    if (readers == rcReaders.end()) continue;
    std::map<int, std::map<int, std::set<int>>> byWriter;
    for (Node* writer : memory->member) {
      if (writer == nullptr || writer->status != VALID_NODE) continue;
      if (writer->type != NODE_WRITER && writer->type != NODE_READWRITER) continue;
      const int src = writer->rcPart;
      if (src < 0 || src >= threads) continue;
      for (Node* port : readers->second) {
        if (port == nullptr || port->super == nullptr || port->super->cppId < 0 || !rcTrackedReader(writer, port)) continue;
        const int dst = rcSuperPart(port->super);
        if (dst < 0 || dst >= threads) continue;
        if (rcAlways[dst].count(port->super->cppId) != 0) continue;
        byWriter[src][dst].insert(port->super->cppId);
      }
    }
    for (auto& writer : byWriter) {
      if (writer.second.empty()) continue;
      RcMemFlushWake wake;
      wake.memory = memory;
      wake.writerPart = writer.first;
      wake.groups.reserve(writer.second.size());
      for (auto& readersByPart : writer.second) {
        wake.groups.push_back(RcMemWakeGroup{readersByPart.first, std::move(readersByPart.second)});
      }
      rcMemFlushWakes.push_back(std::move(wake));
    }
  }
}

void graph::genRepCutCommit() {
  const int threads = rcThreads();
  std::set<std::pair<int, int>> pendPairs;
  // rcCommitPend is indexed by active-flag byte (ACTIVE_WIDTH ids per slot).
  // Merge only dirty 8-byte windows: a zero pending word is left untouched.
  std::map<std::pair<int, int>, std::set<int>> pendSlots;
  buildRcMemFlushWakes();
  if (threads > 1) {
    for (int part = 0; part < threads; ++part) {
      for (SuperNode* super : rcCommitSupers[part]) {
        for (Node* member : super->member) {
          if (rcUsesRegChange(member)) continue;
          auto found = rcForeign.find(member);
          if (found == rcForeign.end()) continue;
          for (const auto& edge : found->second) {
            if (edge.first != part) {
              const std::pair<int, int> pair{part, edge.first};
              pendPairs.insert(pair);
              pendSlots[pair].insert(edge.second / ACTIVE_WIDTH);
            }
          }
        }
      }
    }
    for (const RcMemFlushWake& wake : rcMemFlushWakes) {
      for (const RcMemWakeGroup& group : wake.groups) {
        const std::pair<int, int> pair{wake.writerPart, group.part};
        pendPairs.insert(pair);
        for (int id : group.ids) pendSlots[pair].insert(id / ACTIVE_WIDTH);
      }
    }
  }
  rcHasCommitPend = !pendPairs.empty();
  if (rcHasRegChanges && rcHeader != nullptr) {
    for (int part = 0; part < threads; ++part) {
      if (rcRegChangeWords[part] == 0) continue;
      fprintf(rcHeader, "alignas(64) uint64_t rcRegChanged%d[%d]{};\n", part, rcRegChangeWords[part]);
    }
  }
  if (rcHasCommitPend && rcHeader != nullptr) {
    for (const auto& pend : pendPairs) {
      const int src = pend.first;
      const int dst = pend.second;
      fprintf(rcHeader, "alignas(64) uint%d_t rcCommitPend%d_%d[%d]{};\n",
              ACTIVE_WIDTH, src, dst, rcFlagWords[dst]);
    }
  }
  for (int part = 0; part < threads; ++part) {
    rcEmitPart = part;
    rcInWorker = threads > 1;
    rcDeferForeign = threads > 1;
    emitRepCutFns(rcCommitSupers[part], format("commitPart%d", part), rcCommitChunks[part]);
  }
  rcDeferForeign = false;
  rcInWorker = false;
  rcEmitPart = -1;
  if (threads == 1) {
    if (rcHeader != nullptr) fprintf(rcHeader, "void commitCycle();\n");
    emitFuncDecl(0, "void S%s::commitCycle() {\n", name.c_str());
    for (int part = 0; part < threads; ++part) {
      if (!repCutSnapshotMemories().empty()) emitBodyLock(1, "rcFlushMem(%d);\n", part);
      for (int chunk = 0; chunk < rcCommitChunks[part]; ++chunk) {
        emitBodyLock(1, "commitPart%d_%d();\n", part, chunk);
      }
    }
    emitBodyLock(0, "}\n");
  } else if (rcHeader != nullptr) {
    fprintf(rcHeader, "void rcCommitPart(int part);\n");
    emitFuncDecl(0, "void S%s::rcCommitPart(int part) {\n", name.c_str());
    // Register and memory wakes both land in rcCommitPend. Merge consumes the touched slots.
    if (!repCutSnapshotMemories().empty()) emitBodyLock(1, "rcFlushMem(part);\n");
    emitBodyLock(1, "switch (part) {\n");
    for (int part = 0; part < threads; ++part) {
      emitBodyLock(1, "case %d:\n", part);
      if (rcRegChangeWords[part] > 0) {
        emitBodyLock(2, "memset(rcRegChanged%d, 0, sizeof(rcRegChanged%d));\n", part, part);
      }
      for (int chunk = 0; chunk < rcCommitChunks[part]; ++chunk) {
        emitBodyLock(2, "commitPart%d_%d();\n", part, chunk);
      }
      emitBodyLock(2, "break;\n");
    }
    emitBodyLock(1, "default: break;\n");
    emitBodyLock(1, "}\n");
    emitBodyLock(0, "}\n");
  }
  if (rcHasCommitPend) {
    Assert(ACTIVE_WIDTH == 8, "RepCut 8-byte flag merge requires byte active flags");
    if (rcHeader != nullptr) {
      fprintf(rcHeader,
              "static inline __attribute__((always_inline)) void rcMergeWord(uint8_t* flags, uint8_t* pend) {\n"
              "  uint64_t bits;\n"
              "  memcpy(&bits, pend, sizeof(bits));\n"
              "  if (unlikely(bits != 0)) {\n"
              "    uint64_t cur;\n"
              "    memcpy(&cur, flags, sizeof(cur));\n"
              "    cur |= bits;\n"
              "    memcpy(flags, &cur, sizeof(cur));\n"
              "    memset(pend, 0, sizeof(bits));\n"
              "  }\n"
              "}\n"
              "void rcMergeCommitFlags(int part);\n");
    }
    emitFuncDecl(0, "void S%s::rcMergeCommitFlags(int part) {\n", name.c_str());
    emitBodyLock(1, "switch (part) {\n");
    for (int part = 0; part < threads; ++part) {
      emitBodyLock(1, "case %d:\n", part);
      for (const auto& pend : pendPairs) {
        if (pend.second != part) continue;
        const int src = pend.first;
        const int dst = pend.second;
        auto slots = pendSlots.find(pend);
        Assert(slots != pendSlots.end() && !slots->second.empty(),
               "RepCut pending pair %d -> %d has no touched active slots", src, dst);
        std::set<int> words;
        for (int slot : slots->second) {
          Assert(slot >= 0 && slot < rcFlagWords[dst],
                 "RepCut pending slot %d out of range for destination %d", slot, dst);
          const int word = slot & ~7;
          Assert(word + 8 <= rcFlagWords[dst],
                 "RepCut pending word %d overruns destination %d", word, dst);
          words.insert(word);
        }
        for (int word : words) {
          emitBodyLock(3, "rcMergeWord(&activeFlags%d[%d], &rcCommitPend%d_%d[%d]);\n",
                       dst, word, src, dst, word);
        }
      }
      emitBodyLock(2, "break;\n");
    }
    emitBodyLock(1, "default: break;\n");
    emitBodyLock(1, "}\n");
    emitBodyLock(0, "}\n");
  }
  if (rcHasRegChanges) {
    if (rcHeader != nullptr) fprintf(rcHeader, "void rcApplyRegChanges(int part);\n");
    std::map<int, std::map<std::pair<int, int>, std::vector<std::pair<int, std::set<int>>>>> byDest;
    for (const RcRegChange& change : rcRegChanges) {
      const int word = change.index / 64;
      for (const auto& dest : change.destIds) {
        byDest[dest.first][{change.owner, word}].push_back({change.index % 64, dest.second});
      }
    }
    emitFuncDecl(0, "void S%s::rcApplyRegChanges(int part) {\n", name.c_str());
    emitBodyLock(1, "switch (part) {\n");
    for (int part = 0; part < threads; ++part) {
      auto found = byDest.find(part);
      if (found == byDest.end()) continue;
      emitBodyLock(1, "case %d:\n", part);
      for (const auto& wordGroup : found->second) {
        const int owner = wordGroup.first.first;
        const int word = wordGroup.first.second;
        uint64_t relevant = 0;
        for (const auto& bitIds : wordGroup.second) relevant |= 1ull << bitIds.first;
        emitBodyLock(2, "{\n");
        emitBodyLock(3, "const uint64_t changed = rcRegChanged%d[%d] & 0x%lxull;\n", owner, word, relevant);
        emitBodyLock(3, "if (unlikely(changed != 0)) {\n");
        for (const auto& bitIds : wordGroup.second) {
          emitBodyLock(4, "if (changed & (1ull << %d)) {\n", bitIds.first);
          std::set<int> ids = bitIds.second;
          RcFlags guard("activeFlags" + std::to_string(part), part);
          emitActiveMask(ids, 5);
          emitBodyLock(4, "}\n");
        }
        emitBodyLock(3, "}\n");
        emitBodyLock(2, "}\n");
      }
      emitBodyLock(2, "break;\n");
    }
    emitBodyLock(1, "default: break;\n");
    emitBodyLock(1, "}\n");
    emitBodyLock(0, "}\n");
  }
}

void graph::genRepCutResetRefresh() {
  fprintf(rcHeader, "void rcRefreshResetInputs();\n");
  emitFuncDecl(0, "void S%s::rcRefreshResetInputs() {\n", name.c_str());
  for (SuperNode* super : rcResetSupers) {
    const int part = rcSuperPart(super);
    RcFlags guard("activeFlags" + std::to_string(part), part);
    genSuperEval(super, format("%s[%d]", gActiveFlags.c_str(), super->cppId / ACTIVE_WIDTH), 1);
  }
  emitBodyLock(0, "}\n");
}

void graph::genRepCutMemoryDefer() {
  const std::vector<Node*>& memories = repCutSnapshotMemories();
  if (memories.empty() || rcHeader == nullptr) return;
  buildRcMemFlushWakes();
  fprintf(rcHeader, "void rcFlushMem(int part);\n");
  std::map<int, std::vector<Node*>> byPart;
  for (Node* memory : memories) {
    const std::string elem = widthUType(memory->width);
    for (int part : repCutSnapshotWriterParts(memory)) {
      byPart[part].push_back(memory);
      const std::string log = "rcLog$" + memory->name + "$" + std::to_string(part);
      emitFuncDecl(0,
                   "void S%s::rcWrite$%s$%d(uint64_t elem, %s data) {\n"
                   "  for (auto& entry : %s) {\n"
                   "    if (entry.elem == elem) { entry.data = data; return; }\n"
                   "  }\n"
                   "  %s.push_back({elem, data});\n"
                   "}\n"
                   "%s S%s::rcRead$%s$%d(uint64_t elem) {\n"
                   "  for (size_t i = %s.size(); i-- > 0;) {\n"
                   "    if (%s[i].elem == elem) return %s[i].data;\n"
                   "  }\n"
                   "  return ((%s*)%s)[elem];\n"
                   "}\n",
                   name.c_str(), memory->name.c_str(), part, elem.c_str(), log.c_str(), log.c_str(),
                   elem.c_str(), name.c_str(), memory->name.c_str(), part,
                   log.c_str(), log.c_str(), log.c_str(), elem.c_str(), memory->name.c_str());
    }
  }
  emitFuncDecl(0, "void S%s::rcFlushMem(int part) {\n", name.c_str());
  emitBodyLock(1, "switch (part) {\n");
  for (const auto& group : byPart) {
    emitBodyLock(1, "case %d:\n", group.first);
    for (Node* memory : group.second) {
      const std::string elem = widthUType(memory->width);
      const std::string log = "rcLog$" + memory->name + "$" + std::to_string(group.first);
      RcMemFlushWake* wake = nullptr;
      for (RcMemFlushWake& item : rcMemFlushWakes) {
        if (item.memory == memory && item.writerPart == group.first) {
          wake = &item;
          break;
        }
      }
      if (wake == nullptr) {
        emitBodyLock(2, "for (const auto& entry : %s) ((%s*)%s)[entry.elem] = entry.data;\n",
                     log.c_str(), elem.c_str(), memory->name.c_str());
        emitBodyLock(2, "%s.clear();\n", log.c_str());
        continue;
      }
      // A writer supernode can run while its when stores nothing. Only a
      // non-empty log is a real array update, so only then wake readers.
      emitBodyLock(2, "if (unlikely(!%s.empty())) {\n", log.c_str());
      emitBodyLock(3, "for (const auto& entry : %s) ((%s*)%s)[entry.elem] = entry.data;\n",
                   log.c_str(), elem.c_str(), memory->name.c_str());
      emitBodyLock(3, "%s.clear();\n", log.c_str());
      for (RcMemWakeGroup& readers : wake->groups) {
        RcFlags guard(format("rcCommitPend%d_%d", group.first, readers.part), readers.part);
        emitActiveMask(readers.ids, 3);
      }
      emitBodyLock(2, "}\n");
    }
    emitBodyLock(2, "break;\n");
  }
  emitBodyLock(1, "default: break;\n");
  emitBodyLock(1, "}\n");
  emitBodyLock(0, "}\n");
}

void graph::genRepCutStep() {
  genRepCutResetRefresh();
  genRepCutMemoryDefer();
  const int threads = rcThreads();
  if (threads > 1 && rcHeader != nullptr) {
    fprintf(rcHeader,
            "struct RcPack {\n"
            "  S%s* self;\n"
            "  int part;\n"
            "  std::atomic<uint64_t>* epoch;\n"
            "};\n"
            "static void rcPinPart(int part);\n"
            "static void* rcWorkerMain(void* arg);\n"
            "void rcRunPart(int part);\n"
            "void rcBarrier(int part, int phase);\n"
            "void rcCycle(int part);\n"
            "struct alignas(64) RcPhaseFlag { std::atomic<uint8_t> parity{0}; };\n"
            "RcPhaseFlag rcArrive[2][%d]{};\n"
            "RcPhaseFlag rcRelease[2]{};\n"
            "struct alignas(64) RcEpoch { std::atomic<uint64_t> v; };\n"
            "RcEpoch rcEpoch{};\n"
            "struct alignas(64) RcJoin { std::atomic<uint64_t> v; };\n"
            "RcJoin rcJoin[%d]{};\n"
            "pthread_t rcThreads[%d]{};\nRcPack rcPacks[%d]{};\nbool rcStarted = false;\n"
            "~S%s();\nS%s(const S%s&) = delete;\nS%s& operator=(const S%s&) = delete;\n",
            name.c_str(), threads, threads, threads - 1, threads,
            name.c_str(), name.c_str(), name.c_str(), name.c_str(), name.c_str());
  }
  if (threads > 1) {
    emitFuncDecl(0, "void S%s::rcRunPart(int part) {\n", name.c_str());
    emitBodyLock(1, "switch (part) {\n");
    for (int part = 0; part < threads; ++part) {
      emitBodyLock(1, "case %d:\n", part);
      for (int chunk = 0; chunk < rcChunks[part]; ++chunk) emitBodyLock(2, "evalPart%d_%d();\n", part, chunk);
      emitBodyLock(2, "break;\n");
    }
    emitBodyLock(1, "default: break;\n");
    emitBodyLock(1, "}\n");
    emitBodyLock(0, "}\n");
    emitFuncDecl(0,
                 "void S%s::rcBarrier(int part, int phase) {\n"
                 "  const uint8_t parity = static_cast<uint8_t>(rcEpoch.v.load(std::memory_order_acquire) & 1);\n"
                 "  if (part == 0) {\n"
                 "    for (int i = 1; i < %d; ++i) {\n"
                 "      while (rcArrive[phase][i].parity.load(std::memory_order_acquire) != parity) {\n"
                 "        asm volatile(\"pause\" ::: \"memory\");\n"
                 "      }\n"
                 "    }\n"
                 "    rcRelease[phase].parity.store(parity, std::memory_order_release);\n"
                 "  } else {\n"
                 "    rcArrive[phase][part].parity.store(parity, std::memory_order_release);\n"
                 "    while (rcRelease[phase].parity.load(std::memory_order_acquire) != parity) {\n"
                 "      asm volatile(\"pause\" ::: \"memory\");\n"
                 "    }\n"
                 "  }\n"
                 "}\n",
                 name.c_str(), threads);
    bool rcHasAsync = false;
    for (SuperNode* reset : allReset) {
      if (reset->superType == SUPER_ASYNC_RESET && reset->resetNode->status != CONSTANT_NODE) rcHasAsync = true;
    }
    emitFuncDecl(0, "void S%s::rcCycle(int part) {\n", name.c_str());
    emitBodyLock(1, "rcCommitPart(part);\n");
    emitBodyLock(1, "rcBarrier(part, 0);\n");
    if (rcHasAsync) {
      emitBodyLock(1, "if (part == 0) {\n");
      for (SuperNode* reset : allReset) {
        if (reset->superType != SUPER_ASYNC_RESET || reset->resetNode->status == CONSTANT_NODE) continue;
        const int id = super2ResetId.at(reset->resetNode).second;
        emitBodyLock(2, "rcRefreshResetInputs();\n");
        emitBodyLock(2, "rcResetActive%d = rcPrevAsync%d || %s;\n", id, id, reset->resetNode->name.c_str());
        emitBodyLock(2, "subReset%d();\n", id);
      }
      emitBodyLock(1, "}\n");
      emitBodyLock(1, "rcBarrier(part, 1);\n");
    }
    if (rcHasCommitPend) emitBodyLock(1, "rcMergeCommitFlags(part);\n");
    if (rcHasRegChanges) emitBodyLock(1, "rcApplyRegChanges(part);\n");
    emitBodyLock(1, "rcRunPart(part);\n");
    emitBodyLock(0, "}\n");
    emitFuncDecl(0, "void S%s::rcPinPart(int part) {\n", name.c_str());
    emitBodyLock(0,
                 "  const char* affinity = getenv(\"GSIM_MT_CPU_AFFINITY\");\n"
                 "  if (affinity == nullptr || affinity[0] == '\\0') return;\n"
                 "  cpu_set_t allowed;\n"
                 "  CPU_ZERO(&allowed);\n"
                 "  if (strcmp(affinity, \"auto\") == 0) {\n"
                 "    if (sched_getaffinity(0, sizeof(allowed), &allowed) != 0) return;\n"
                 "  } else {\n"
                 "    const char* cursor = affinity;\n"
                 "    while (*cursor != '\\0') {\n"
                 "      while (*cursor == ' ' || *cursor == ',') ++cursor;\n"
                 "      if (*cursor == '\\0') break;\n"
                 "      char* end = nullptr;\n"
                 "      const long start = strtol(cursor, &end, 10);\n"
                 "      if (end == cursor) {\n"
                 "        fprintf(stderr, \"[repcut] warning: bad GSIM_MT_CPU_AFFINITY '%%s'\\n\", affinity);\n"
                 "        return;\n"
                 "      }\n"
                 "      long stop = start;\n"
                 "      cursor = end;\n"
                 "      if (*cursor == '-') {\n"
                 "        ++cursor;\n"
                 "        stop = strtol(cursor, &end, 10);\n"
                 "        if (end == cursor || stop < start) {\n"
                 "          fprintf(stderr, \"[repcut] warning: bad GSIM_MT_CPU_AFFINITY '%%s'\\n\", affinity);\n"
                 "          return;\n"
                 "        }\n"
                 "        cursor = end;\n"
                 "      }\n"
                 "      if (start < 0 || stop >= CPU_SETSIZE) {\n"
                 "        fprintf(stderr, \"[repcut] warning: CPU in GSIM_MT_CPU_AFFINITY is out of range\\n\");\n"
                 "        return;\n"
                 "      }\n"
                 "      for (long cpu = start; cpu <= stop; ++cpu) CPU_SET(static_cast<int>(cpu), &allowed);\n"
                 "    }\n"
                 "  }\n"
                 "  int selected = -1;\n"
                 "  int index = 0;\n"
                 "  for (int cpu = 0; cpu < CPU_SETSIZE; ++cpu) {\n"
                 "    if (CPU_ISSET(cpu, &allowed) && index++ == part) { selected = cpu; break; }\n"
                 "  }\n"
                 "  if (selected < 0) {\n"
                 "    fprintf(stderr, \"[repcut] warning: no CPU for part %%d\\n\", part);\n"
                 "    return;\n"
                 "  }\n"
                 "  cpu_set_t target;\n"
                 "  CPU_ZERO(&target);\n"
                 "  CPU_SET(selected, &target);\n"
                 "  if (sched_setaffinity(0, sizeof(target), &target) != 0)\n"
                 "    fprintf(stderr, \"[repcut] warning: failed to pin part %%d to cpu %%d\\n\", part, selected);\n"
                 "}\n");
    emitFuncDecl(0,
                 "void* S%s::rcWorkerMain(void* arg) {\n"
                 "  RcPack* pack = static_cast<RcPack*>(arg);\n"
                 "  S%s* self = pack->self;\n"
                 "  const int part = pack->part;\n"
                 "  rcPinPart(part);\n"
                 "  std::atomic<uint64_t>* epoch = pack->epoch;\n"
                 "  uint64_t seen = 0;\n"
                 "  for (;;) {\n"
                 "    const uint64_t cur = epoch->load(std::memory_order_acquire);\n"
                 "    if (cur == seen) {\n"
                 "      asm volatile(\"pause\" ::: \"memory\");\n"
                 "      continue;\n"
                 "    }\n"
                 "    if (cur == ~uint64_t{0}) return nullptr;\n"
                 "    seen = cur;\n"
                 "    self->rcCycle(part);\n"
                 "    self->rcJoin[part].v.store(seen, std::memory_order_release);\n"
                 "  }\n"
                 "}\n",
                 name.c_str(), name.c_str());
    emitFuncDecl(0, "S%s::~S%s() {\n", name.c_str(), name.c_str());
    emitBodyLock(1, "if (!rcStarted) return;\n");
    if (!repCutSnapshotMemories().empty()) {
      emitBodyLock(1, "for (int part = 0; part < %d; ++part) rcFlushMem(part);\n", threads);
    }
    emitBodyLock(1, "rcEpoch.v.store(~uint64_t{0}, std::memory_order_release);\n");
    emitBodyLock(1, "for (int part = 1; part < %d; ++part) pthread_join(rcThreads[part - 1], nullptr);\n", threads);
    emitBodyLock(0, "}\n");
  }
  fprintf(rcHeader, "bool rcChecked = false;\n");
  if (rcHeader != nullptr) fprintf(rcHeader, "void step();\n");
  emitFuncDecl(0, "void S%s::step() {\n", name.c_str());
  emitBodyLock(1,
               "if (!rcChecked) {\n"
               "    rcChecked = true;\n"
               "    const char* rcEnv = getenv(\"GSIM_THREADS\");\n"
               "    if (rcEnv != nullptr && rcEnv[0] != '\\0') {\n"
               "      char* rcEnd = nullptr;\n"
               "      long rcWant = strtol(rcEnv, &rcEnd, 10);\n"
               "      gAssert(rcEnd != rcEnv && *rcEnd == '\\0' && rcWant == %d, \"GSIM_THREADS=%%s but RepCut model was built for %d\", rcEnv);\n"
               "    }\n"
               "  }\n",
               threads, threads);
  for (SuperNode* reset : allReset) {
    if (reset->superType != SUPER_ASYNC_RESET || reset->resetNode->status == CONSTANT_NODE) continue;
    const int id = super2ResetId.at(reset->resetNode).second;
    emitBodyLock(1, "rcPrevAsync%d = %s;\n", id,
                 reset->resetNode->rcResetSource ? reset->resetNode->name.c_str() : "false");
  }
  emitBodyLock(1, "rcRefreshResetInputs();\n");
  emitBodyLock(1, "resetAll();\n");
  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->isReset() && member->type == NODE_REG_SRC) {
        emitBodyLock(1, "%s = %s;\n", RESET_NAME(member).c_str(), member->name.c_str());
      }
    }
  }
  if (threads == 1) {
    emitBodyLock(1, "commitCycle();\n");
    for (SuperNode* reset : allReset) {
      if (reset->superType != SUPER_ASYNC_RESET || reset->resetNode->status == CONSTANT_NODE) continue;
      const int id = super2ResetId.at(reset->resetNode).second;
      emitBodyLock(1, "rcRefreshResetInputs();\n");
      emitBodyLock(1, "rcResetActive%d = rcPrevAsync%d || %s;\n", id, id, reset->resetNode->name.c_str());
      emitBodyLock(1, "subReset%d();\n", id);
    }
    for (int chunk = 0; chunk < rcChunks[0]; ++chunk) emitBodyLock(1, "evalPart0_%d();\n", chunk);
  } else {
    emitBodyLock(1,
                 "if (!rcStarted) {\n"
                 "    rcStarted = true;\n"
                 "    for (int part = 1; part < %d; part ++) {\n"
                 "      rcPacks[part].self = this;\n"
                 "      rcPacks[part].part = part;\n"
                 "      rcPacks[part].epoch = &rcEpoch.v;\n"
                 "      gAssert(pthread_create(&rcThreads[part - 1], nullptr, rcWorkerMain, &rcPacks[part]) == 0, \"pthread_create failed\");\n"
                 "    }\n"
                 "    rcPinPart(0);\n"
                 "  }\n"
                 "  const uint64_t rcTurn = rcEpoch.v.fetch_add(1, std::memory_order_release) + 1;\n"
                 "  rcCycle(0);\n"
                 "  for (int part = 1; part < %d; ++part) {\n"
                 "    while (rcJoin[part].v.load(std::memory_order_acquire) != rcTurn) {\n"
                 "      asm volatile(\"pause\" ::: \"memory\");\n"
                 "    }\n"
                 "  }\n",
                 threads, threads);
  }
  emitBodyLock(1, "cycles ++;\n");
  emitBodyLock(0, "}\n");
}

void graph::cppEmitter() {
  gActiveFlags = "activeFlags";
  rcEmitPart = -1;
  rcEmitEval = false;
  rcHeader = nullptr;
  if (globalConfig.RepCutMode) {
    assignRepCutIds(sortedSuper);
  } else {
  for (SuperNode* super : sortedSuper) {
    if (!super->instsEmpty() || super->superType == SUPER_EXTMOD || super->superType == SUPER_ASYNC_RESET) {
      super->cppId = superId ++;
      cppId2Super[super->cppId] = super;
      if (super->superType == SUPER_EXTMOD) {
        alwaysActive.insert(super->cppId);
      }
#if 0
      if (super->member.size() == 1) {
        alwaysActive.insert(super->cppId);
        printf("alwaysActive %d\n", super->cppId);
      }
#endif
    }
  }
  activeFlagNum = (superId + ACTIVE_WIDTH - 1) / ACTIVE_WIDTH;
  // avoid buffer overflow when accessing the last elements as uint64_t
  activeFlagNum = ROUNDUP(activeFlagNum, 8);
  }

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE) {
        if (globalConfig.RepCutMode) repCutActivate(member);
        else {
          member->updateActivate();
          member->updateNeedActivate(alwaysActive);
        }
      }
    }
  }

  srcFp = NULL;
  srcFileIdx = 0;

  FILE* header = genHeaderStart();
#ifdef DIFFTEST_PER_SIG
  sigFile = fopen((globalConfig.OutputDir + "/" + name + "_sigs.txt").c_str(), "w");
#endif

  /* class start*/
  fprintf(header, "class S%s {\npublic:\n", name.c_str());
  fprintf(header, "uint64_t cycles;\n");
  fprintf(header, "uint64_t LOG_START, LOG_END;\n");
  if (globalConfig.RepCutMode) {
    const int threads = rcThreads();
    for (int part = 0; part < threads; ++part) {
      fprintf(header, "alignas(64) uint%d_t activeFlags%d[%d];\n", ACTIVE_WIDTH, part, rcFlagWords[part]);
    }
  } else {
    fprintf(header, "uint%d_t activeFlags[%d];\n", ACTIVE_WIDTH, activeFlagNum); // or super.size() if id == idx
  }
#ifdef PERF
  fprintf(header, "size_t activeTimes[%d];\n", superId);
#if ENABLE_ACTIVATOR
  fprintf(header, "std::map<int, int>activator[%d];\n", superId);
#endif
  fprintf(header, "size_t validActive[%d];\n", superId);
  fprintf(header, "size_t nodeNum[%d];\n", superId);
#endif
  emitPrintf();
  /* constrcutor */
  emitFuncDecl(0, "S%s::S%s() {\n"
               "  cycles = 0;\n"
               "  LOG_START = 1;\n"
               "  LOG_END = 0;\n"
               "  init();\n"
               "}\n", name.c_str(), name.c_str());

  /* initialization */
  emitFuncDecl(0, "void S%s::init() {\n", name.c_str());
  const std::string clearStateBytes = globalConfig.RepCutMode
      ? "offsetof(S" + name + ", _var_end) - offsetof(S" + name + ", _var_start)"
      : "&_var_end - &_var_start";
  emitBodyLock(1, "activateAll();\n");
#ifdef PERF
  emitBodyLock(1, "for (int i = 0; i < %d; i ++) activeTimes[i] = 0;\n", superId);
  #if ENABLE_ACTIVATOR
  emitBodyLock(1, "for (int i = 0; i < %d; i ++) activator[i] = std::map<int, int>();\n", superId);
  #endif
  for (SuperNode* super : sortedSuper) {
    if (super->cppId >= 0) {
      size_t num = 0;
      for (Node* member : super->member) {
        if (member->anyNextActive()) num ++;
      }
      emitBodyLock(1, "nodeNum[%d] = %ld; // memberNum=%ld\n", super->cppId, num, super->member.size());
    }
  }
  emitBodyLock(1, "for (int i = 0; i < %d; i ++) validActive[i] = 0;\n", superId);
#endif
  emitBodyLock(0, "#ifdef RANDOMIZE_INIT\n"
               "  srand((unsigned int)time(NULL));\n"
               "  for (uint32_t *p = &_var_start; p != &_var_end; p ++) {\n"
               "    *p = rand();\n"
               "  }\n"
               "// mask out the bits out of the width range\n");

  // header: node definition; src: node evaluation
  fprintf(header, "uint32_t _var_start;\n");
  auto isStateNode = [](Node* node) {
    return node != nullptr && (node->type == NODE_REG_SRC || node->type == NODE_REG_DST);
  };
  if (globalConfig.RepCutMode) {
    const int threads = rcThreads();
    for (int part = 0; part < threads; ++part) {
      for (SuperNode* super : sortedSuper) {
        if (rcSuperPart(super) != part) continue;
        const std::vector<Node*> bulk = rcBulkCommitNodes(super);
        if (bulk.empty()) continue;
        for (Node* node : bulk) {
          Assert(definedNode.insert(node).second, "bulk register declared twice: %s", node->name.c_str());
          fprintf(header, "uint64_t %s;\n", node->name.c_str());
        }
        for (Node* node : bulk) {
          Node* next = node->getDst();
          Assert(definedNode.insert(next).second, "bulk next-state declared twice: %s", next->name.c_str());
          fprintf(header, "uint64_t %s;\n", next->name.c_str());
        }
      }
    }
  }
  if (globalConfig.RepCutMode) {
    const int threads = rcThreads();
    for (int part = 0; part < threads; ++part) {
      for (SuperNode* super : sortedSuper) {
        if (rcSuperPart(super) != part) continue;
        if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET)
          for (Node* n : super->member) if (isStateNode(n)) genNodeDef(header, n);
      }
    }
    for (SuperNode* super : sortedSuper) {
      if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET)
        for (Node* n : super->member) if (!isStateNode(n)) genNodeDef(header, n);
      if (super->superType == SUPER_EXTMOD)
        for (size_t i = 1; i < super->member.size(); i ++) genNodeDef(header, super->member[i]);
    }
  } else {
    for (SuperNode* super : sortedSuper) {
      if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET)
        for (Node* n : super->member) genNodeDef(header, n);
      if (super->superType == SUPER_EXTMOD)
        for (size_t i = 1; i < super->member.size(); i ++) genNodeDef(header, super->member[i]);
    }
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDef(header, mem);
  fprintf(header, "uint32_t _var_end;\n");

  emitBodyLock(0, "// initialize registers with reset value 0 to overwrite the rand() results\n" );
  emitBodyLock(1, "memset(&_var_start, 0, %s);\n", clearStateBytes.c_str());

  emitBodyLock(0, "#else\n" // RANDOMIZE_INIT
               "  memset(&_var_start, 0, %s);\n"
               "#endif\n", clearStateBytes.c_str());

  fprintf(header, "S%s();\n", name.c_str());
  fprintf(header, "void init();\n");

  emitBodyLock(0, "}\n");

  /* activation all nodes for reset */
  fprintf(header, "void activateAll();\n");
  if (globalConfig.RepCutMode) {
    const int threads = rcThreads();
    emitFuncDecl(0, "void S%s::activateAll() {\n", name.c_str());
    for (int part = 0; part < threads; ++part) {
      emitBodyLock(1, "memset(activeFlags%d, 0xff, sizeof(activeFlags%d));\n", part, part);
    }
    emitBodyLock(0, "}\n");
  } else {
    emitFuncDecl(0, "void S%s::activateAll() {\n"
                 "  memset(activeFlags, 0xff, sizeof(activeFlags));\n"
                 "}\n", name.c_str());
  }

   /* input/output interface */
  for (Node* node : input) {
    fprintf(header, "void set_%s(%s val);\n", node->name.c_str(), widthUType(node->width).c_str());
    genInterfaceInput(node);
  }
  for (Node* node : output) {
    fprintf(header, "%s get_%s();\n", widthUType(node->width).c_str(), node->name.c_str());
    genInterfaceOutput(node);
  }

  /* reset functions */
  fprintf(header, "void resetAll();\n");
  genResetAll();
  for (int i = 0; i < resetFuncNum; i ++) {
    fprintf(header, "void subReset%d();\n", i);
    if (globalConfig.RepCutMode) fprintf(header, "bool rcResetActive%d = false;\nbool rcPrevAsync%d = false;\n", i, i);
  }

  /* main evaluation loop (step) */
  if (globalConfig.RepCutMode) {
    rcHeader = header;
    const int threads = rcThreads();
    for (int part = 0; part < threads; ++part) genRepCutEval(part);
    genRepCutCommit();
    genRepCutStep();
    rcHeader = nullptr;
  } else {
    int subStepIdxMax = genActivate();
    for (int i = 0; i <= subStepIdxMax; i ++) {
      fprintf(header, "void subStep%d();\n", i);
    }
    fprintf(header, "void step();\n");
    genStep(subStepIdxMax);
  }

  /* end of file */
  fprintf(header, "};\n"
                  "#endif\n");
  fclose(header);
  fclose(srcFp);
#ifdef DIFFTEST_PER_SIG
  fclose(sigFile);
#endif

  printf("[cppEmitter] define %ld nodes %d superNodes\n", definedNode.size(), superId);
  std::cout << "[cppEmitter] finish writing " << srcFileIdx << " cpp files to " + globalConfig.OutputDir + "/" << std::endl;
}
