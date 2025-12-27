/*
  cppEmitter: emit C++ files for simulation
*/

#include "common.h"
#include "util.h"

#include <cstddef>
#include <cstdio>
#include <map>
#include <string>
#include <utility>

#define ACTIVE_WIDTH 8
#define RESET_PER_FUNC 400

#define ENABLE_ACTIVATOR false

// Avoid emitting waveform handles for giant arrays (e.g., full RAM contents);
// limit is controlled by globalConfig.FstMaxArrayElems (0 = unlimited).

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

static std::map<Node*, std::pair<int, int>> super2ResetId;  // uint & async reset
static std::vector<Node*> fstWaveNodes;
static std::set<Node*> fstWaveNodeSet;

extern int maxConcatNum;
bool nameExist(std::string str);
static int resetFuncNum = 0;

static bool isAlwaysActive(int cppId) {
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
  if (mask <= MAX_U8) return format("activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U16) return format("*(uint16_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  if (mask <= MAX_U32) return format("*(uint32_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
  return format("*(uint64_t*)&activeFlags[%d] |= 0x%lx;", idx, mask);
}

std::string updateActiveStr(int idx, uint64_t mask, std::string& cond, int uniqueId) {
  auto activeFlags = std::string("activeFlags[") + std::to_string(idx) + std::string("]");

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

static bool isTopField(Node* node) {
  if (node->type == NODE_SPECIAL || node->type == NODE_REG_RESET || node->status != VALID_NODE) return false;
  if (node->type == NODE_REG_DST && !node->regSplit) return false;
  if (node->type == NODE_WRITER) return false;
  return !node->isLocal();
}

static std::vector<int> nodeArrayDims(Node* node) {
  std::vector<int> dims;
  if (node->type == NODE_MEMORY) dims.push_back(upperPower2(node->depth));
  for (int dim : node->dimension) dims.push_back(upperPower2(dim));
  return dims;
}

static std::vector<std::string> splitBySeparators(const std::string& name) {
  // Honor user-provided module/aggregate separators (default: "$"); fall back to
  // the original dollar split if no separator matches.
  std::vector<std::string> seps;
  if (!globalConfig.sep_module.empty()) seps.push_back(globalConfig.sep_module);
  if (!globalConfig.sep_aggr.empty() && globalConfig.sep_aggr != globalConfig.sep_module) {
    seps.push_back(globalConfig.sep_aggr);
  }
  std::vector<std::string> parts;
  size_t pos = 0;
  while (pos < name.size()) {
    size_t next = std::string::npos;
    size_t sepLen = 0;
    for (const auto& sep : seps) {
      size_t found = name.find(sep, pos);
      if (found != std::string::npos && (next == std::string::npos || found < next)) {
        next = found;
        sepLen = sep.size();
      }
    }
    if (next == std::string::npos) break;
    if (next > pos) parts.push_back(name.substr(pos, next - pos));
    pos = next + sepLen;
  }
  if (pos < name.size()) {
    parts.push_back(name.substr(pos));
  }
  if (parts.empty() && name.find('$') != std::string::npos) {
    // Fallback: legacy dollar separator.
    std::string cur;
    for (char c : name) {
      if (c == '$') {
        if (!cur.empty()) {
          parts.push_back(cur);
          cur.clear();
        }
      } else {
        cur.push_back(c);
      }
    }
    if (!cur.empty()) parts.push_back(cur);
  }
  if (parts.empty()) parts.push_back(name);
  return parts;
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
  includeLib(header, "string", true);
  includeLib(header, "map", true);
  includeLib(header, "cstdarg", true);
  newLine(header);

  fprintf(header, "\n// User configuration\n");
  fprintf(header, "//#define ENABLE_LOG\n");
  fprintf(header, "//#define RANDOMIZE_INIT\n");

  includeLib(header, "gsimFst.h", false);
  newLine(header);

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

void graph::genInterfaceInput(Node* input) {
  /* set by string */
  emitFuncDecl(0, "void S%s::set_%s(%s val) {\n", name.c_str(), input->name.c_str(), widthUType(input->width).c_str());
  emitBodyLock(1, "if (%s != val) { \n", input->name.c_str());
  emitBodyLock(2, "%s = val;\n", input->name.c_str());
  /* update nodes in the same superNode */
  std::set<int> allNext;
  for (Node* next : input->next) {
    if (next->super->cppId >= 0) allNext.insert(next->super->cppId);
  }
  std::map<uint64_t, ActiveType> bitMapInfo;
  activeSet2bitMap(allNext, bitMapInfo, -1);
  for (auto iter : bitMapInfo) {
    emitBodyLock(2, "%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
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
  if (!isTopField(node)) return;
#if defined(GSIM_DIFF) || defined(VERILATOR_DIFF)
  genDiffSig(fp, node);
#endif
  if (definedNode.find(node) != definedNode.end()) return;
  definedNode.insert(node);
  fprintf(fp, "%s %s", widthUType(node->width).c_str(), node->name.c_str());
  if (node->type == NODE_MEMORY) fprintf(fp, "[%d]", upperPower2(node->depth));
  for (int dim : node->dimension) fprintf(fp, "[%d]", upperPower2(dim));
  fprintf(fp, "; // width = %d, lineno = %d\n", node->width, node->lineno);
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

void graph::activateNext(Node* node, std::set<int>& nextNodeId, std::string oldName, bool inStep, std::string flagName, int indent) {
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
    emitBodyLock(indent ++, "if(unlikely(%s & 0x%lx)) { // id=%d\n", flagName.c_str(), mask, idx);
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

bool Node::isLocal() { // TODO: isArray is OK
  return status == VALID_NODE && type == NODE_OTHERS && !anyNextActive() && !isArray() && !isReset();
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
      {
        bool skipActivate = inst.node->isLocal() || !inst.node->needActivate();
        if (!skipActivate) {
          if (inst.node->isArray() || inst.node->type == NODE_WRITER) activateUncondNext(inst.node, inst.node->nextActiveId, false, flagName, indent);
          else activateNext(inst.node, inst.node->nextActiveId, oldName(inst.node), false, flagName, indent);
        }
        if (globalConfig.TraceFst && fstWaveNodeSet.count(inst.node)) {
          emitBodyLock(indent, "if (dumpWaveformFlag && fstCtx) {\n");
          std::vector<int> dims = nodeArrayDims(inst.node);
          if (dims.empty()) {
            emitBodyLock(indent + 1, "updateFstSignal(fstHandle_%s, &%s, %d);\n", inst.node->name.c_str(), inst.node->name.c_str(), inst.node->width);
          } else {
            int curIndent = indent + 1;
            for (size_t i = 0; i < dims.size(); i ++) {
              emitBodyLock(curIndent ++, "for (int i%zu = 0; i%zu < %d; i%zu ++) {\n", i, i, dims[i], i);
            }
            std::string accessStr = inst.node->name;
            emitBodyLock(curIndent, "size_t handle_idx = 0;\n");
            for (size_t i = 0; i < dims.size(); i ++) {
              accessStr += format("[i%zu]", i);
              emitBodyLock(curIndent, "handle_idx = handle_idx * %d + i%zu;\n", dims[i], i);
            }
            emitBodyLock(curIndent, "updateFstSignal(fstHandle_%s[handle_idx], &%s, %d);\n", inst.node->name.c_str(), accessStr.c_str(), inst.node->width);
            for (size_t i = 0; i < dims.size(); i ++) {
              emitBodyLock(-- curIndent, "}\n");
            }
          }
          emitBodyLock(indent, "}\n");
        }
        if (skipActivate) break;
      }
      break;
    default:
      break;
  }
  return indent;
}

void graph::genSuperEval(SuperNode* super, std::string flagName, int indent) { // current indent = 2
  if (super->superType == SUPER_EXTMOD) { // TODO: normalize
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
      if (!super->member[i]->needActivate()) continue;
      if (super->member[i]->isArray()) activateUncondNext(super->member[i], super->member[i]->nextActiveId, false, flagName, indent);
      else activateNext(super->member[i], super->member[i]->nextActiveId, oldName(super->member[i]), false, flagName, indent);
    }
  } else {
    if (super->superType == SUPER_ASYNC_RESET) {
      emitBodyLock(indent, "subReset%d();\n", super2ResetId[super->resetNode].second);
    }
    /* local nodes definition */
    for (Node* n : super->member) {
      if (n->isLocal()) {
        emitBodyLock(indent, "%s %s;\n", widthUType(n->width).c_str(), n->name.c_str());
      }
    }
    for (InstInfo inst : super->insts) {
      indent = translateInst(inst, indent, flagName);
    }
    if (super->superType == SUPER_ASYNC_RESET) emitBodyLock(indent, "subReset%d();\n", super2ResetId[super->resetNode].second);
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

void graph::genResetDef(SuperNode* super, bool isUIntReset, int indent) {
  emitBodyLock(indent ++, "void S%s::subReset%d(){ // %s reset\n", name.c_str(), resetFuncNum, isUIntReset ? "uint" : "async");
  if (super2ResetId.find(super->resetNode) != super2ResetId.end()) {
    super2ResetId[super->resetNode] = std::make_pair(-1, -1);
  }
  if (isUIntReset) super2ResetId[super->resetNode].first = resetFuncNum;
  else super2ResetId[super->resetNode].second = resetFuncNum;
  resetFuncNum ++;
  std::string resetName = super->resetNode->type == NODE_REG_SRC ? RESET_NAME(super->resetNode).c_str() : super->resetNode->name.c_str();
  emitBodyLock(indent ++, "if(unlikely(%s)) {\n", resetName.c_str());
  std::set<int> allNext;
  for (size_t i = 0; i < super->member.size(); i ++) {
    Node* node = super->member[i];
    if (node->type == NODE_REG_RESET) node = node->getResetSrc();
    for (Node* next : node->next) {
      if (next->super->cppId >= 0) allNext.insert(next->super->cppId);
    }
  }

  if (allNext.size() > 100) emitBodyLock(indent, "activateAll();\n");
  else {
    std::map<uint64_t, ActiveType> bitMapInfo;
    activeSet2bitMap(allNext, bitMapInfo, -1);
    for (auto iter : bitMapInfo) {
      emitBodyLock(indent, "%s // %s\n", updateActiveStr(iter.first, ACTIVE_MASK(iter.second)).c_str(), ACTIVE_COMMENT(iter.second).c_str());
    }
  }
  emitBodyLock(-- indent, "}\n");
  for (InstInfo inst : super->insts) {
    switch (inst.infoType) {
      case SUPER_INFO_IF:
        emitBodyLock(indent ++, "%s\n", inst.inst.c_str());
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
  emitBodyLock(1, "dumpWaveformFlag = waveformEnabled;\n");
  emitBodyLock(1, "if (dumpWaveformFlag && !fstDumpInitialized) {\n");
  std::vector<std::string> fstScopeStack;
  fstScopeStack.push_back("gsim");
  fstScopeStack.push_back("gsim_top");
  emitBodyLock(2, "resetFstHandles();\n");
  emitBodyLock(2, "if (fstCtx) { fstWriterClose(fstCtx); fstCtx = nullptr; }\n");
  emitBodyLock(2, "fstCtx = fstWriterCreate(fstPath.c_str(), 1);\n");
  emitBodyLock(2, "fstWriterSetPackType(fstCtx, FST_WR_PT_LZ4);\n");  // favor faster compression for value blocks
  emitBodyLock(2, "fstWriterSetTimescaleFromString(fstCtx, \"1ns\");\n");
  emitBodyLock(2, "fstWriterSetVersion(fstCtx, \"gsim\");\n");
  emitBodyLock(2, "fstWriterSetFileType(fstCtx, FST_FT_VERILOG);\n");
  emitBodyLock(2, "fstCycleBase = cycles;\n");
  emitBodyLock(2, "fstWriterSetScope(fstCtx, FST_ST_VCD_MODULE, \"gsim\", nullptr);\n");
  emitBodyLock(2, "fstWriterSetScope(fstCtx, FST_ST_VCD_MODULE, \"gsim_top\", nullptr);\n");
  for (Node* node : fstWaveNodes) {
    std::vector<std::string> pathComponents = splitBySeparators(node->name);
    pathComponents.insert(pathComponents.begin(), "gsim_top");
    pathComponents.insert(pathComponents.begin(), "gsim");
    if (pathComponents.empty()) {
      pathComponents.push_back(node->name);
    }
    std::string fstName = pathComponents.back();
    pathComponents.pop_back();

    size_t commonPrefix = 0;
    while (commonPrefix < fstScopeStack.size() && commonPrefix < pathComponents.size() && fstScopeStack[commonPrefix] == pathComponents[commonPrefix]) {
      commonPrefix ++;
    }
    for (size_t idx = fstScopeStack.size(); idx > commonPrefix; idx --) {
      emitBodyLock(2, "fstWriterSetUpscope(fstCtx);\n");
    }
    fstScopeStack.resize(commonPrefix);
    for (size_t idx = commonPrefix; idx < pathComponents.size(); idx ++) {
      emitBodyLock(2, "fstWriterSetScope(fstCtx, FST_ST_VCD_MODULE, \"%s\", nullptr);\n", pathComponents[idx].c_str());
      fstScopeStack.push_back(pathComponents[idx]);
    }

    std::string typestr;
    switch (node->type) {
      case NODE_REG_SRC:
      case NODE_REG_DST:
      case NODE_MEMORY:
        typestr = "FST_VT_VCD_REG";
        break;
      default:
        typestr = "FST_VT_VCD_WIRE";
        break;
    }

    std::vector<int> dims = nodeArrayDims(node);
    if (dims.empty()) {
      emitBodyLock(2, "fstHandle_%s = fstWriterCreateVar(fstCtx, %s, FST_VD_IMPLICIT, %d, \"%s\", 0u);\n",
                   node->name.c_str(), typestr.c_str(), node->width, fstName.c_str());
    } else {
      int current_indent = 2;
      int total_elems = 1;
      for (int d : dims) total_elems *= d;
      emitBodyLock(current_indent, "fstHandle_%s.resize(%d);\n", node->name.c_str(), total_elems);
      emitBodyLock(current_indent, "size_t fstHandleIdx_%s = 0;\n", node->name.c_str());
      for (size_t i = 0; i < dims.size(); i ++) {
        emitBodyLock(current_indent ++, "for (int i%zu = 0; i%zu < %d; i%zu ++) {\n", i, i, dims[i], i);
      }
      emitBodyLock(current_indent, "std::string fst_name = \"%s\";\n", fstName.c_str());
      for (size_t i = 0; i < dims.size(); i ++) {
        emitBodyLock(current_indent, "fst_name += \"[\" + std::to_string(i%zu) + \"]\";\n", i);
      }
      emitBodyLock(current_indent, "fstHandle_%s[fstHandleIdx_%s ++] = fstWriterCreateVar(fstCtx, %s, FST_VD_IMPLICIT, %d, fst_name.c_str(), 0u);\n",
                   node->name.c_str(), node->name.c_str(), typestr.c_str(), node->width);
      for (size_t i = 0; i < dims.size(); i ++) {
        emitBodyLock(-- current_indent, "}\n");
      }
    }
  }
  for (size_t idx = fstScopeStack.size(); idx > 0; idx --) {
    emitBodyLock(2, "fstWriterSetUpscope(fstCtx);\n");
  }
  emitBodyLock(2, "fstWriterSetScope(fstCtx, FST_ST_VCD_MODULE, \"gsim\", nullptr);\n");
  emitBodyLock(2, "fstWriterSetScope(fstCtx, FST_ST_VCD_MODULE, \"gsim_meta\", nullptr);\n");
  emitBodyLock(2, "fstHandle_gsim_cycle = fstWriterCreateVar(fstCtx, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, 64, \"gsim_cycle\", 0u);\n");
  emitBodyLock(2, "fstWriterSetUpscope(fstCtx);\n");
  emitBodyLock(2, "fstWriterSetUpscope(fstCtx);\n");
  emitBodyLock(2, "uint64_t fst_rel_cycle = cycles - fstCycleBase;\n");
  emitBodyLock(2, "fstWriterEmitTimeChange(fstCtx, fst_rel_cycle);\n");
  emitBodyLock(2, "emitAllSignalValues();\n");
  emitBodyLock(2, "fstDumpInitialized = true;\n");
  emitBodyLock(1, "}\n");
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
  emitBodyLock(1, "if (dumpWaveformFlag && fstCtx) {\n");
  emitBodyLock(2, "uint64_t fst_rel_cycle = cycles - fstCycleBase;\n");
  emitBodyLock(2, "updateFstSignal(fstHandle_gsim_cycle, &fst_rel_cycle, 64);\n");
  emitBodyLock(2, "fstWriterEmitTimeChange(fstCtx, fst_rel_cycle);\n");
  emitBodyLock(1, "}\n");
  emitBodyLock(0, "}\n");
}

bool SuperNode::instsEmpty() {
  return insts.size() == 0;
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

void graph::cppEmitter() {
  definedNode.clear();
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

  fstWaveNodes.clear();
  fstWaveNodeSet.clear();
  const size_t fstWaveLimit = globalConfig.FstMaxArrayElems;
  auto collectWaveNode = [&](Node* n) {
    if (!isTopField(n)) return;
    if (globalConfig.TraceFstNoNext) {
      const std::string& nm = n->name;
      if (nm.size() >= 5 && nm.rfind("$NEXT") == nm.size() - 5) return;
    }
    auto dims = nodeArrayDims(n);
    if (!dims.empty() && fstWaveLimit != 0) {
      size_t total = 1;
      for (int dim : dims) {
        if (dim <= 0) continue;
        if (total > fstWaveLimit / static_cast<size_t>(dim)) {
          total = fstWaveLimit + 1;
          break;
        }
        total *= static_cast<size_t>(dim);
      }
      if (total > fstWaveLimit) {
        fprintf(stderr, "[gsim] skip waveform for %s: %zu elements exceed limit %zu\n",
                n->name.c_str(), total, fstWaveLimit);
        return;
      }
    }
    fstWaveNodeSet.insert(n);
  };
  for (SuperNode* super : sortedSuper) {
    if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
      for (Node* n : super->member) collectWaveNode(n);
    }
    if (super->superType == SUPER_EXTMOD) {
      for (size_t i = 1; i < super->member.size(); i ++) collectWaveNode(super->member[i]);
    }
  }
  for (Node* mem : memory) collectWaveNode(mem);
  fstWaveNodes.assign(fstWaveNodeSet.begin(), fstWaveNodeSet.end());
  std::sort(fstWaveNodes.begin(), fstWaveNodes.end(), [](Node* a, Node* b) {
    return a->name < b->name;
  });

  activeFlagNum = (superId + ACTIVE_WIDTH - 1) / ACTIVE_WIDTH;
  // avoid buffer overflow when accessing the last elements as uint64_t
  activeFlagNum = ROUNDUP(activeFlagNum, 8);

  for (SuperNode* super : sortedSuper) {
    for (Node* member : super->member) {
      if (member->status == VALID_NODE) {
        member->updateActivate();
        member->updateNeedActivate(alwaysActive);
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
  fprintf(header, "uint64_t fstCycleBase;\n");
  fprintf(header, "uint%d_t activeFlags[%d];\n", ACTIVE_WIDTH, activeFlagNum); // or super.size() if id == idx
  fprintf(header, "bool waveformEnabled;\n");
  fprintf(header, "void *fstCtx;\n");
  fprintf(header, "fstHandle fstHandle_gsim_cycle;\n");
  for (Node* node : fstWaveNodes) {
    if (nodeArrayDims(node).empty()) {
      fprintf(header, "fstHandle fstHandle_%s;\n", node->name.c_str());
    } else {
      fprintf(header, "std::vector<fstHandle> fstHandle_%s;\n", node->name.c_str());
    }
  }
  fprintf(header, "std::string fstPath;\n");
  fprintf(header, "bool dumpWaveformFlag;\n");
  fprintf(header, "bool fstDumpInitialized;\n");
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
               "  fstCycleBase = 0;\n"
               "  waveformEnabled = false;\n"
               "  fstCtx = nullptr;\n"
               "  fstHandle_gsim_cycle = 0;\n"
               "  fstPath = \"%s.fst\";\n"
               "  dumpWaveformFlag = false;\n"
               "  fstDumpInitialized = false;\n"
               "  resetFstHandles();\n"
               "  init();\n"
               "}\n", name.c_str(), name.c_str(), name.c_str());

  emitFuncDecl(0, "S%s::~S%s() {\n", name.c_str(), name.c_str());
  emitBodyLock(1, "if (this->fstCtx) {\n");
  emitBodyLock(2, "fstWriterFlushContext(this->fstCtx);\n");
  emitBodyLock(2, "fstWriterClose(this->fstCtx);\n");
  emitBodyLock(1, "}\n");
  emitBodyLock(0, "}\n");

  emitFuncDecl(0, "void S%s::resetFstHandles() {\n", name.c_str());
  emitBodyLock(1, "fstHandle_gsim_cycle = 0;\n");
  for (Node* node : fstWaveNodes) {
    if (nodeArrayDims(node).empty()) {
      emitBodyLock(1, "fstHandle_%s = 0;\n", node->name.c_str());
    } else {
      emitBodyLock(1, "fstHandle_%s.clear();\n", node->name.c_str());
    }
  }
  emitBodyLock(0, "}\n");

  /* initialization */
  emitFuncDecl(0, "void S%s::init() {\n", name.c_str());
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
  for (SuperNode* super : sortedSuper) {
    // std::string insts;
    if (super->superType == SUPER_VALID || super->superType == SUPER_ASYNC_RESET) {
      for (Node* n : super->member) genNodeDef(header, n);
    }
    if (super->superType == SUPER_EXTMOD) {
      for (size_t i = 1; i < super->member.size(); i ++) genNodeDef(header, super->member[i]);
    }
  }
  /* memory definition */
  for (Node* mem : memory) genNodeDef(header, mem);
  fprintf(header, "uint32_t _var_end;\n");

  emitBodyLock(0, "// initialize registers with reset value 0 to overwrite the rand() results\n" );
  emitBodyLock(1, "memset(&_var_start, 0, &_var_end - &_var_start);\n");

  emitBodyLock(0, "#else\n" // RANDOMIZE_INIT
               "  memset(&_var_start, 0, &_var_end - &_var_start);\n"
               "#endif\n");

  emitBodyLock(1, "waveformEnabled = false;\n");
  emitBodyLock(1, "fstDumpInitialized = false;\n");
  emitBodyLock(1, "dumpWaveformFlag = false;\n");
  emitBodyLock(1, "fstCycleBase = 0;\n");
  emitBodyLock(1, "resetFstHandles();\n");
  emitBodyLock(1, "if (fstCtx) { fstWriterClose(fstCtx); fstCtx = nullptr; }\n");

  fprintf(header, "S%s();\n", name.c_str());
  fprintf(header, "~S%s();\n", name.c_str());
  fprintf(header, "void init();\n");
  fprintf(header, "void emitAllSignalValues();\n");
  fprintf(header, "void setWaveformPath(const std::string& path);\n");
  fprintf(header, "void enableWaveform();\n");
  fprintf(header, "void disableWaveform();\n");
  fprintf(header, "void flushWaveform();\n");
  fprintf(header, "void resetFstHandles();\n");

  emitBodyLock(0, "}\n");

  /* activation all nodes for reset */
  fprintf(header, "void activateAll();\n");
  emitFuncDecl(0, "void S%s::activateAll() {\n"
               "  memset(activeFlags, 0xff, sizeof(activeFlags));\n"
               "}\n", name.c_str());

  emitFuncDecl(0, "void S%s::setWaveformPath(const std::string& path) {\n", name.c_str());
  emitBodyLock(1, "fstPath = path;\n");
  emitBodyLock(1, "dumpWaveformFlag = false;\n");
  emitBodyLock(1, "fstDumpInitialized = false;\n");
  emitBodyLock(1, "resetFstHandles();\n");
  emitBodyLock(1, "if (fstCtx) { fstWriterClose(fstCtx); fstCtx = nullptr; }\n");
  emitBodyLock(0, "}\n");

  emitFuncDecl(0, "void S%s::enableWaveform() {\n", name.c_str());
  emitBodyLock(1, "waveformEnabled = true;\n");
  emitBodyLock(1, "dumpWaveformFlag = dumpWaveformFlag || waveformEnabled;\n");
  emitBodyLock(0, "}\n");

  emitFuncDecl(0, "void S%s::disableWaveform() {\n", name.c_str());
  emitBodyLock(1, "waveformEnabled = false;\n");
  emitBodyLock(1, "dumpWaveformFlag = false;\n");
  emitBodyLock(0, "}\n");

  emitFuncDecl(0, "void S%s::flushWaveform() {\n", name.c_str());
  emitBodyLock(1, "if (fstCtx) fstWriterFlushContext(fstCtx);\n");
  emitBodyLock(0, "}\n");

  fprintf(header, "template<typename T>\n");
  fprintf(header, "inline void updateFstSignal(fstHandle handle, T* value_ptr, uint32_t width) {\n"
                  "  if (!dumpWaveformFlag || !fstCtx || handle == 0) return;\n"
                  "  if (width <= 32) {\n"
                  "    uint32_t val = 0;\n"
                  "    std::memcpy(&val, value_ptr, sizeof(T) < sizeof(val) ? sizeof(T) : sizeof(val));\n"
                  "    if (width < 32) {\n"
                  "      if (width == 0) val = 0;\n"
                  "      else val &= ((uint32_t(1) << width) - 1);\n"
                  "    }\n"
                  "    fstWriterEmitValueChange32(this->fstCtx, handle, width, val);\n"
                  "  } else if (width <= 64) {\n"
                  "    uint64_t val = 0;\n"
                  "    std::memcpy(&val, value_ptr, sizeof(T) < sizeof(val) ? sizeof(T) : sizeof(val));\n"
                  "    if (width < 64) {\n"
                  "      if (width == 0) val = 0;\n"
                  "      else val &= ((uint64_t(1) << width) - 1);\n"
                  "    }\n"
                  "    fstWriterEmitValueChange64(this->fstCtx, handle, width, val);\n"
                  "  } else {\n"
                  "    fstWriterEmitValueChangeVec64(this->fstCtx, handle, width, (const uint64_t*)value_ptr);\n"
                  "  }\n"
                  "}\n");

  emitFuncDecl(0, "void S%s::emitAllSignalValues() {\n", name.c_str());
  emitBodyLock(1, "if (!dumpWaveformFlag || !fstCtx) return;\n");
  emitBodyLock(1, "uint64_t fst_rel_cycle = cycles - fstCycleBase;\n");
  emitBodyLock(1, "updateFstSignal(fstHandle_gsim_cycle, &fst_rel_cycle, 64);\n");
  for (Node* node : fstWaveNodes) {
    std::vector<int> dims = nodeArrayDims(node);
    if (dims.empty()) {
      emitBodyLock(2, "updateFstSignal(fstHandle_%s, &%s, %d);\n", node->name.c_str(), node->name.c_str(), node->width);
    } else {
      int indent = 2;
      for (size_t i = 0; i < dims.size(); i ++) {
        emitBodyLock(indent ++, "for (int i%zu = 0; i%zu < %d; i%zu ++) {\n", i, i, dims[i], i);
      }
      std::string accessStr = node->name;
      emitBodyLock(indent, "size_t handle_idx = 0;\n");
      for (size_t i = 0; i < dims.size(); i ++) {
        accessStr += format("[i%zu]", i);
        emitBodyLock(indent, "handle_idx = handle_idx * %d + i%zu;\n", dims[i], i);
      }
      emitBodyLock(indent, "updateFstSignal(fstHandle_%s[handle_idx], &%s, %d);\n", node->name.c_str(), accessStr.c_str(), node->width);
      for (size_t i = 0; i < dims.size(); i ++) {
        emitBodyLock(-- indent, "}\n");
      }
    }
  }
  emitBodyLock(0, "}\n");

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
  }

  /* main evaluation loop (step) */
  int subStepIdxMax = genActivate();
  for (int i = 0; i <= subStepIdxMax; i ++) {
    fprintf(header, "void subStep%d();\n", i);
  }

  /* step wrapper */
  fprintf(header, "void step();\n");
  genStep(subStepIdxMax);

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
